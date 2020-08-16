#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#include <memory>

#include "d3d12/Camera.h"
#include "d3d12/clock.h"
#include "d3d12/Pass.h"
#include "d3d12/WindowTarget.h"

#define NUM_BACK_BUFFERS 2

class MessageQueue;

class DXApp {
 private:
  bool m_isInitialized = false;
  //HWND m_hwnd;

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
  WindowTarget m_window;

  // Pass data.
  ColorPass m_colorPass;

  // Fence stuff.
  Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;  // Is this actually per-window?
  HANDLE m_fenceEvent = NULL;
  uint64_t m_nextFenceValue = 1;  // This must be initialized to 1, since fences start out at 0.

  // App data.
  Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

  PinholeCamera m_camera;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBufferPerFrame;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBufferPerObject;

 public:
  void Initialize(HWND hwnd, std::shared_ptr<MessageQueue> messageQueue);
  bool IsInitialized() const;

  void HandleResizeIfNecessary();
  void DrawScene();
  void SignalAndPresent();

  static void RunRenderLoop(std::unique_ptr<DXApp> app);
  bool HandleMessages();

 private:
  // Note: InitializePerDeviceObjects must be called before the others.
  void InitializePerDeviceObjects();
  void InitializePerWindowObjects(HWND hwnd);
  void InitializePerPassObjects();
  void InitializeFenceObjects();
  void InitializeAppObjects();

  void FlushGPUWork();
  void WaitForNextFrame();

  // TODO: Move to separate Resoruce Allocator class.
  static Microsoft::WRL::ComPtr<ID3D12Resource> AllocateBuffer(ID3D12Device* device,
                                                               uint64_t frameSignalValue,
                                                               unsigned int bytesToAllocate);
};
