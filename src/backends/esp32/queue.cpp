#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_ESP32

#include "DevBoxQueue.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <new>

struct DevBoxQueue {
  QueueHandle_t handle;
  size_t capacity;
};

namespace {
TickType_t timeoutTicks(uint32_t timeoutMs) {
  if (timeoutMs == DEVBOX_WAIT_FOREVER) return portMAX_DELAY;
  return pdMS_TO_TICKS(timeoutMs);
}
}

DevBoxQueue* devboxQueueCreate(size_t itemSize, size_t capacity) {
  if (itemSize == 0 || capacity == 0) return nullptr;
  DevBoxQueue* queue = new (std::nothrow) DevBoxQueue{};
  if (!queue) return nullptr;
  queue->handle = xQueueCreate(capacity, itemSize);
  queue->capacity = capacity;
  if (!queue->handle) {
    delete queue;
    return nullptr;
  }
  return queue;
}

void devboxQueueDestroy(DevBoxQueue* queue) {
  if (!queue) return;
  vQueueDelete(queue->handle);
  delete queue;
}

bool devboxQueueSend(DevBoxQueue* queue, const void* item, uint32_t timeoutMs) {
  return queue && item &&
         xQueueSend(queue->handle, item, timeoutTicks(timeoutMs)) == pdTRUE;
}

bool devboxQueueOverwrite(DevBoxQueue* queue, const void* item) {
  return queue && item && queue->capacity == 1 &&
         xQueueOverwrite(queue->handle, item) == pdTRUE;
}

bool devboxQueueReceive(DevBoxQueue* queue, void* item, uint32_t timeoutMs) {
  return queue && item &&
         xQueueReceive(queue->handle, item, timeoutTicks(timeoutMs)) == pdTRUE;
}

#endif // DEVBOX_TARGET_ESP32
