#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#include <memory>

#include "d3d12/Camera.h"
#include "d3d12/clock.h"
#include "d3d12/Pass.h"
#include "d3d12/PerFrameAllocator.h"

#define NUM_BACK_BUFFERS 2

class MessageQueue;

class DXApp {
 private:
  bool m_isInitialized;
  HWND m_hwnd;

  std::shared_ptr<MessageQueue> m_messageQueue;

  // Per-device data.
  // TODO: These should really be abstracted into their own class, as they are not exactly bound to
  // one window.
  Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
  Microsoft::WRL::ComPtr<ID3D12Device> m_device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_directCommandQueue;
  // Command allocators and command lists especially should be split into something different, as
  // there should be 1 per render thread.
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_directCommandAllocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cl;

  // Per-window data.

  // Swap chain stuff.
  Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[NUM_BACK_BUFFERS];
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_backBufferDescriptorHandles[NUM_BACK_BUFFERS];
  UINT m_currentBackBufferIndex;
  HANDLE m_frameWaitableObjectHandle;

  // Fence stuff.
  Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;  // Is this actually per-window?
  HANDLE m_fenceEvent;
  uint64_t m_nextFenceValue = 1;  // This must be initialized to 1, since fences start out at 0.

  unsigned int m_clientWidth;
  unsigned int m_clientHeight;
  bool m_isResizePending = false;

  // Pass data.
  ColorPass m_colorPass;

  // App data.
  Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

  PinholeCamera m_camera;
  PerFrameAllocator m_bufferAllocator;

 public:
  DXApp();
  void Initialize(HWND hwnd, std::shared_ptr<MessageQueue> messageQueue);
  bool IsInitialized() const;

  void OnResize();
  void DrawScene();
  void PresentAndSignal();

  static void RunRenderLoop(std::unique_ptr<DXApp> app);
  bool HandleMessages();

 private:
  // Note: InitializePerDeviceObjects must be called before the others.
  void InitializePerDeviceObjects();
  void InitializePerWindowObjects();
  void InitializePerPassObjects();
  void InitializeAppObjects();

  void FlushGPUWork();
  void WaitForNextFrame();
};
