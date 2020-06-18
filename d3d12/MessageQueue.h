#pragma once

#include <Windows.h>

#include <mutex>
#include <queue>

class MessageQueue {
 private:
  std::queue<MSG> m_messageQueue;
  std::mutex m_mut;

 public:
  MessageQueue() = default;
  void Push(MSG message);
  std::queue<MSG> Flush();
};