#include "clock.h"

#include <cassert>

constexpr uint64_t nanosecondsInMilliseconds = 1000000;
constexpr uint64_t nanosecondsInSeconds = 1000000000;

void Clock::Start() {
  if (m_isRunning)
    return;

  (void)QueryPerformanceFrequency(&m_frequency);
  (void)QueryPerformanceCounter(&m_startTime);

  m_isRunning = true;
}

uint64_t Clock::GetTotalElapsedNanoseconds() {
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

double Clock::GetTotalElapsedMilliseconds() {
  return static_cast<double>(GetTotalElapsedNanoseconds()) / static_cast<double>(nanosecondsInMilliseconds);
}

double Clock::GetTotalElapsedSeconds() {
  return static_cast<double>(GetTotalElapsedNanoseconds()) / static_cast<double>(nanosecondsInSeconds);
}

void Clock::Stop() {
  m_isRunning = false;
}
