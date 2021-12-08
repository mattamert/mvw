#include "d3d12/D3D12Renderer.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"
#include "d3d12/ResourceHelper.h"

using Microsoft::WRL::ComPtr;

namespace {
void EnableDebugLayer() {
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    debugController->EnableDebugLayer();
}

ComPtr<IDXGIAdapter> FindAdapter(IDXGIFactory4* factory) {
  for (UINT adapterIndex = 0;; ++adapterIndex) {
    ComPtr<IDXGIAdapter> adapter;
    if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters(0, &adapter)) {
      break;
    }

    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
      return adapter;
    }
  }

  return nullptr;
}

}  // namespace

void D3D12Renderer::Initialize(HWND hwnd) {
  EnableDebugLayer();

  InitializePerDeviceObjects();
  InitializePerWindowObjects(hwnd);
  InitializePerPassObjects();
  InitializeFenceObjects();
  InitializeShadowMapObjects();
}

void D3D12Renderer::InitializePerDeviceObjects() {
  HR(CreateDXGIFactory(IID_PPV_ARGS(&m_factory)));

  ComPtr<IDXGIAdapter> adapter = FindAdapter(m_factory.Get());
  HR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

  D3D12_COMMAND_QUEUE_DESC queueDesc;
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.NodeMask = 0;
  HR(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_directCommandQueue)));
  HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_directCommandAllocator)));
  HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_directCommandAllocator.Get(),
                                 /*pInitialState*/ nullptr, IID_PPV_ARGS(&m_cl)));

  // Command lists automatically start out as open.
  HR(m_cl->Close());

  m_constantBufferAllocator.Initialize(m_device.Get());
  m_linearSRVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_linearDSVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  m_linearRTVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  m_circularSRVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3D12Renderer::InitializePerWindowObjects(HWND hwnd) {
  m_window.Initialize(m_factory.Get(), m_device.Get(), m_directCommandQueue.Get(), hwnd);
  m_renderTarget.Initialize(m_device.Get(), m_linearRTVDescriptorAllocator.AllocateSingleDescriptor().cpuStart,
                            m_window.GetWidth(), m_window.GetHeight());
  m_depthBuffer.Initialize(m_device.Get(), m_linearDSVDescriptorAllocator.AllocateSingleDescriptor(),
                           m_window.GetWidth(), m_window.GetHeight());
}

void D3D12Renderer::InitializePerPassObjects() {
  m_colorPass.Initialize(m_device.Get());
  m_shadowMapPass.Initialize(m_device.Get());
}

void D3D12Renderer::InitializeFenceObjects() {
  HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
  m_fenceEvent = CreateEvent(nullptr, /*bManualRestart*/ FALSE, /*bInitialState*/ FALSE, nullptr);
  if (m_fenceEvent == nullptr)
    HR(HRESULT_FROM_WIN32(GetLastError()));
}

void D3D12Renderer::InitializeShadowMapObjects() {
  m_shadowMap.InitializeWithSRV(m_device.Get(), m_linearDSVDescriptorAllocator.AllocateSingleDescriptor(),
                                m_linearSRVDescriptorAllocator.AllocateSingleDescriptor(), 2000, 2000);

  m_shadowMapCamera.position_ = DirectX::XMFLOAT4(-1, 1, 1, 1.f);
  m_shadowMapCamera.look_at_ = DirectX::XMFLOAT4(0, 0, 0, 1);
  m_shadowMapCamera.widthInWorldCoordinates = 1.5;
  m_shadowMapCamera.heightInWorldCoordinates = 1.5;
}

void D3D12Renderer::HandleResize(unsigned int width, unsigned int height) {
  // TODO: We don't actually have to flush the GPU Work now that we're using a waitable swap
  // chain. But it shouldn't affect anything, so right now, let's keep it in.
  FlushGPUWork();
  m_window.HandleResize(width, height);
  m_renderTarget.Resize(m_device.Get(), width, height);
  m_depthBuffer.Resize(m_device.Get(), width, height);
}

