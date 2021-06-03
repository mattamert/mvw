#pragma once

#include <Windows.h>
#include <cstdint>

class Timer {
private:
  bool m_isRunning = false;
  LARGE_INTEGER m_frequency;
  LARGE_INTEGER m_startTime;

public:
  void Start();
  void Stop();

  uint64_t GetTotalElapsedMicroseconds();
  double GetTotalElapsedMilliseconds();
  double GetTotalElapsedSeconds();

  static double ConvertNanosecondsToMilliseconds(uint64_t microseconds);
  static double ConvertNanosecondsToSeconds(uint64_t microseconds);
};
