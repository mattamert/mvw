#pragma once

#include "d3d12/D3D12Renderer.h"
#include "d3d12/Scene.h"

#include <Windows.h>

#include <memory>

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

  // Input.
  bool m_isLeftButtonDown = false;
  int m_currentPointerX;
  int m_currentPointerY;

  // Flag used for debugging.
  bool m_isInitialized = false;

 public:
  void Initialize(std::shared_ptr<MessageQueue> messageQueue, HWND hwnd, std::string filename);
  bool IsInitialized() const;

  bool HandleMessages();
  void ExecuteFrame();
  void FlushGPUWork();
  static void RunRenderLoop(std::unique_ptr<DXApp> app);

  // Input.
  void OnLeftButtonDown(int x, int y);
  void OnLeftButtonUp(int x, int y);
  void OnPointerUpdate(int x, int y);
  void OnPointerWheel(float wheelDelta);
};
