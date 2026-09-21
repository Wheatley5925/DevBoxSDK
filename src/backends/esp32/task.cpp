#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_ESP32

#include "DevBoxTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool devboxStartTask(DevBoxTaskFunction function,
                     void* argument,
                     const char* name,
                     size_t stackSize,
                     unsigned priority,
                     int core) {
  if (!function) return false;

  const char* taskName = name ? name : "devboxTask";
  BaseType_t result;
  if (core < 0) {
    result = xTaskCreate(function, taskName, stackSize, argument,
                         priority, nullptr);
  } else {
    result = xTaskCreatePinnedToCore(function, taskName, stackSize, argument,
                                     priority, nullptr, core);
  }
  return result == pdPASS;
}

#endif // DEVBOX_TARGET_ESP32