void D3D12Renderer::DrawScene(Scene& scene) {
  HR(m_directCommandAllocator->Reset());
  HR(m_cl->Reset(m_directCommandAllocator.Get(), nullptr));

  RunShadowPass(scene.m_object);
  RunColorPass(scene.m_camera.GetPinholeCamera(), scene.m_object);

  CD3DX12_RESOURCE_BARRIER preCopyResourceBarriers[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(m_window.GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
                                           D3D12_RESOURCE_STATE_COPY_DEST),
      CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_COPY_SOURCE),
  };

  m_cl->ResourceBarrier(2, preCopyResourceBarriers);
  m_cl->CopyResource(m_window.GetCurrentBackBuffer(), m_renderTarget.GetResource());

  CD3DX12_RESOURCE_BARRIER postCopyResourceBarriers[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(m_window.GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST,
                                           D3D12_RESOURCE_STATE_PRESENT),
      CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET),
  };
  m_cl->ResourceBarrier(2, postCopyResourceBarriers);

  m_cl->Close();
  ID3D12CommandList* cl[] = {m_cl.Get()};
  m_directCommandQueue->ExecuteCommandLists(1, cl);
}

// Expects that the shadow map resource is in D3D12_RESOURCE_STATE_DEPTH_WRITE.
void D3D12Renderer::RunShadowPass(const Object& object) {
  m_cl->SetPipelineState(m_shadowMapPass.GetPipelineState());
  m_cl->SetGraphicsRootSignature(m_shadowMapPass.GetRootSignature());

  // Set up the constant buffer for the per-frame data.
  DirectX::XMFLOAT4X4 shadowMapViewPerspective4x4 = m_shadowMapCamera.GenerateViewPerspectiveTransform4x4();
  D3D12_GPU_VIRTUAL_ADDRESS shadowMapPerFrameConstantBuffer = m_constantBufferAllocator.AllocateAndUpload(
      sizeof(shadowMapViewPerspective4x4), &shadowMapViewPerspective4x4, m_nextFenceValue);
  m_cl->SetGraphicsRootConstantBufferView(/*rootParameterIndex*/ 0, shadowMapPerFrameConstantBuffer);

  // Set up the constant buffer for the per-object data.
  DirectX::XMFLOAT4X4 modelTransform4x4 = object.GenerateModelTransform4x4();
  D3D12_GPU_VIRTUAL_ADDRESS shadowMapModelTransformConstantBuffer =
      m_constantBufferAllocator.AllocateAndUpload(sizeof(modelTransform4x4), &modelTransform4x4, m_nextFenceValue);
  m_cl->SetGraphicsRootConstantBufferView(/*rootParameterIndex*/ 1, shadowMapModelTransformConstantBuffer);

  unsigned int shadowMapWidth = m_shadowMap.GetWidth();
  unsigned int shadowMapHeight = m_shadowMap.GetHeight();
  CD3DX12_VIEWPORT shadowMapClientAreaViewport(0.0f, 0.0f, static_cast<float>(shadowMapWidth),
                                               static_cast<float>(shadowMapHeight));
  CD3DX12_RECT shadowMapScissorRect(0, 0, shadowMapWidth, shadowMapHeight);
  m_cl->RSSetViewports(1, &shadowMapClientAreaViewport);
  m_cl->RSSetScissorRects(1, &shadowMapScissorRect);

  D3D12_CPU_DESCRIPTOR_HANDLE shadowMapDSVHandle = m_shadowMap.GetDSVDescriptorHandle();
  m_cl->OMSetRenderTargets(0, nullptr, FALSE, &shadowMapDSVHandle);
  m_cl->ClearDepthStencilView(shadowMapDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
  m_cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  m_cl->IASetVertexBuffers(0, 1, &object.model.m_vertexBufferView);
  m_cl->IASetIndexBuffer(&object.model.m_indexBufferView);

  for (const auto& meshPart : object.model.m_meshParts) {
    // TODO: Eventually we will want to reference the texture in the shadow pass, so that we can
    //       accurately clip pixels that are fully transparent.
    m_cl->DrawIndexedInstanced(meshPart.numIndices, /*instanceCount*/ 1, meshPart.indexStart, /*baseVertexLocation*/ 0,
                               /*startInstanceLocation*/ 0);
  }
}

// Expects that the shadow map resouce is in D3D12_RESOURCE_STATE_DEPTH_WRITE.
void D3D12Renderer::RunColorPass(const PinholeCamera& camera, const Object& object) {
  ID3D12Resource* backBuffer = m_window.GetCurrentBackBuffer();
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_renderTarget.GetRTVDescriptorHandle();
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthBuffer.GetDSVDescriptorHandle();

  m_cl->SetPipelineState(m_colorPass.GetPipelineState());
  m_cl->SetGraphicsRootSignature(m_colorPass.GetRootSignature());

  // Set up the constant buffer for the per-frame data.
  ColorPass::PerFrameData perFrameData;
  perFrameData.projectionViewTransform = camera.GenerateViewPerspectiveTransform4x4(m_window.GetAspectRatio());
  perFrameData.shadowMapProjectionViewTransform = m_shadowMapCamera.GenerateViewPerspectiveTransform4x4();
  perFrameData.lightDirection = m_shadowMapCamera.GetLightDirection();
  D3D12_GPU_VIRTUAL_ADDRESS colorPassPerFrameConstantBuffer =
      m_constantBufferAllocator.AllocateAndUpload(sizeof(ColorPass::PerFrameData), &perFrameData, m_nextFenceValue);
  m_cl->SetGraphicsRootConstantBufferView(/*rootParameterIndex*/ 0, colorPassPerFrameConstantBuffer);

  DirectX::XMMATRIX modelTransform = object.GenerateModelTransform();
  DirectX::XMMATRIX modelTransformModified = modelTransform;
  modelTransformModified.r[3] = DirectX::XMVectorSet(0.f, 0.f, 0.f, 1.f);
  DirectX::XMMATRIX modelTransformInverseTranspose =
      DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelTransformModified));

  DirectX::XMFLOAT4X4 transforms4x4[2];
  DirectX::XMStoreFloat4x4(&transforms4x4[0], modelTransform);
  // TODO: normals don't need the full 4x4, only 3x3. We should use XMStoreFloat3x3 instead.
  DirectX::XMStoreFloat4x4(&transforms4x4[1], modelTransformInverseTranspose);

  // Set up the constant buffer for the per-object data.
  D3D12_GPU_VIRTUAL_ADDRESS colorPassModelTransformsConstantBuffer =
      m_constantBufferAllocator.AllocateAndUpload(sizeof(transforms4x4), transforms4x4, m_nextFenceValue);
  m_cl->SetGraphicsRootConstantBufferView(/*rootParameterIndex*/ 1, colorPassModelTransformsConstantBuffer);

  unsigned int width = m_window.GetWidth();
  unsigned int height = m_window.GetHeight();
  CD3DX12_VIEWPORT clientAreaViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
  CD3DX12_RECT scissorRect(0, 0, width, height);
  m_cl->RSSetViewports(1, &clientAreaViewport);
  m_cl->RSSetScissorRects(1, &scissorRect);
  m_cl->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

  float clearColor[4] = {0.1f, 0.2f, 0.3f, 1.0f};
  m_cl->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_cl->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
  m_cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  m_cl->IASetVertexBuffers(0, 1, &object.model.m_vertexBufferView);
  m_cl->IASetIndexBuffer(&object.model.m_indexBufferView);

  CD3DX12_RESOURCE_BARRIER shadowMapResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
  m_cl->ResourceBarrier(1, &shadowMapResourceBarrier);

  // Set the descriptor heap.
  ID3D12DescriptorHeap* circularBufferSRVDescriptorHeap[] = {m_circularSRVDescriptorAllocator.GetDescriptorHeap()};
  m_cl->SetDescriptorHeaps(1, circularBufferSRVDescriptorHeap);

  DescriptorAllocation shadowMapSRVDescriptor =
      m_circularSRVDescriptorAllocator.AllocateSingleDescriptor(m_nextFenceValue);
  m_device->CopyDescriptorsSimple(1, shadowMapSRVDescriptor.cpuStart, m_shadowMap.GetSRVDescriptorHandle(),
                                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  m_cl->SetGraphicsRootDescriptorTable(2, shadowMapSRVDescriptor.gpuStart);

  for (const auto& meshPart : object.model.m_meshParts) {
    DescriptorAllocation textureSRVDescriptor =
        m_circularSRVDescriptorAllocator.AllocateSingleDescriptor(m_nextFenceValue);

    const Model::Material& material = object.model.m_materials[meshPart.materialIndex];
    m_device->CopyDescriptorsSimple(1, textureSRVDescriptor.cpuStart, material.m_srvDescriptor.cpuStart,
                                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_cl->SetGraphicsRootDescriptorTable(3, textureSRVDescriptor.gpuStart);
    m_cl->DrawIndexedInstanced(meshPart.numIndices, /*instanceCount*/ 1, meshPart.indexStart, /*baseVertexLocation*/ 0,
                               /*startInstanceLocation*/ 0);
  }

  shadowMapResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
  m_cl->ResourceBarrier(1, &shadowMapResourceBarrier);
}

void D3D12Renderer::SignalAndPresent() {
  HR(m_directCommandQueue->Signal(m_fence.Get(), m_nextFenceValue));
  ++m_nextFenceValue;

  m_window.Present();
}

void D3D12Renderer::WaitForNextFrame() {
  m_window.WaitForNextFrame();
  m_garbageCollector.Cleanup(m_fence->GetCompletedValue());
  m_constantBufferAllocator.Cleanup(m_fence->GetCompletedValue());
  m_circularSRVDescriptorAllocator.Cleanup(m_fence->GetCompletedValue());
}

void D3D12Renderer::FlushGPUWork() {
  UINT64 fenceValue = m_nextFenceValue;
  ++m_nextFenceValue;

  HR(m_directCommandQueue->Signal(m_fence.Get(), fenceValue));
  if (m_fence->GetCompletedValue() < fenceValue) {
    HR(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }

  m_garbageCollector.Cleanup(m_fence->GetCompletedValue());
  m_constantBufferAllocator.Cleanup(m_fence->GetCompletedValue());
}

ComPtr<ID3D12Resource> D3D12Renderer::AllocateAndUploadBufferData(const void* data, size_t sizeInBytes) {
  ComPtr<ID3D12Resource> buffer;

  CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC vertexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);
  HR(m_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferResourceDesc,
                                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&buffer)));

  ComPtr<ID3D12Resource> intermediateBuffer =
      ResourceHelper::AllocateIntermediateBuffer(m_device.Get(), buffer.Get(), m_garbageCollector, m_nextFenceValue);

  D3D12_SUBRESOURCE_DATA vertexData;
  vertexData.pData = data;
  vertexData.RowPitch = sizeInBytes;
  vertexData.SlicePitch = sizeInBytes;
  UpdateSubresources(m_cl.Get(), buffer.Get(), intermediateBuffer.Get(), 0, 0, 1, &vertexData);

  return buffer;
}

