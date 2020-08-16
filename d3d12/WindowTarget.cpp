#include "d3d12/WindowTarget.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

#include <assert.h>

using namespace Microsoft::WRL;

void WindowTarget::Initialize(IDXGIFactory2* factory,
                              ID3D12Device* device,
                              ID3D12CommandQueue* commandQueue,
                              HWND hwnd) {
  m_hwnd = hwnd;

  RECT clientArea;
  GetClientRect(m_hwnd, &clientArea);
  m_clientWidth = static_cast<unsigned int>(clientArea.right);
  m_clientHeight = static_cast<unsigned int>(clientArea.bottom);

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
  // swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  // Note: All Windows Store apps must use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL.
  // TODO: Maybe use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL? But only if we need to reuse pixels from
  // the previous frame.
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // Bitblt.
  swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

  ComPtr<IDXGISwapChain1> swapChain;
  HR(factory->CreateSwapChainForHwnd(commandQueue, m_hwnd, &swapChainDesc,
                                     /*pFullscreenDesc*/ nullptr, /*pRestrictToOutput*/ nullptr,
                                     &swapChain));

  HR(swapChain.As(&m_swapChain));
  m_frameWaitableObjectHandle = m_swapChain->GetFrameLatencyWaitableObject();

  D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
  rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvDescriptorHeapDesc.NumDescriptors = NUM_BACK_BUFFERS;
  rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  rtvDescriptorHeapDesc.NodeMask = 0;

  HR(device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = swapChainDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapStart(
      m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (size_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
    HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));

    UINT rtvDescriptorOffset = descriptorSize * i;
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorPtr(descriptorHeapStart, rtvDescriptorOffset);

    device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvViewDesc, descriptorPtr);
    m_backBufferDescriptorHandles[i] = descriptorPtr;
  }
}

void WindowTarget::AddPendingResize(unsigned int width, unsigned int height) {
  m_pendingClientWidth = width;
  m_pendingClientHeight = height;
  m_isResizePending = true;
}

bool WindowTarget::HasPendingResize() const {
  return m_isResizePending;
}

void WindowTarget::HandlePendingResize() {
  assert(m_isResizePending);

  for (int i = 0; i < NUM_BACK_BUFFERS; ++i) {
    m_backBuffers[i].Reset();
  }

  HR(m_swapChain->ResizeBuffers(0, m_pendingClientWidth, m_pendingClientHeight,
                                DXGI_FORMAT_R8G8B8A8_UNORM,
                                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

  ComPtr<ID3D12Device> device;
  HR(m_swapChain->GetDevice(IID_PPV_ARGS(&device)));

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
  HR(m_swapChain->GetDesc1(&swapChainDesc));

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = swapChainDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapStart(
      m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (size_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
    HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));

    UINT rtvDescriptorOffset = descriptorSize * i;
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorPtr(descriptorHeapStart, rtvDescriptorOffset);

    device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvViewDesc, descriptorPtr);
    m_backBufferDescriptorHandles[i] = descriptorPtr;
  }

  m_clientWidth = m_pendingClientWidth;
  m_clientHeight = m_pendingClientHeight;
  m_isResizePending = false;
}

void WindowTarget::WaitForNextFrame() {
  // Waitable swap chain reference:
  // https://docs.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains
  WaitForSingleObject(m_frameWaitableObjectHandle, INFINITE);
}

void WindowTarget::Present() {
  m_swapChain->Present(1, 0);
}

ID3D12Resource* WindowTarget::GetCurrentBackBuffer() {
  UINT currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
  assert(currentBackBufferIndex < NUM_BACK_BUFFERS);
  return m_backBuffers[currentBackBufferIndex].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE WindowTarget::GetCurrentBackBufferRTVHandle() {
  UINT currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
  assert(currentBackBufferIndex < NUM_BACK_BUFFERS);
  return m_backBufferDescriptorHandles[currentBackBufferIndex];
}

unsigned int WindowTarget::GetWidth() const {
  return m_clientWidth;
}
unsigned int WindowTarget::GetHeight() const {
  return m_clientHeight;
}

float WindowTarget::GetAspectRatio() const {
  return (float)m_clientWidth / (float)m_clientHeight;
}
