#pragma once

#include <Windows.h>
#include <vector>

struct Animation {
  uint64_t start_time_us_;
  uint64_t duration_us_;
  bool repeat_;
  bool up_then_down_;
};

namespace AnimationManager {
Animation CreateAnimation(double duration_ms, bool repeat = false, bool up_then_down = false);
float TickAnimation(const Animation& animation);
};
