#include <Windows.h>

#include <cassert>

#include "DXApp.h"

#define USE_CONSOLE_SUBSYSTEM

void RunMessageLoop(DXApp* app) {
  assert(app->IsInitialized());

  // TODO: This isn't ideal. What's happening is that this loop is running as fast as possible.
  // The rendering code doesn't actually block for a full frame, but rather only until the GPU work is done.
  // It will then submit the frame into the frame queue (likely overwriting the previously submitted frame).
  // The application is never blocked by the present, and so renders many frames faster than needed.
  // This can be sorted out later.
  // Additionally, as long as the window is moving, the message queue will never fully remove the move message(s)
  // and so rendering never actually happens. This needs to be fixed in the future.
  // But for now, let's just stick with what's simple.
  // Future note: To wait on the present, use a Waitable swap chain
  // https://docs.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains
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
      DXApp app;
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
      DXApp app;
      app.Initialize();
      RunMessageLoop();
    }
    CoUninitialize();
  }

  return 0;
}

#endif
