#pragma once

#include <vector>

#include <Windows.h>

struct Animation {
  static Animation CreateAnimation(double duration_ms, bool repeat = false, bool up_then_down = false);
  static float TickAnimation(const Animation& animation);

  LARGE_INTEGER frequency;

  LARGE_INTEGER startTime;
  uint64_t durationInMicroseconds;
  bool repeat_;
  bool up_then_down_;
};
