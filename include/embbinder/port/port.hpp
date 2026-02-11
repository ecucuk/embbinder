/**
 * @file port.hpp
 * @author Ergün CÜCÜK
 * @brief Port abstraction layer header file.
 * @version 0.1
 * @date 2025-12-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef EMBBINDER_PORT_PORT_HPP
#define EMBBINDER_PORT_PORT_HPP

#include <cstddef>
#include <cstdint>

namespace embbinder::port {

// Port abstraction layer functionalities would be declared here.

enum class Result : int32_t {
  SUCCESS = 0U,
  TIMEOUT,
  NO_MEMORY,
  INVALID_ARGUMENT,
  NOT_SUPPORTED,
  INTERNAL_ERROR,
  UNKNOWN_ERROR
};

using time_ms_t = uint64_t;

struct Clock {
  static time_ms_t now_ms();
};

using ThreadHandle = void *;

struct ThreadConfig {
  const char *name;
  uint32_t stack_size;
  uint32_t priority;
};

using ThreadEntry = void (*)(void *);

struct Thread {
  static Result create(ThreadHandle &handle, ThreadEntry entry, void *arg,
                       const ThreadConfig &config);
  static Result join(ThreadHandle handle);
  static Result detach(ThreadHandle handle);
};

using MutexHandle = void *;

struct Mutex {
  static Result init(MutexHandle &mutex);
  static Result destroy(MutexHandle mutex);

  static Result lock(MutexHandle mutex);
  static Result unlock(MutexHandle mutex);
};

using SemaphoreHandle = void *;

struct Semaphore {
  static Result create(void *&out, uint32_t initial_count);
  static void destroy(void *handle);

  static Result take(void *handle, time_ms_t timeout_ms);
  static void give(void *handle);
};

struct TransportConfig {
  uint32_t max_message_size;
};

using TransportHandle = void *;

struct Transport {
  static Result open(TransportHandle &out, const char *endpoint_name,
                     const TransportConfig &cfg);

  static void close(TransportHandle handle);

  static Result send(TransportHandle handle, const uint8_t *data, size_t size,
                     time_ms_t timeout_ms);

  static Result receive(TransportHandle handle, uint8_t *buffer,
                        size_t buffer_size, size_t &out_size,
                        time_ms_t timeout_ms);
};

struct Memory {
  static void *alloc(size_t size);
  static void free(void *ptr);
};

enum class LogLevel : uint8_t {
  DEBUG = 0U,
  INFO,
  WARNING,
  ERROR,
  FATAL,
  COUNT_MAX
};

using LogFunction = void (*)(LogLevel level, const char *tag,
                             const char *message);

struct Logger {
  static void set_log_function(LogFunction func);
  static void log(LogLevel level, const char *tag, const char *message);
};

} // namespace embbinder::port

#endif // EMBBINDER_PORT_PORT_HPP