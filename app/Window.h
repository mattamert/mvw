#pragma once

#include <memory>
#include <string>
#include <thread>

#include "app/DXApp.h"
#include "utils/MessageQueue.h"

class Window {
 private:
  std::shared_ptr<MessageQueue> m_messageQueue;
  std::thread m_renderThread;

 public:
  Window() = default;

  void Initialize(std::string filename, bool isTownscaper);
  void PushMessage(MSG msg);
  void WaitForRenderThreadToFinish();
};
