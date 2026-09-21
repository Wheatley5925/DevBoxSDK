#pragma once

#include <cstddef>
#include <cstdint>

struct DevBoxQueue;

constexpr uint32_t DEVBOX_WAIT_FOREVER = UINT32_MAX;

DevBoxQueue* devboxQueueCreate(size_t itemSize, size_t capacity);
void devboxQueueDestroy(DevBoxQueue* queue);

bool devboxQueueSend(DevBoxQueue* queue, const void* item,
                     uint32_t timeoutMs = 0);
bool devboxQueueOverwrite(DevBoxQueue* queue, const void* item);
bool devboxQueueReceive(DevBoxQueue* queue, void* item,
                        uint32_t timeoutMs = 0);
