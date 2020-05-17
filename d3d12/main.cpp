#include <Windows.h>

#include "d3d12/DXWindow.h"

#define USE_CONSOLE_SUBSYSTEM

void RunMessageLoop() {
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

#ifdef USE_CONSOLE_SUBSYSTEM

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
