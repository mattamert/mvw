#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#include <cstdint>
#include <memory>

#include "d3d12/Animation.h"
#include "d3d12/Camera.h"
#include "d3d12/Object.h"
#include "d3d12/Pass.h"
#include "d3d12/ResourceGarbageCollector.h"
#include "d3d12/WindowTarget.h"

#define NUM_BACK_BUFFERS 2

class MessageQueue;

class DXApp {
 private:
  bool m_isInitialized = false;

  std::shared_ptr<MessageQueue> m_messageQueue;

  // Per-device data.
  Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
  Microsoft::WRL::ComPtr<ID3D12Device> m_device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_directCommandQueue;
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

  // Garbage collector.
  ResourceGarbageCollector m_garbageCollector;

  // App data.
  std::string m_objFilename;
  Object m_object;
  Animation m_objectRotationAnimation;
  bool m_isLeftButtonDown = false;
  int m_initialButtonDownPosX;
  int m_initialButtonDownPosY;
  int m_currentPointerX;
  int m_currentPointerY;
  int m_lastXOffset = 0;

  PinholeCamera m_camera;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBufferPerFrame;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBufferPerObject;

 public:
  void Initialize(HWND hwnd, std::shared_ptr<MessageQueue> messageQueue, std::string filename);
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
  void InitializeAppObjects(const std::string& objFilename);

  void FlushGPUWork();
  void WaitForNextFrame();

  // Input.
  void OnLeftButtonDown(int x, int y);
  void OnLeftButtonUp(int x, int y);
  void OnPointerUpdate(int x, int y);
};
