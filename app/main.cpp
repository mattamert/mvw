#include <Windows.h>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "app/DXApp.h"
#include "app/Window.h"
#include "utils/MessageQueue.h"

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

void EmitUsageMessage(const char* exeName) {
  std::cerr << "Usage: " << exeName << " [-townscaper] <obj file>" << std::endl;
}

int main(int argc, char** argv) {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  // Technically calling this API is discouraged, and it's prefered to specify this in the Application manifest.
  // See: https://docs.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  if (!IsMouseInPointerEnabled()) {
    (void)EnableMouseInPointer(TRUE);
  }

  if (argc < 2) {
    EmitUsageMessage(argv[0]);
    return 1;
  }

  std::string objFilename;
  bool isTownscaper = false;
  for (size_t i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "-townscaper") {
      isTownscaper = true;
    } else if (objFilename.empty()) {
      objFilename = std::move(arg);
    } else {
      EmitUsageMessage(argv[0]);
      return 1;
    }
  }

  if (!std::filesystem::exists(objFilename)) {
    std::cerr << "File '" << objFilename << "' not found." << std::endl;
    return 1;
  }

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      Window appWindow;
      appWindow.Initialize(std::move(objFilename), isTownscaper);
      RunMessageLoop();
    }
    CoUninitialize();
  }

  return 0;
}

#else

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /* lpCmdLine */, int /*nCmdShow*/) {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  if (argc != 2) {
    std::cerr << "Usage: d3d12_renderer.exe <obj file>" << std::endl;
    return 1;
  }

  std::string objFilename = std::string(argv[1]);
  if (!std::filesystem::exists(objFilename)) {
    std::cerr << "File not found." << std::endl;
    return 1;
  }

  if (SUCCEEDED(CoInitialize(NULL))) {
    {
      Window appWindow;
      appWindow.Initialize();
      RunMessageLoop();
    }
    CoUninitialize();
  }

  return 0;
}

#endif
