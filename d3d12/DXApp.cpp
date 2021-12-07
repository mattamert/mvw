#include "DXApp.h"

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#include <cassert>
#include <iostream>
#include <string>

#include "d3d12/MessageQueue.h"
#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

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

void DXApp::Initialize(HWND hwnd, std::shared_ptr<MessageQueue> messageQueue, std::string filename) {
  m_messageQueue = std::move(messageQueue);

  EnableDebugLayer();

  InitializePerDeviceObjects();
  InitializePerWindowObjects(hwnd);
  InitializePerPassObjects();
  InitializeFenceObjects();
  InitializeShadowMapObjects();
  InitializeScene(filename);

  HR(m_cl->Close());
  ID3D12CommandList* cl[] = {m_cl.Get()};
  m_directCommandQueue->ExecuteCommandLists(1, cl);

  FlushGPUWork();

  m_isInitialized = true;
}

bool DXApp::IsInitialized() const {
  return m_isInitialized;
}

void DXApp::InitializePerDeviceObjects() {
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

  // Keeping m_cl open so that default-heap resources can be initialized.

  m_constantBufferAllocator.Initialize(m_device.Get());
  m_linearSRVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_linearDSVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  m_linearRTVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  m_circularSRVDescriptorAllocator.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DXApp::InitializePerWindowObjects(HWND hwnd) {
  m_window.Initialize(m_factory.Get(), m_device.Get(), m_directCommandQueue.Get(), hwnd);
  m_renderTarget.Initialize(m_device.Get(), m_linearRTVDescriptorAllocator.AllocateSingleDescriptor().cpuStart,
                            m_window.GetWidth(), m_window.GetHeight());
  m_depthBuffer.Initialize(m_device.Get(), m_linearDSVDescriptorAllocator.AllocateSingleDescriptor(),
                           m_window.GetWidth(), m_window.GetHeight());
}

void DXApp::InitializePerPassObjects() {
  m_colorPass.Initialize(m_device.Get());
  m_shadowMapPass.Initialize(m_device.Get());
}

void DXApp::InitializeFenceObjects() {
  HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
  m_fenceEvent = CreateEvent(nullptr, /*bManualRestart*/ FALSE, /*bInitialState*/ FALSE, nullptr);
  if (m_fenceEvent == nullptr)
    HR(HRESULT_FROM_WIN32(GetLastError()));
}

void DXApp::InitializeShadowMapObjects() {
  m_shadowMap.InitializeWithSRV(m_device.Get(), m_linearDSVDescriptorAllocator.AllocateSingleDescriptor(),
                                m_linearSRVDescriptorAllocator.AllocateSingleDescriptor(), 2000, 2000);

  m_shadowMapCamera.position_ = DirectX::XMFLOAT4(-1, 1, 1, 1.f);
  m_shadowMapCamera.look_at_ = DirectX::XMFLOAT4(0, 0, 0, 1);
  m_shadowMapCamera.widthInWorldCoordinates = 1.5;
  m_shadowMapCamera.heightInWorldCoordinates = 1.5;
}

void DXApp::InitializeScene(const std::string& objFilename) {
  m_scene.Initialize(objFilename, m_device.Get(), m_cl.Get(), m_linearSRVDescriptorAllocator, m_garbageCollector,
                     m_nextFenceValue);
}

void DXApp::HandleResizeIfNecessary() {
  if (m_hasPendingResize) {
    // TODO: We don't actually have to flush the GPU Work now that we're using a waitable swap
    // chain. But it shouldn't affect anything, so right now, let's keep it in.
    FlushGPUWork();
    m_window.HandleResize(m_pendingClientWidth, m_pendingClientHeight);
    m_renderTarget.Resize(m_device.Get(), m_pendingClientWidth, m_pendingClientHeight);
    m_depthBuffer.Resize(m_device.Get(), m_pendingClientWidth, m_pendingClientHeight);
    m_hasPendingResize = false;
  }
}

void DXApp::DrawScene() {
  HandleResizeIfNecessary();
  WaitForNextFrame();

  m_scene.TickAnimations();

  HR(m_directCommandAllocator->Reset());
  HR(m_cl->Reset(m_directCommandAllocator.Get(), nullptr));

  RunShadowPass();
  RunColorPass();

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
void DXApp::RunShadowPass() {
  m_cl->SetPipelineState(m_shadowMapPass.GetPipelineState());
  m_cl->SetGraphicsRootSignature(m_shadowMapPass.GetRootSignature());

  // Set up the constant buffer for the per-frame data.
  DirectX::XMFLOAT4X4 shadowMapViewPerspective4x4 = m_shadowMapCamera.GenerateViewPerspectiveTransform4x4();
  D3D12_GPU_VIRTUAL_ADDRESS shadowMapPerFrameConstantBuffer = m_constantBufferAllocator.AllocateAndUpload(
      sizeof(shadowMapViewPerspective4x4), &shadowMapViewPerspective4x4, m_nextFenceValue);
  m_cl->SetGraphicsRootConstantBufferView(/*rootParameterIndex*/ 0, shadowMapPerFrameConstantBuffer);

  // Set up the constant buffer for the per-object data.
  DirectX::XMFLOAT4X4 modelTransform4x4 = m_scene.GetObj().GenerateModelTransform4x4();
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

  m_cl->IASetVertexBuffers(0, 1, &m_scene.GetObj().model.GetVertexBufferView());

  for (size_t i = 0; i < m_scene.GetObj().model.GetNumberOfGroups(); ++i) {
    // TODO: Eventually we will want to reference the texture in the shadow pass, so that we can
    // accurately clip pixels that are fully transparent.
    // m_cl->SetGraphicsRootDescriptorTable(2, m_object.model.GetTextureDescriptorHandle(i));
    m_cl->IASetIndexBuffer(&m_scene.GetObj().model.GetIndexBufferView(i));
    m_cl->DrawIndexedInstanced(m_scene.GetObj().model.GetNumIndices(i), 1, 0, 0, 0);
  }
}

// Expects that the shadow map resouce is in D3D12_RESOURCE_STATE_DEPTH_WRITE.
void DXApp::RunColorPass() {
  ID3D12Resource* backBuffer = m_window.GetCurrentBackBuffer();
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_renderTarget.GetRTVDescriptorHandle();
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthBuffer.GetDSVDescriptorHandle();

  m_cl->SetPipelineState(m_colorPass.GetPipelineState());
  m_cl->SetGraphicsRootSignature(m_colorPass.GetRootSignature());

  // Set up the constant buffer for the per-frame data.
  ColorPass::PerFrameData perFrameData;
  perFrameData.projectionViewTransform = m_scene.GetCamera().GenerateViewPerspectiveTransform4x4(m_window.GetAspectRatio());
  perFrameData.shadowMapProjectionViewTransform = m_shadowMapCamera.GenerateViewPerspectiveTransform4x4();
  perFrameData.lightDirection = m_shadowMapCamera.GetLightDirection();
  D3D12_GPU_VIRTUAL_ADDRESS colorPassPerFrameConstantBuffer =
      m_constantBufferAllocator.AllocateAndUpload(sizeof(ColorPass::PerFrameData), &perFrameData, m_nextFenceValue);
  m_cl->SetGraphicsRootConstantBufferView(/*rootParameterIndex*/ 0, colorPassPerFrameConstantBuffer);

  DirectX::XMMATRIX modelTransform = m_scene.GetObj().GenerateModelTransform();
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

  m_cl->IASetVertexBuffers(0, 1, &m_scene.GetObj().model.GetVertexBufferView());

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

  for (size_t i = 0; i < m_scene.GetObj().model.GetNumberOfGroups(); ++i) {
    DescriptorAllocation textureSRVDescriptor =
        m_circularSRVDescriptorAllocator.AllocateSingleDescriptor(m_nextFenceValue);
    m_device->CopyDescriptorsSimple(1, textureSRVDescriptor.cpuStart, m_scene.GetObj().model.GetTextureDescriptorHandle(i),
                                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_cl->SetGraphicsRootDescriptorTable(3, textureSRVDescriptor.gpuStart);

    m_cl->IASetIndexBuffer(&m_scene.GetObj().model.GetIndexBufferView(i));
    m_cl->DrawIndexedInstanced(m_scene.GetObj().model.GetNumIndices(i), 1, 0, 0, 0);
  }

  shadowMapResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
  m_cl->ResourceBarrier(1, &shadowMapResourceBarrier);
}

void DXApp::SignalAndPresent() {
  HR(m_directCommandQueue->Signal(m_fence.Get(), m_nextFenceValue));
  ++m_nextFenceValue;

  m_window.Present();
}

void DXApp::WaitForNextFrame() {
  m_window.WaitForNextFrame();
  m_garbageCollector.Cleanup(m_fence->GetCompletedValue());
  m_constantBufferAllocator.Cleanup(m_fence->GetCompletedValue());
  m_circularSRVDescriptorAllocator.Cleanup(m_fence->GetCompletedValue());
}

void DXApp::FlushGPUWork() {
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

bool DXApp::HandleMessages() {
  std::queue<MSG> messages = m_messageQueue->Flush();
  while (!messages.empty()) {
    const MSG& msg = messages.front();
    switch (msg.message) {
      case WM_SIZE: {
        UINT width = LOWORD(msg.lParam);
        UINT height = HIWORD(msg.lParam);
        m_pendingClientWidth = width;
        m_pendingClientHeight = height;
        m_hasPendingResize = true;
        break;
      }
      case WM_DESTROY:
        return true;
    }

    messages.pop();
  }
  return false;
}

void DXApp::RunRenderLoop(std::unique_ptr<DXApp> app) {
  assert(app->IsInitialized());

  while (true) {
    bool shouldQuit = app->HandleMessages();
    if (shouldQuit)
      break;

    app->DrawScene();
    app->SignalAndPresent();
  }

  app->FlushGPUWork();
}