Microsoft::WRL::ComPtr<ID3D12Resource> D3D12Renderer::AllocateAndUploadTextureData(
    const void* textureData,
    DXGI_FORMAT format,
    size_t bytesPerPixel,
    size_t width,
    size_t height,
    /*out*/ DescriptorAllocation* srvDescriptor) {
  ComPtr<ID3D12Resource> texture;

  CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC textureResourceDesc =
      CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, /*arraySize*/ 1, /*mipLevels*/ 1);

  HR(m_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureResourceDesc,
                                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture)));

  ComPtr<ID3D12Resource> intermediateTextureBuffer =
      ResourceHelper::AllocateIntermediateBuffer(m_device.Get(), texture.Get(), m_garbageCollector, m_nextFenceValue);

  D3D12_SUBRESOURCE_DATA textureSubresourceData;
  textureSubresourceData.pData = textureData;
  textureSubresourceData.RowPitch = bytesPerPixel * width;
  textureSubresourceData.SlicePitch = bytesPerPixel * width * height;
  UpdateSubresources(m_cl.Get(), texture.Get(), intermediateTextureBuffer.Get(), 0, 0, 1, &textureSubresourceData);

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.Format = format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.PlaneSlice = 0;
  srvDesc.Texture2D.ResourceMinLODClamp = 0;

  *srvDescriptor = m_linearSRVDescriptorAllocator.AllocateSingleDescriptor();
  m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvDescriptor->cpuStart);

  return texture;
}

void D3D12Renderer::ExecuteBarriers(size_t numBarriers, const D3D12_RESOURCE_BARRIER* barriers) {
  m_cl->ResourceBarrier(numBarriers, barriers);
}

void D3D12Renderer::BeginResourceUpload() {
  HR(m_directCommandAllocator->Reset());
  HR(m_cl->Reset(m_directCommandAllocator.Get(), nullptr));
}

void D3D12Renderer::FinalizeResourceUpload() {
  HR(m_cl->Close());
  ID3D12CommandList* cl[] = {m_cl.Get()};
  m_directCommandQueue->ExecuteCommandLists(1, cl);

  FlushGPUWork();
}
