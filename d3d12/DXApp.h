#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#include <cstdint>
#include <memory>

#include "d3d12/Animation.h"
#include "d3d12/Camera.h"
#include "d3d12/ConstantBufferAllocator.h"
#include "d3d12/DescriptorHeapManagers.h"
#include "d3d12/Object.h"
#include "d3d12/Pass.h"
#include "d3d12/ResourceGarbageCollector.h"
#include "d3d12/TextureResources.h"
#include "d3d12/WindowSwapChain.h"

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

  ConstantBufferAllocator m_constantBufferAllocator;
  LinearDescriptorAllocator m_linearSRVDescriptorAllocator;
  LinearDescriptorAllocator m_linearDSVDescriptorAllocator;
  LinearDescriptorAllocator m_linearRTVDescriptorAllocator;
  CircularBufferDescriptorAllocator m_circularSRVDescriptorAllocator;

  // Per-window data.
  bool m_hasPendingResize = false;
  unsigned int m_pendingClientWidth;
  unsigned int m_pendingClientHeight;
  WindowSwapChain m_window;
  RenderTargetTexture m_renderTarget;
  DepthBufferTexture m_depthBuffer;

  // Pass data.
  ColorPass m_colorPass;
  ShadowMapPass m_shadowMapPass;

  // Fence stuff.
  Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;  // Is this actually per-window instead?
  HANDLE m_fenceEvent = NULL;
  uint64_t m_nextFenceValue = 1;  // This must be initialized to 1, since fences start out at 0.

  // Garbage collector.
  ResourceGarbageCollector m_garbageCollector;

  // App data.
  std::string m_objFilename;
  Object m_object;
  Animation m_objectRotationAnimation;

  PinholeCamera m_camera;

  DepthBufferTexture m_shadowMap;
  OrthographicCamera m_shadowMapCamera;

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
  void InitializeShadowMapObjects();
  void InitializeAppObjects(const std::string& objFilename);

  void RunShadowPass();
  void RunColorPass();

  void FlushGPUWork();
  void WaitForNextFrame();
};
