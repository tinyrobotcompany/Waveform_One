#pragma once
#include "FreeRTOS.h"
#include <cstring>
#include <deque>
#include <vector>
#include <mutex>
struct FakeQueue {
  size_t capacity=0, itemSize=0;
  std::deque<std::vector<unsigned char>> items;
  std::mutex mutex;
};
using QueueHandle_t = FakeQueue*;
inline FakeQueue testQueue;
inline QueueHandle_t xQueueCreate(size_t capacity, size_t size) {
  testQueue.capacity=capacity; testQueue.itemSize=size; testQueue.items.clear();
  return &testQueue;
}
inline int xQueueReset(QueueHandle_t q) {
  std::lock_guard<std::mutex> lock(q->mutex); q->items.clear(); return pdTRUE;
}
inline int xQueueSend(QueueHandle_t q, const void* data, TickType_t) {
  std::lock_guard<std::mutex> lock(q->mutex);
  if(q->items.size()==q->capacity) return 0;
  const auto* p=static_cast<const unsigned char*>(data);
  q->items.emplace_back(p,p+q->itemSize); return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, void* data, TickType_t) {
  std::lock_guard<std::mutex> lock(q->mutex);
  if(q->items.empty()) return 0;
  std::memcpy(data,q->items.front().data(),q->itemSize); q->items.pop_front(); return pdTRUE;
}
