#include "MessageQueue.h"

void MessageQueue::Push(MSG message) {
  m_mut.lock();
  m_messageQueue.push(message);
  m_mut.unlock();
}

std::queue<MSG> MessageQueue::Flush() {
  m_mut.lock();
  std::queue<MSG> queue;
  m_messageQueue.swap(queue);
  m_mut.unlock();
  return queue;
}
