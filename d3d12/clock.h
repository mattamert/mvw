#pragma once

#include <Windows.h>
#include <cstdint>

class Clock {
private:
  bool m_isRunning = false;
  LARGE_INTEGER m_frequency;
  LARGE_INTEGER m_startTime;

public:
  void Start();
  void Stop();

  // Returns the delta in nanoseconds.
  uint64_t GetTotalElapsed();
};