#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_LINUX

#include "DevBoxQueue.h"
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <new>
#include <vector>

struct DevBoxQueue {
  size_t itemSize;
  size_t capacity;
  std::deque<std::vector<unsigned char>> items;
  std::mutex mutex;
  std::condition_variable canRead;
  std::condition_variable canWrite;
};

namespace {
template <typename Predicate>
bool waitFor(std::condition_variable& condition,
             std::unique_lock<std::mutex>& lock,
             uint32_t timeoutMs, Predicate predicate) {
  if (timeoutMs == 0) return predicate();
  if (timeoutMs == DEVBOX_WAIT_FOREVER) {
    condition.wait(lock, predicate);
    return true;
  }
  return condition.wait_for(lock, std::chrono::milliseconds(timeoutMs), predicate);
}
}

DevBoxQueue* devboxQueueCreate(size_t itemSize, size_t capacity) {
  if (itemSize == 0 || capacity == 0) return nullptr;
  return new (std::nothrow) DevBoxQueue{itemSize, capacity};
}

void devboxQueueDestroy(DevBoxQueue* queue) {
  delete queue;
}

bool devboxQueueSend(DevBoxQueue* queue, const void* item, uint32_t timeoutMs) {
  if (!queue || !item) return false;
  std::unique_lock<std::mutex> lock(queue->mutex);
  if (!waitFor(queue->canWrite, lock, timeoutMs, [&] {
        return queue->items.size() < queue->capacity;
      })) return false;

  queue->items.emplace_back(queue->itemSize);
  std::memcpy(queue->items.back().data(), item, queue->itemSize);
  lock.unlock();
  queue->canRead.notify_one();
  return true;
}

bool devboxQueueOverwrite(DevBoxQueue* queue, const void* item) {
  if (!queue || !item || queue->capacity != 1) return false;
  std::lock_guard<std::mutex> lock(queue->mutex);
  if (queue->items.empty()) queue->items.emplace_back(queue->itemSize);
  std::memcpy(queue->items.front().data(), item, queue->itemSize);
  queue->canRead.notify_one();
  return true;
}

bool devboxQueueReceive(DevBoxQueue* queue, void* item, uint32_t timeoutMs) {
  if (!queue || !item) return false;
  std::unique_lock<std::mutex> lock(queue->mutex);
  if (!waitFor(queue->canRead, lock, timeoutMs, [&] {
        return !queue->items.empty();
      })) return false;

  std::memcpy(item, queue->items.front().data(), queue->itemSize);
  queue->items.pop_front();
  lock.unlock();
  queue->canWrite.notify_one();
  return true;
}

#endif // DEVBOX_TARGET_LINUX
