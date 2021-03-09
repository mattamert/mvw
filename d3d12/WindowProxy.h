#pragma once

#include <memory>
#include <string>
#include <thread>

#include "d3d12/DXApp.h"
#include "d3d12/MessageQueue.h"

class WindowProxy {
 private:
  std::shared_ptr<MessageQueue> m_messageQueue;
  std::thread m_renderThread;

 public:
  WindowProxy() = default;

  void Initialize(std::string filename);
  void PushMessage(MSG msg);
  void WaitForRenderThreadToFinish();
};
