#include "DXApp.h"

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#include <cassert>
#include <iostream>
#include <string>

#include "d3d12/MessageQueue.h"
#include "d3d12/ResourceHelper.h"
#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

using Microsoft::WRL::ComPtr;

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

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

    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device),
                                    nullptr))) {
      return adapter;
    }
  }

  return nullptr;
}

}  // namespace

void DXApp::Initialize(HWND hwnd, std::shared_ptr<MessageQueue> messageQueue) {
  m_messageQueue = std::move(messageQueue);

  EnableDebugLayer();

  InitializePerDeviceObjects();
  InitializePerWindowObjects(hwnd);
  InitializePerPassObjects();
  InitializeFenceObjects();
  InitializeAppObjects();

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

  HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      IID_PPV_ARGS(&m_directCommandAllocator)));

  HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_directCommandAllocator.Get(),
                                 /*pInitialState*/ nullptr, IID_PPV_ARGS(&m_cl)));
  HR(m_cl->Close());
}

void DXApp::InitializePerWindowObjects(HWND hwnd) {
  m_window.Initialize(m_factory.Get(), m_device.Get(), m_directCommandQueue.Get(), hwnd);
}

void DXApp::InitializePerPassObjects() {
  m_colorPass.Initialize(m_device.Get());
}

void DXApp::InitializeFenceObjects() {
  HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
  m_fenceEvent = CreateEvent(nullptr, /*bManualRestart*/ FALSE, /*bInitialState*/ FALSE, nullptr);
  if (m_fenceEvent == nullptr)
    HR(HRESULT_FROM_WIN32(GetLastError()));
}

void DXApp::InitializeAppObjects() {
  ColorPass::VertexData vertices[] = {
      // +x direction
      {+1.0f, -1.0f, -1.0f, 1.f, 0.f, 0.f},
      {+1.0f, +1.0f, -1.0f, 1.f, 0.f, 0.f},
      {+1.0f, +1.0f, +1.0f, 1.f, 0.f, 0.f},
      {+1.0f, -1.0f, +1.0f, 1.f, 0.f, 0.f},

      // -x direction
      {-1.0f, -1.0f, +1.0f, -1.f, 0.f, 0.f},
      {-1.0f, +1.0f, +1.0f, -1.f, 0.f, 0.f},
      {-1.0f, +1.0f, -1.0f, -1.f, 0.f, 0.f},
      {-1.0f, -1.0f, -1.0f, -1.f, 0.f, 0.f},

      // +y direction
      {+1.0f, +1.0f, +1.0f, 0.f, 1.f, 0.f},
      {+1.0f, +1.0f, -1.0f, 0.f, 1.f, 0.f},
      {-1.0f, +1.0f, -1.0f, 0.f, 1.f, 0.f},
      {-1.0f, +1.0f, +1.0f, 0.f, 1.f, 0.f},

      // -y direction
      {+1.0f, -1.0f, +1.0f, 0.f, -1.f, 0.f},
      {+1.0f, -1.0f, -1.0f, 0.f, -1.f, 0.f},
      {-1.0f, -1.0f, -1.0f, 0.f, -1.f, 0.f},
      {-1.0f, -1.0f, +1.0f, 0.f, -1.f, 0.f},

      // +z direction
      {+1.0f, -1.0f, +1.0f, 0.f, 0.f, 1.f},
      {+1.0f, +1.0f, +1.0f, 0.f, 0.f, 1.f},
      {-1.0f, +1.0f, +1.0f, 0.f, 0.f, 1.f},
      {-1.0f, -1.0f, +1.0f, 0.f, 0.f, 1.f},

      // -z direction
      {-1.0f, -1.0f, -1.0f, 0.f, 0.f, -1.f},
      {-1.0f, +1.0f, -1.0f, 0.f, 0.f, -1.f},
      {+1.0f, +1.0f, -1.0f, 0.f, 0.f, -1.f},
      {+1.0f, -1.0f, -1.0f, 0.f, 0.f, -1.f},
  };

  std::vector<uint32_t> indices;
  indices.reserve(6 * 6);
  for (size_t i = 0; i < 6; ++i) {
    uint32_t starting_index = i * 4;
    indices.push_back(starting_index);
    indices.push_back(starting_index + 1);
    indices.push_back(starting_index + 2);

    indices.push_back(starting_index);
    indices.push_back(starting_index + 2);
    indices.push_back(starting_index + 3);
  }

  const size_t vertexBufferSize = sizeof(vertices);
  CD3DX12_HEAP_PROPERTIES vertexBufferHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC vertexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
  HR(m_device->CreateCommittedResource(&vertexBufferHeapProperties, D3D12_HEAP_FLAG_NONE,
                                       &vertexBufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                       nullptr, IID_PPV_ARGS(&m_vertexBuffer)));

  ResourceHelper::UpdateBuffer(m_vertexBuffer.Get(), vertices, vertexBufferSize);

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = vertexBufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(ColorPass::VertexData);

  m_numIndices = indices.size();
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
  CD3DX12_HEAP_PROPERTIES indexBufferHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC indexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
  HR(m_device->CreateCommittedResource(&indexBufferHeapProperties, D3D12_HEAP_FLAG_NONE,
                                       &indexBufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                       nullptr, IID_PPV_ARGS(&m_indexBuffer)));

  ResourceHelper::UpdateBuffer(m_indexBuffer.Get(), indices.data(), indexBufferSize);

  m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes = indexBufferSize;
  m_indexBufferView.Format = DXGI_FORMAT_R32_UINT; // This might not be right.

  // Initialize the camera location.
  m_camera.look_at_ = DirectX::XMFLOAT4(0, 0, 0, 1);
  m_camera.position_ = DirectX::XMFLOAT4(0, 0, -4, 1);

  // Initialize the constant buffers.
  m_constantBufferPerFrame =
      ResourceHelper::AllocateBuffer(m_device.Get(), sizeof(DirectX::XMFLOAT4X4));
  m_constantBufferPerObject =
      ResourceHelper::AllocateBuffer(m_device.Get(), sizeof(DirectX::XMFLOAT4X4));
}

