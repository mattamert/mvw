#pragma once

#include <memory>
#include <thread>

#include "DXApp.h"
#include "MessageQueue.h"

class WindowProxy {
 private:
  std::shared_ptr<MessageQueue> messageQueue;
  std::thread renderThread;

 public:
  WindowProxy() = default;

  void Initialize();
  void PushMessage(MSG msg);
  void WaitForRenderThreadToFinish();
};
