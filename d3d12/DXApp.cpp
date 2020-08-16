#include "DXApp.h"

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#include <cassert>
#include <iostream>
#include <string>

#include "MessageQueue.h"
#include "comhelper.h"
#include "d3dx12.h"

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

DXApp::DXApp() : m_isInitialized(false), m_currentBackBufferIndex(0), m_fenceEvent(NULL) { }

void DXApp::Initialize(HWND hwnd, std::shared_ptr<MessageQueue> messageQueue) {
  m_hwnd = hwnd;
  m_messageQueue = std::move(messageQueue);

  EnableDebugLayer();

  RECT clientArea;
  GetClientRect(m_hwnd, &clientArea);
  m_clientWidth = static_cast<unsigned int>(clientArea.right);
  m_clientHeight = static_cast<unsigned int>(clientArea.bottom);

  InitializePerDeviceObjects();
  InitializePerWindowObjects();
  InitializePerPassObjects();
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

void DXApp::InitializePerWindowObjects() {
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.Width = 0;  // Use automatic sizing.
  swapChainDesc.Height = 0;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // This is the most common swap chain format.
  swapChainDesc.Stereo = false;
  swapChainDesc.SampleDesc.Count = 1;  // Don't use multi-sampling.
  swapChainDesc.SampleDesc.Quality = 0;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = NUM_BACK_BUFFERS;  // Use double-buffering to minimize latency.
  swapChainDesc.Scaling = DXGI_SCALING_NONE;
  //swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  // Note: All Windows Store apps must use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL.
  // TODO: Maybe use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL? But only if we need to reuse pixels from
  // the previous frame.
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // Bitblt.
  swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

  ComPtr<IDXGISwapChain1> swapChain;
  HR(m_factory->CreateSwapChainForHwnd(m_directCommandQueue.Get(), m_hwnd, &swapChainDesc,
                                       /*pFullscreenDesc*/ nullptr,
                                       /*pRestrictToOutput*/ nullptr, &swapChain));

  HR(swapChain.As(&m_swapChain));
  m_frameWaitableObjectHandle = m_swapChain->GetFrameLatencyWaitableObject();

  D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
  rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvDescriptorHeapDesc.NumDescriptors = NUM_BACK_BUFFERS;
  rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  rtvDescriptorHeapDesc.NodeMask = 0;

  HR(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = swapChainDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapStart(
      m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (size_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
    HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));

    UINT rtvDescriptorOffset = descriptorSize * i;
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorPtr(descriptorHeapStart, rtvDescriptorOffset);

    m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvViewDesc, descriptorPtr);
    m_backBufferDescriptorHandles[i] = descriptorPtr;
  }

  HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
  m_fenceEvent = CreateEvent(nullptr, /*bManualRestart*/ FALSE, /*bInitialState*/ FALSE, nullptr);
  if (m_fenceEvent == nullptr)
    HR(HRESULT_FROM_WIN32(GetLastError()));
}

void DXApp::InitializePerPassObjects() {
  m_colorPass.Initialize(m_device.Get());
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
  m_camera.aspect_ratio_ = (float)m_clientWidth / (float)m_clientHeight;
  m_camera.look_at_ = DirectX::XMFLOAT4(0, 0, 0, 1);
  m_camera.position_ = DirectX::XMFLOAT4(0, 0, -2, 1);
}

void DXApp::OnResize() {
  FlushGPUWork();

  for (int i = 0; i < NUM_BACK_BUFFERS; ++i) {
    m_backBuffers[i].Reset();
  }

  HR(m_swapChain->ResizeBuffers(0, m_clientWidth, m_clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM,
                                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
  HR(m_swapChain->GetDesc1(&swapChainDesc));

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = swapChainDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapStart(
      m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (size_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
    HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));

    UINT rtvDescriptorOffset = descriptorSize * i;
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorPtr(descriptorHeapStart, rtvDescriptorOffset);

    m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvViewDesc, descriptorPtr);
    m_backBufferDescriptorHandles[i] = descriptorPtr;
  }

  m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

  m_camera.aspect_ratio_ = (float)m_clientWidth / (float)m_clientHeight;

  m_isResizePending = false;
}

