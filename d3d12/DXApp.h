#pragma once

#include <Windows.h>

#include <memory>

#include "d3d12/D3D12Renderer.h"
#include "d3d12/Scene.h"

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
