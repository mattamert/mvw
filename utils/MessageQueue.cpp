#include "utils/MessageQueue.h"

void MessageQueue::Push(MSG message) {
  m_mut.lock();
  m_messageQueue.push(message);
  m_mut.unlock();
}

std::queue<MSG> MessageQueue::Flush() {
  std::queue<MSG> queue;
  m_mut.lock();
  m_messageQueue.swap(queue);
  m_mut.unlock();
  return queue;
}
