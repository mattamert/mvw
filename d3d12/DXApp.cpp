#include "DXApp.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"
#include "d3d12/MessageQueue.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <Windows.h>

#include <cassert>
#include <iostream>
#include <string>

void DXApp::Initialize(std::shared_ptr<MessageQueue> messageQueue, HWND hwnd, std::string filename) {
  m_messageQueue = std::move(messageQueue);
  m_renderer.Initialize(hwnd);
  m_scene.Initialize(filename, &m_renderer);
  m_renderer.FinializeResourceUpload(); // This whole thing is super jank, but it works for now.
  m_isInitialized = true;
}

bool DXApp::IsInitialized() const {
  return m_isInitialized;
}

bool DXApp::HandleMessages() {
  std::queue<MSG> messages = m_messageQueue->Flush();
  while (!messages.empty()) {
    const MSG& msg = messages.front();
    switch (msg.message) {
      case WM_SIZE: {
        UINT width = LOWORD(msg.lParam);
        UINT height = HIWORD(msg.lParam);
        m_pendingClientWidth = width;
        m_pendingClientHeight = height;
        m_hasPendingResize = true;
        break;
      }
      case WM_DESTROY:
        return true;
    }

    messages.pop();
  }
  return false;
}

void DXApp::ExecuteFrame() {
  // TODO: Investigate if we should be handling the resize after the WaitForNextFrame.
  if (m_hasPendingResize) {
    m_renderer.HandleResize(m_pendingClientWidth, m_pendingClientHeight);
    m_hasPendingResize = false;
  }

  m_renderer.WaitForNextFrame();
  m_scene.TickAnimations();
  m_renderer.DrawScene(m_scene);
  m_renderer.SignalAndPresent();
}

void DXApp::FlushGPUWork() {
  m_renderer.FlushGPUWork();
}

void DXApp::RunRenderLoop(std::unique_ptr<DXApp> app) {
  assert(app->IsInitialized());

  while (true) {
    bool shouldQuit = app->HandleMessages();
    if (shouldQuit)
      break;

    app->ExecuteFrame();
  }

  app->FlushGPUWork();
}
