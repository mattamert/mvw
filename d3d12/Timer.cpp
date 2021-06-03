#include "d3d12/Timer.h"

#include <cassert>

constexpr uint64_t nanosecondsInMilliseconds = 1000;
constexpr uint64_t nanosecondsInSeconds = 1000000;

void Timer::Start() {
  if (m_isRunning)
    return;

  (void)QueryPerformanceFrequency(&m_frequency);
  (void)QueryPerformanceCounter(&m_startTime);

  m_isRunning = true;
}

uint64_t Timer::GetTotalElapsedMicroseconds() {
  if (!m_isRunning)
    return 0ull;

  LARGE_INTEGER qpcNow;
  QueryPerformanceCounter(&qpcNow);

  assert(qpcNow.QuadPart > m_startTime.QuadPart);
  uint64_t elapsed = qpcNow.QuadPart - m_startTime.QuadPart;

  // Multiply first to prevent loss of precision.
  // Note, however, that this means that the program may overflow the uint64_t here after a certain
  // amount of time. (The last I calculated it, it was around 10 days on the machine I was using at
  // the time).
  elapsed *= 1000000;
  elapsed /= m_frequency.QuadPart;

  return elapsed;
}

double Timer::GetTotalElapsedMilliseconds() {
  return static_cast<double>(GetTotalElapsedMicroseconds()) /
    static_cast<double>(nanosecondsInMilliseconds);
}

double Timer::GetTotalElapsedSeconds() {
  return Timer::ConvertNanosecondsToSeconds(GetTotalElapsedMicroseconds());
}

double Timer::ConvertNanosecondsToMilliseconds(uint64_t nanoseconds) {
  return static_cast<double>(nanoseconds) / static_cast<double>(nanosecondsInMilliseconds);
}

double Timer::ConvertNanosecondsToSeconds(uint64_t nanoseconds) {
  return static_cast<double>(nanoseconds) / static_cast<double>(nanosecondsInSeconds);
}

void Timer::Stop() {
  m_isRunning = false;
}
