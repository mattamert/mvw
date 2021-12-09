#include "DXApp.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"
#include "d3d12/MessageQueue.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <windowsx.h> // For GET_X_LPARAM & family.

#include <cassert>
#include <iostream>
#include <string>

void DXApp::Initialize(std::shared_ptr<MessageQueue> messageQueue, HWND hwnd, std::string filename) {
  m_messageQueue = std::move(messageQueue);
  m_renderer.Initialize(hwnd);
  m_scene.Initialize(filename, &m_renderer);
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

      case WM_POINTERDOWN:
        std::cout << "WM_POINTERDOWN - flag: " << std::hex << (DWORD)HIWORD(msg.wParam) << std::endl;
        if (IS_POINTER_FIRSTBUTTON_WPARAM(msg.wParam)) {
          int x = GET_X_LPARAM(msg.lParam);
          int y = GET_Y_LPARAM(msg.lParam);
          OnLeftButtonDown(x, y);
        }
        break;

      case WM_POINTERUP:
        // It's a little strange, but WM_POINTERUP messages don't appear to set the flag for which button-up event it
        // refers to. Instead, it seems to rely on the application to have kept track of it, and assumes that there can
        // never be 2 pointer buttons held down at the same time. (This largely goes against what the documentation
        // implicitly says though).
        // FUTURE NOTE: Please look into POINTER_BUTTON_CHANGE_TYPE, and the POINTER_INFO structure.
        //
        // Also, from the disclaimer on the documentation:
        //    When a window loses capture of a pointer and it receives the WM_POINTERCAPTURECHANGED notification, it
        //    typically will not receive any further notifications. For this reason, it is important that you not make
        //    any assumptions based on evenly paired WM_POINTERDOWN/WM_POINTERUP or WM_POINTERENTER/WM_POINTERLEAVE
        //    notifications.
        // Therefore, we should also add logic for WM_POINTERCAPTURECHANGED to correctly configure m_isLeftButtonDown.
        if (m_isLeftButtonDown) {
          int x = GET_X_LPARAM(msg.lParam);
          int y = GET_Y_LPARAM(msg.lParam);
          OnLeftButtonUp(x, y);
        }
        break;

      // Touch input never actually sends a WM_POINTERUP for some reason, but rather WM_POINTERLEAVE.
      // NOTE: I don't think this is true; should probably look at this again.
      case WM_POINTERLEAVE:
        if (m_isLeftButtonDown) {
          int x = GET_X_LPARAM(msg.lParam);
          int y = GET_Y_LPARAM(msg.lParam);
          OnLeftButtonUp(x, y);
        }
        break;

      case WM_POINTERUPDATE: {
        int x = GET_X_LPARAM(msg.lParam);
        int y = GET_Y_LPARAM(msg.lParam);
        OnPointerUpdate(x, y);
        break;
      }

      case WM_POINTERWHEEL: {
        // MEGA HACK ALERT!
        // See the comment in WindowProxy.cpp.
        int wheelDelta = msg.lParam;
        float wheelDeltaNormalized = (float)wheelDelta / WHEEL_DELTA;
        OnPointerWheel(wheelDeltaNormalized);
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
    if (m_pendingClientWidth > 0 && m_pendingClientHeight > 0) {
      m_renderer.HandleResize(m_pendingClientWidth, m_pendingClientHeight);
    }
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

void DXApp::OnLeftButtonDown(int x, int y) {
  m_isLeftButtonDown = true;
  m_currentPointerX = x;
  m_currentPointerY = y;
}

void DXApp::OnLeftButtonUp(int x, int y) {
  m_isLeftButtonDown = false;
  m_currentPointerX = 0;
  m_currentPointerY = 0;
}

void DXApp::OnPointerUpdate(int x, int y) {
  if (m_isLeftButtonDown) {
    int deltaX = x - m_currentPointerX;
    int deltaY = y - m_currentPointerY;
    m_currentPointerX = x;
    m_currentPointerY = y;

    m_scene.m_camera.OnMouseDrag(deltaX, deltaY);
  }
}

void DXApp::OnPointerWheel(float wheelDelta) {
  m_scene.m_camera.OnMouseWheel(wheelDelta);
}
