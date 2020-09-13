#include "Animation.h"

#include <Windows.h>

Animation Animation::CreateAnimation(double duration_ms, bool repeat, bool up_then_down) {
  LARGE_INTEGER qpc;
  QueryPerformanceCounter(&qpc);

  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);

  Animation animation;
  animation.frequency = frequency;
  animation.startTime = qpc;
  animation.durationInMicroseconds = static_cast<uint64_t>(duration_ms * 1000);
  animation.repeat_ = repeat;
  animation.up_then_down_ = up_then_down;

  return animation;
}

float Animation::TickAnimation(const Animation& animation) {
  LARGE_INTEGER qpc;
  QueryPerformanceCounter(&qpc);

  LONGLONG delta = qpc.QuadPart - animation.startTime.QuadPart;
  LONGLONG deltaInMicroseconds = (delta * 1000000) / animation.frequency.QuadPart;

  if (deltaInMicroseconds > animation.durationInMicroseconds && !animation.repeat_) {
    return 1.f;
  } else {
    bool reverse_progress = (animation.up_then_down_)
                                ? (deltaInMicroseconds / animation.durationInMicroseconds) % 2 == 1
                                : false;
    float progress = static_cast<double>(deltaInMicroseconds % animation.durationInMicroseconds) /
                     static_cast<double>(animation.durationInMicroseconds);
    return (reverse_progress) ? 1.f - progress : progress;
  }
}
