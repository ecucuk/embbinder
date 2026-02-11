/**
 * @file port_rtos.cpp
 * @author Ergün CÜCÜK
 * @brief
 * @version 0.1
 * @date 2025-12-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "embbinder/port/port.hpp"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <cstring>
#include <new>

namespace embbinder::port {

/**
 * @brief Current time return in milliseconds.
 *
 * @return time_ms_t
 */
time_ms_t Clock::now_ms() {
  TickType_t tim = xTaskGetTickCount();
  return static_cast<time_ms_t>((tim * 1000ULL) / configTICK_RATE_HZ);
}

/**
 * @brief Create a new thread.
 *
 * @param out
 * @param entry
 * @param arg
 * @param cfg
 * @return Result
 */
Result Thread::create(ThreadHandle &out, ThreadEntry entry, void *arg,
                      const ThreadConfig &cfg) {
  if (!entry)
    return Result::INVALID_ARGUMENT;

  BaseType_t ok = xTaskCreate(
      [](void *p) {
        auto *pack = static_cast<void **>(p);
        auto fn = reinterpret_cast<ThreadEntry>(pack[0]);
        void *a = pack[1];
        vPortFree(pack);
        fn(a);
        vTaskDelete(nullptr);
      },
      (cfg.name ? cfg.name : "embb"),
      static_cast<uint16_t>(cfg.stack_size / sizeof(StackType_t)), nullptr,
      static_cast<UBaseType_t>(cfg.priority),
      reinterpret_cast<TaskHandle_t *>(&out));

  if (ok == pdPASS) {
    void **pack = static_cast<void **>(pvPortMalloc(2 * sizeof(void *)));
    if (!pack)
      return Result::NO_MEMORY;
    pack[0] = reinterpret_cast<void *>(entry);
    pack[1] = arg;

    vPortFree(pack);
    return Result::NOT_SUPPORTED;
  }
  return Result::INTERNAL_ERROR;
}

void Thread::join(ThreadHandle) {
  // FreeRTOS join concept 없음 (MVP: no-op)
}

Result Mutex::create(void *&out) {
  auto h = xSemaphoreCreateMutex();
  if (!h)
    return Result::NO_MEMORY;
  out = h;
  return Result::Ok;
}

void Mutex::destroy(void *handle) {
  if (handle)
    vSemaphoreDelete(static_cast<SemaphoreHandle_t>(handle));
}

void Mutex::lock(void *handle) {
  xSemaphoreTake(static_cast<SemaphoreHandle_t>(handle), portMAX_DELAY);
}

void Mutex::unlock(void *handle) {
  xSemaphoreGive(static_cast<SemaphoreHandle_t>(handle));
}

Result Semaphore::create(void *&out, uint32_t initial_count) {
  auto h = xSemaphoreCreateCounting(0xFFFFu, initial_count);
  if (!h)
    return Result::NO_MEMORY;
  out = h;
  return Result::SUCCESS;
}

void Semaphore::destroy(void *handle) {
  if (handle)
    vSemaphoreDelete(static_cast<SemaphoreHandle_t>(handle));
}

Result Semaphore::take(void *handle, TimeMs timeout_ms) {
  TickType_t to = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
  auto ok = xSemaphoreTake(static_cast<SemaphoreHandle_t>(handle), to);
  return (ok == pdTRUE) ? Result::SUCCESS : Result::TIMEOUT;
}

void Semaphore::give(void *handle) {
  xSemaphoreGive(static_cast<SemaphoreHandle_t>(handle));
}

void *Memory::alloc(size_t size) { return pvPortMalloc(size); }

void Memory::free(void *ptr) { vPortFree(ptr); }

} // namespace embbinder::port