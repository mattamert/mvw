#include <Windows.h>

#include <cassert>
#include <thread>

#include "DXApp.h"
#include "MessageQueue.h"

#define USE_CONSOLE_SUBSYSTEM

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

class WindowHandler
{
private:
  std::shared_ptr<MessageQueue> messageQueue;
  std::thread renderThread;

public:
  WindowHandler() = default;
  void Initialize() {
    messageQueue = std::make_shared<MessageQueue>();
    std::unique_ptr<DXApp> app = std::make_unique<DXApp>();

    HWND hwnd = CreateDXWindow(this, L"DXApp", 640, 480);
    app->Initialize(hwnd, messageQueue);
    ShowDXWindow(hwnd);

    renderThread = std::thread(DXApp::RunRenderLoop, std::move(app));
  }

  void WaitForRenderThreadToFinish() {
    renderThread.join();
  }

  void PushMessage(MSG msg) {
    messageQueue->Push(msg);
  }

  static HWND CreateDXWindow(WindowHandler* handler, const std::wstring& windowName, int width, int height);
  static void ShowDXWindow(HWND hwnd);
};

static const wchar_t* g_className = L"DXApp";
static LRESULT CALLBACK DXWindowWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

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
    //HR(E_FAIL);
}

HWND WindowHandler::CreateDXWindow(
  WindowHandler* handler,
  const std::wstring& windowName,
  int width,
  int height) {
  RegisterDXWindow();

  // TODO: Look into how to remove the title bar.
  long windowStyle = WS_OVERLAPPEDWINDOW;
  HWND hwnd = CreateWindowW(
    g_className,          // Class name
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

void WindowHandler::ShowDXWindow(HWND hwnd) {
  if (hwnd) {
    ShowWindow(hwnd, SW_SHOWNORMAL);
  }
}

void RunMessageLoop() {
  // Future note: To wait on the present, use a Waitable swap chain
  // https://docs.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) == TRUE) {
    if (msg.message == WM_QUIT) {
      return;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

static LRESULT CALLBACK DXWindowWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  MSG msg;
  msg.hwnd = hwnd;
  msg.message = message;
  msg.wParam = wParam;
  msg.lParam = lParam;

  WindowHandler* handler = reinterpret_cast<WindowHandler*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (message) {
  case WM_CREATE: {
    LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
    WindowHandler* handler = (WindowHandler*)createStruct->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)handler);
    return 0;
  }

  case WM_SIZE:
    if (handler != nullptr) {
      handler->PushMessage(msg);
    }
    return 0;

  case WM_DESTROY:
    if (handler != nullptr) {
      handler->PushMessage(msg);
    }

    PostQuitMessage(0);
    return 0;

  default:
    return DefWindowProc(hwnd, message, wParam, lParam);
  }
}

#ifdef USE_CONSOLE_SUBSYSTEM

int main() {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      WindowHandler handler;
      handler.Initialize();
      RunMessageLoop();
      handler.WaitForRenderThreadToFinish();
    }
    CoUninitialize();
  }

  return 0;
}

#else

int WINAPI WinMain(HINSTANCE /*hInstance*/,
                   HINSTANCE /*hPrevInstance*/,
                   LPSTR /* lpCmdLine */,
                   int /*nCmdShow*/) {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      DXApp app;
      app.Initialize();
      RunMessageLoop();
    }
    CoUninitialize();
  }

  return 0;
}

#endif
