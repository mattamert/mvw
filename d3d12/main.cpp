#include <Windows.h>

#include <cassert>
#include <thread>

#include "d3d12/DXApp.h"
#include "d3d12/MessageQueue.h"
#include "d3d12/WindowProxy.h"

#define USE_CONSOLE_SUBSYSTEM

void RunMessageLoop() {
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) == TRUE) {
    if (msg.message == WM_QUIT) {
      return;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

#ifdef USE_CONSOLE_SUBSYSTEM

int main() {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      WindowProxy proxy;
      proxy.Initialize();
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
      WindowProxy proxy;
      proxy.Initialize();
      RunMessageLoop();
      proxy.WaitForRenderThreadToFinish();
    }
    CoUninitialize();
  }

  return 0;
}

#endif
