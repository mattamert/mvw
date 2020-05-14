#include <Windows.h>

#include "d3d12/DXWindow.h"

#define IDT_TIMER1 1234

LRESULT CALLBACK WndProc(HWND hwnd,
                         UINT message,
                         WPARAM wParam,
                         LPARAM lParam) {
  DXWindow* app =
      reinterpret_cast<DXWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (message) {
    case WM_CREATE: {
      LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
      DXWindow* app = (DXWindow*)createStruct->lpCreateParams;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);

      SetTimer(hwnd,              // handle to main window
               IDT_TIMER1,        // timer identifier
               16,                // 10-second interval
               (TIMERPROC)NULL);  // no timer callback

      return 0;
    }

    case WM_TIMER:
      if (wParam == IDT_TIMER1) {
        InvalidateRect(hwnd, nullptr, false);
      }
      return 0;

    case WM_SIZE:
      if (app != nullptr) {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        app->OnResize(width, height);
      }
      return 0;

    case WM_DISPLAYCHANGE:
      if (app != nullptr) {
        InvalidateRect(hwnd, nullptr, false);
      }
      return 0;

    case WM_PAINT:
      if (app != nullptr) {
        //app->Animate();
        app->DrawScene();
        app->Present();
        ValidateRect(hwnd, nullptr);
      }
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProc(hwnd, message, wParam, lParam);
  }
}

void RunMessageLoop() {
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

// int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
// LPSTR /* lpCmdLine */, int /*nCmdShow*/)
//{
//    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
//
//    if (SUCCEEDED(CoInitialize(NULL)))
//    {
//        {
//            App app;
//            app.Initialize();
//            RunMessageLoop();
//        }
//        CoUninitialize();
//    }
//
//    return 0;
//}

int main() {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      DXWindow app;
      app.Initialize();
      RunMessageLoop();
    }
    CoUninitialize();
  }

  return 0;
}