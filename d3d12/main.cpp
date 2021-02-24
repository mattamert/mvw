#include <Windows.h>

#include <cassert>
#include <string>
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

int main(int argc, char** argv) {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  if (!IsMouseInPointerEnabled()) {
    (void)EnableMouseInPointer(TRUE);
  }

  std::string objFilename =
      (argc > 1) ? std::string(argv[1])
                 : "C:\\Users\\Matt\\Documents\\Assets\\StanfordBunnyTextured\\20180310_KickAir8P_"
                   "UVUnwrapped_Stanford_Bunny.obj";

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      WindowProxy proxy;
      proxy.Initialize(std::move(objFilename));
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

  // TODO: Command line arguments to get filename of obj file.

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

#endif
