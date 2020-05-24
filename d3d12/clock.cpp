#include "clock.h"

#include <cassert>

void Clock::Start() {
  if (m_isRunning)
    return;

  (void)QueryPerformanceFrequency(&m_frequency);
  (void)QueryPerformanceCounter(&m_startTime);

  m_isRunning = true;
}

uint64_t Clock::GetTotalElapsed() {
  if (!m_isRunning)
    return 0ull;

  LARGE_INTEGER qpcNow;
  QueryPerformanceCounter(&qpcNow);

  assert(qpcNow.QuadPart > m_startTime.QuadPart);
  uint64_t elapsed = qpcNow.QuadPart - m_startTime.QuadPart;

  elapsed *= 1000000;
  elapsed /= m_frequency.QuadPart;

  return elapsed;
}

void Clock::Stop() {
  m_isRunning = false;
}
