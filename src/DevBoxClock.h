#pragma once

#include <stdint.h>
#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_LINUX
#include <chrono>
#include <thread>

inline uint32_t devboxMicros() {
  using Clock = std::chrono::steady_clock;
  static const auto start = Clock::now();
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start)
          .count());
}

inline void devboxDelayMicros(uint32_t microseconds) {
  if (microseconds == 0) return;
  std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}
#else
#include <Arduino.h>

inline uint32_t devboxMicros() { return micros(); }
inline void devboxDelayMicros(uint32_t microseconds) {
  delayMicroseconds(microseconds);
}
#endif
