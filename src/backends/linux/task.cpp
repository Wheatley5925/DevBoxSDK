#include "DevBoxTarget.h"

#if DEVBOX_TARGET_IS_DESKTOP

#include "DevBoxTask.h"
#include <thread>

bool devboxStartTask(DevBoxTaskFunction function,
                     void* argument,
                     const char*,
                     size_t,
                     unsigned,
                     int) {
  if (!function) return false;

  try {
    std::thread(function, argument).detach();
    return true;
  } catch (...) {
    return false;
  }
}

#endif // DEVBOX_TARGET_IS_DESKTOP
