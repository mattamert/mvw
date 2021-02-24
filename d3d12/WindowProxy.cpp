#include "d3d12/WindowProxy.h"

#include <string>

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

namespace {
static LRESULT CALLBACK DXWindowWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  MSG msg;
  msg.hwnd = hwnd;
  msg.message = message;
  msg.wParam = wParam;
  msg.lParam = lParam;

  WindowProxy* handler = reinterpret_cast<WindowProxy*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (message) {
    case WM_CREATE: {
      LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
      WindowProxy* handler = (WindowProxy*)createStruct->lpCreateParams;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)handler);
      return 0;
    }

    case WM_SIZE:
      if (handler != nullptr) {
        handler->PushMessage(msg);
      }
      return 0;

    case WM_POINTERUPDATE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERLEAVE:
      if (handler != nullptr) {
        handler->PushMessage(msg);
      }
      return 0;

    case WM_DESTROY:
      if (handler != nullptr) {
        handler->PushMessage(msg);

        // NOTE: Make sure that the render thread finishes up before we destroy the window.
        // If we don't, then there are issues with the render thread flushing the command queue.
        handler->WaitForRenderThreadToFinish();
      }

      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProc(hwnd, message, wParam, lParam);
  }
}

static const wchar_t* g_className = L"DXApp";
void RegisterDXWindow() {
  WNDCLASSEXW windowClass;
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = DXWindowWndProc;
  windowClass.cbClsExtra = 0;
  windowClass.cbWndExtra = sizeof(LONG_PTR);
  windowClass.hInstance = HINST_THISCOMPONENT;
  windowClass.hbrBackground = NULL;
  windowClass.lpszMenuName = NULL;
  windowClass.hCursor = LoadCursor(NULL, IDI_APPLICATION);
  windowClass.lpszClassName = g_className;
  windowClass.hIcon = NULL;
  windowClass.hIconSm = NULL;

  ATOM result = RegisterClassExW(&windowClass);
  DWORD err = GetLastError();

  // This is so that we don't have to necessarily keep track of whether or not
  // we've called this function before.
  // We just assume that this will be the only function in this process that
  // will register a window class with this class name.
  if (result == 0 && err != ERROR_CLASS_ALREADY_EXISTS)
    throw;
  // HR(E_FAIL);
}

HWND CreateDXWindow(WindowProxy* handler, const std::wstring& windowName, int width, int height) {
  RegisterDXWindow();

  // TODO: Look into how to remove the title bar.
  long windowStyle = WS_OVERLAPPEDWINDOW;
  HWND hwnd = CreateWindowW(g_className,          // Class name
                            windowName.c_str(),   // Window Name.
                            windowStyle,          // Style
                            CW_USEDEFAULT,        // x
                            CW_USEDEFAULT,        // y
                            width,                // Horiz size (pixels)
                            height,               // Vert size (pixels)
                            NULL,                 // parent window
                            NULL,                 // menu
                            HINST_THISCOMPONENT,  // Instance ...?
                            handler               // lparam
  );

  return hwnd;
}

void ShowDXWindow(HWND hwnd) {
  if (hwnd)
    ShowWindow(hwnd, SW_SHOWNORMAL);
}

}  // namespace

void WindowProxy::Initialize(std::string filename) {
  messageQueue = std::make_shared<MessageQueue>();

  HWND hwnd = CreateDXWindow(this, L"DXApp", 640, 480);

  std::unique_ptr<DXApp> app = std::make_unique<DXApp>();
  app->Initialize(hwnd, messageQueue, std::move(filename));

  ShowDXWindow(hwnd);

  renderThread = std::thread(DXApp::RunRenderLoop, std::move(app));
}

void WindowProxy::WaitForRenderThreadToFinish() {
  renderThread.join();
}

void WindowProxy::PushMessage(MSG msg) {
  messageQueue->Push(msg);
}
