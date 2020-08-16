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
  // What we need: 1. Actual vertex data. 2. Buffer for the vertex data.
  ColorPass::VertexData vertices[3] = {
      {0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f},
      {0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f},
      {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
  };

  const unsigned int bufferSize = sizeof(vertices);

  CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  HR(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                       IID_PPV_ARGS(&m_vertexBuffer)));

  uint8_t* mappedRegion;
  CD3DX12_RANGE readRange(0, 0);
  HR(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedRegion)));
  memcpy(mappedRegion, vertices, bufferSize);
  m_vertexBuffer->Unmap(0, nullptr);

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = bufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(ColorPass::VertexData);

  // Initialize the camera location.
  m_camera.look_at_ = DirectX::XMFLOAT4(0, 0, 0, 1);
  m_camera.position_ = DirectX::XMFLOAT4(0, 0, -2, 1);

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

  HR(m_directCommandAllocator->Reset());
  HR(m_cl->Reset(m_directCommandAllocator.Get(), nullptr));

  m_cl->SetPipelineState(m_colorPass.GetPipelineState());
  m_cl->SetGraphicsRootSignature(m_colorPass.GetRootSignature());

  // Set up the constant buffer for the per-frame data.
  DirectX::XMMATRIX viewPerspective =
      m_camera.GenerateViewPerspectiveTransform(m_window.GetAspectRatio());
  DirectX::XMFLOAT4X4 viewPerspective4x4;
  DirectX::XMStoreFloat4x4(&viewPerspective4x4, viewPerspective);

  uint8_t* mappedRegion;
  CD3DX12_RANGE readRange(0, 0);
  HR(m_constantBufferPerFrame->Map(0, &readRange, reinterpret_cast<void**>(&mappedRegion)));
  memcpy(mappedRegion, &viewPerspective4x4, sizeof(viewPerspective4x4));
  m_constantBufferPerFrame->Unmap(0, nullptr);

  m_cl->SetGraphicsRootConstantBufferView(0, m_constantBufferPerFrame->GetGPUVirtualAddress());

  // Set up the constant buffer for the per-object data.
  DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
  DirectX::XMFLOAT4X4 identity4x4;
  DirectX::XMStoreFloat4x4(&identity4x4, identity);

  HR(m_constantBufferPerObject->Map(0, &readRange, reinterpret_cast<void**>(&mappedRegion)));
  memcpy(mappedRegion, &identity4x4, sizeof(identity4x4));
  m_constantBufferPerObject->Unmap(0, nullptr);

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
  m_cl->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

  float clearColor[4] = {0.1, 0.2, 0.3, 1.0};
  m_cl->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_cl->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  m_cl->DrawInstanced(3, 1, 0, 0);

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
