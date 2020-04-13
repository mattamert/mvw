#include "AnimationManager.h"

#include <Windows.h>

static bool is_frequency_initialized = false;
static LARGE_INTEGER frequency;

void InitializeIfNecessary() {
  if (!is_frequency_initialized) {
    QueryPerformanceFrequency(&frequency);
    is_frequency_initialized = true;
  }
}

Animation AnimationManager::CreateAnimation(double duration_ms, bool repeat, bool up_then_down) {
  InitializeIfNecessary();

  LARGE_INTEGER qpc;
  QueryPerformanceCounter(&qpc);

  double now = static_cast<double>(qpc.QuadPart);
  now /= frequency.QuadPart;
  now *= 1000000;
  
  Animation animation;
  animation.start_time_us_ = static_cast<uint64_t>(now);
  animation.duration_us_ = static_cast<uint64_t>(duration_ms * 1000);
  animation.repeat_ = repeat;
  animation.up_then_down_ = up_then_down;

  return animation;
}

float AnimationManager::TickAnimation(const Animation& animation) {
  InitializeIfNecessary();

  LARGE_INTEGER qpc;
  QueryPerformanceCounter(&qpc);

  double now = static_cast<double>(qpc.QuadPart);
  now /= static_cast<double>(frequency.QuadPart);
  now *= 1000000;

  uint64_t now_us = static_cast<uint64_t>(now);
  uint64_t delta = now_us - animation.start_time_us_;
  
  if (delta > animation.duration_us_ && !animation.repeat_) {
    return 1.f;
  } else {
    bool reverse_progress = (animation.up_then_down_) ? (delta / animation.duration_us_) % 2 == 1 : false;
    float progress = static_cast<double>(delta % animation.duration_us_) / static_cast<double>(animation.duration_us_);
    return (reverse_progress) ? 1.f - progress : progress;
  }
}