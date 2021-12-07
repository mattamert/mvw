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
#include "d3d12/D3D12Renderer.h"
#include "d3d12/DescriptorHeapManagers.h"
#include "d3d12/Object.h"
#include "d3d12/Pass.h"
#include "d3d12/ResourceGarbageCollector.h"
#include "d3d12/Scene.h"
#include "d3d12/TextureResources.h"
#include "d3d12/WindowSwapChain.h"

class MessageQueue;

class DXApp {
 private:
  std::shared_ptr<MessageQueue> m_messageQueue;
  D3D12Renderer m_renderer;
  Scene m_scene;

  // Window data.
  bool m_hasPendingResize = false;
  unsigned int m_pendingClientWidth;
  unsigned int m_pendingClientHeight;

  // Flag used for debugging.
  bool m_isInitialized = false;

 public:
  void Initialize(std::shared_ptr<MessageQueue> messageQueue, HWND hwnd, std::string filename);
  bool IsInitialized() const;

  bool HandleMessages();
  void ExecuteFrame();
  void FlushGPUWork();
  static void RunRenderLoop(std::unique_ptr<DXApp> app);
};
