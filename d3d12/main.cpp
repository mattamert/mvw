#include <Windows.h>

#include <cassert>

#include "d3d12/DXWindow.h"

#define USE_CONSOLE_SUBSYSTEM

void RunMessageLoop(DXWindow* app) {
  assert(app->IsInitialized());

  while (true) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE) {
      if (msg.message == WM_QUIT) {
        return;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    app->DrawScene();
    app->PresentAndSignal();
  }
}

#ifdef USE_CONSOLE_SUBSYSTEM

int main() {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      DXWindow app;
      app.Initialize();
      RunMessageLoop(&app);
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
      DXWindow app;
      app.Initialize();
      RunMessageLoop();
    }
    CoUninitialize();
  }

  return 0;
}

#endif
