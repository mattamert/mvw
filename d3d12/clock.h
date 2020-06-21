#pragma once

#include <Windows.h>
#include <cstdint>

class Clock {
private:
  bool m_isRunning = false;
  LARGE_INTEGER m_frequency;
  LARGE_INTEGER m_startTime;
  //LARGE_INTEGER m_mostRecentCheckTime;

  //LARGE_INTEGER m_totalPauseAmount;
  //LARGE_INTEGER m_mostRecentPauseTime;
public:
  void Start();
  //void Pause();
  void Stop();

  // Returns the delta in nanoseconds.
  //LARGE_INTEGER GetElapsed();
  uint64_t GetTotalElapsed();
};