void DXApp::DrawScene() {
  if (m_isResizePending)
    OnResize();

  WaitForNextFrame();

  ID3D12Resource* backBuffer = m_backBuffers[m_currentBackBufferIndex].Get();
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_backBufferDescriptorHandles[m_currentBackBufferIndex];

  HR(m_directCommandAllocator->Reset());
  HR(m_cl->Reset(m_directCommandAllocator.Get(), nullptr));

  m_cl->SetPipelineState(m_colorPass.GetPipelineState());
  m_cl->SetGraphicsRootSignature(m_colorPass.GetRootSignature());

  // Set up the constant buffer for the per-frame data.
  DirectX::XMMATRIX viewPerspective = m_camera.GenerateViewPerspectiveTransform();
  DirectX::XMFLOAT4X4 viewPerspective4x4;
  DirectX::XMStoreFloat4x4(&viewPerspective4x4, viewPerspective);
  ComPtr<ID3D12Resource> cameraConstantBuffer = m_bufferAllocator.AllocateBuffer(m_device.Get(), m_nextFenceValue, sizeof(viewPerspective4x4));

  uint8_t* mappedRegion;
  CD3DX12_RANGE readRange(0, 0);
  HR(cameraConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedRegion)));
  memcpy(mappedRegion, &viewPerspective4x4, sizeof(viewPerspective4x4));
  cameraConstantBuffer->Unmap(0, nullptr);

  m_cl->SetGraphicsRootConstantBufferView(0, cameraConstantBuffer->GetGPUVirtualAddress());

  // Set up the constant buffer for the per-object data.
  DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
  DirectX::XMFLOAT4X4 identity4x4;
  DirectX::XMStoreFloat4x4(&identity4x4, identity);
  ComPtr<ID3D12Resource> objectConstantBuffer = m_bufferAllocator.AllocateBuffer(m_device.Get(), m_nextFenceValue, sizeof(identity4x4));

  HR(objectConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedRegion)));
  memcpy(mappedRegion, &identity4x4, sizeof(identity4x4));
  objectConstantBuffer->Unmap(0, nullptr);

  m_cl->SetGraphicsRootConstantBufferView(1, objectConstantBuffer->GetGPUVirtualAddress());

  // Resume scheduled programming.

  CD3DX12_VIEWPORT clientAreaViewport(0.0f, 0.0f, static_cast<float>(m_clientWidth),
                                      static_cast<float>(m_clientHeight));
  CD3DX12_RECT scissorRect(0, 0, m_clientWidth, m_clientHeight);
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

void DXApp::PresentAndSignal() {
  m_swapChain->Present(1, 0);

  HR(m_directCommandQueue->Signal(m_fence.Get(), m_nextFenceValue));
  ++m_nextFenceValue;

  m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DXApp::WaitForNextFrame() {
  // Waitable swap chain reference:
  // https://docs.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains
  WaitForSingleObject(m_frameWaitableObjectHandle, INFINITE);

  m_bufferAllocator.Cleanup(m_fence->GetCompletedValue());
}

void DXApp::FlushGPUWork() {
  UINT64 fenceValue = m_nextFenceValue;
  ++m_nextFenceValue;

  HR(m_directCommandQueue->Signal(m_fence.Get(), fenceValue));
  if (m_fence->GetCompletedValue() < fenceValue) {
    HR(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }

  m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
  m_bufferAllocator.Cleanup(m_fence->GetCompletedValue());
}

bool DXApp::HandleMessages() {
  std::queue<MSG> messages = m_messageQueue->Flush();
  while (!messages.empty()) {
    const MSG& msg = messages.front();
    switch (msg.message) {
      case WM_SIZE: {
        UINT width = LOWORD(msg.lParam);
        UINT height = HIWORD(msg.lParam);
        if (m_clientWidth != width || m_clientHeight != height) {
          m_isResizePending = true;
          m_clientWidth = width;
          m_clientHeight = height;
        }
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
    app->PresentAndSignal();
  }

  app->FlushGPUWork();
}