void DXApp::HandleResizeIfNecessary() {
  if (m_window.HasPendingResize()) {
    // TODO: We don't actually have to flush the GPU Work now that we're using a waitable swap
    // chain. But it shouldn't affect anything, so right now, let's keep it in.
    FlushGPUWork();
    m_window.HandlePendingResize();
  }
}

void DXApp::DrawScene() {
  HandleResizeIfNecessary();
  WaitForNextFrame();

  ID3D12Resource* backBuffer = m_window.GetCurrentBackBuffer();
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_window.GetCurrentBackBufferRTVHandle();
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_window.GetDepthStencilViewHandle();

  HR(m_directCommandAllocator->Reset());
  HR(m_cl->Reset(m_directCommandAllocator.Get(), nullptr));

  m_cl->SetPipelineState(m_colorPass.GetPipelineState());
  m_cl->SetGraphicsRootSignature(m_colorPass.GetRootSignature());

  // Set up the constant buffer for the per-frame data.
  DirectX::XMMATRIX viewPerspective =
      m_camera.GenerateViewPerspectiveTransform(m_window.GetAspectRatio());
  DirectX::XMFLOAT4X4 viewPerspective4x4;
  DirectX::XMStoreFloat4x4(&viewPerspective4x4, viewPerspective);

  ResourceHelper::UpdateBuffer(m_constantBufferPerFrame.Get(), &viewPerspective4x4, sizeof(viewPerspective4x4));
  m_cl->SetGraphicsRootConstantBufferView(0, m_constantBufferPerFrame->GetGPUVirtualAddress());

  // Set up the constant buffer for the per-object data.
  DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
  DirectX::XMFLOAT4X4 identity4x4;
  DirectX::XMStoreFloat4x4(&identity4x4, identity);

  ResourceHelper::UpdateBuffer(m_constantBufferPerObject.Get(), &identity4x4, sizeof(identity4x4));
  m_cl->SetGraphicsRootConstantBufferView(1, m_constantBufferPerObject->GetGPUVirtualAddress());

  // Resume scheduled programming.

  unsigned int width = m_window.GetWidth();
  unsigned int height = m_window.GetHeight();
  CD3DX12_VIEWPORT clientAreaViewport(0.0f, 0.0f, static_cast<float>(width),
                                      static_cast<float>(height));
  CD3DX12_RECT scissorRect(0, 0, width, height);
  m_cl->RSSetViewports(1, &clientAreaViewport);
  m_cl->RSSetScissorRects(1, &scissorRect);

  CD3DX12_RESOURCE_BARRIER rtvResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  m_cl->ResourceBarrier(1, &rtvResourceBarrier);
  m_cl->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

  float clearColor[4] = {0.1, 0.2, 0.3, 1.0};
  m_cl->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_cl->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
  m_cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_cl->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  m_cl->IASetIndexBuffer(&m_indexBufferView);
  m_cl->DrawIndexedInstanced(m_numIndices, 1, 0, 0, 0);

  rtvResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  m_cl->ResourceBarrier(1, &rtvResourceBarrier);

  m_cl->Close();

  ID3D12CommandList* cl[] = {m_cl.Get()};
  m_directCommandQueue->ExecuteCommandLists(1, cl);
}

void DXApp::SignalAndPresent() {
  HR(m_directCommandQueue->Signal(m_fence.Get(), m_nextFenceValue));
  ++m_nextFenceValue;

  m_window.Present();
}

void DXApp::WaitForNextFrame() {
  m_window.WaitForNextFrame();
}

void DXApp::FlushGPUWork() {
  UINT64 fenceValue = m_nextFenceValue;
  ++m_nextFenceValue;

  HR(m_directCommandQueue->Signal(m_fence.Get(), fenceValue));
  if (m_fence->GetCompletedValue() < fenceValue) {
    HR(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }
}

bool DXApp::HandleMessages() {
  std::queue<MSG> messages = m_messageQueue->Flush();
  while (!messages.empty()) {
    const MSG& msg = messages.front();
    switch (msg.message) {
      case WM_SIZE: {
        UINT width = LOWORD(msg.lParam);
        UINT height = HIWORD(msg.lParam);
        m_window.AddPendingResize(width, height);
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
