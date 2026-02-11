/**
 * @file port_posix.cpp
 * @author Ergün CÜCÜK
 * @brief POSIX implementation of the port abstraction layer.
 */

#include "embbinder/port/port.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

namespace embbinder::port {

// --- Clock ---

time_ms_t Clock::now_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (static_cast<time_ms_t>(ts.tv_sec) * 1000) +
         (static_cast<time_ms_t>(ts.tv_nsec) / 1000000);
}

// --- Memory ---

void *Memory::alloc(size_t size) { return std::malloc(size); }

void Memory::free(void *ptr) { std::free(ptr); }

// --- Logger ---

static LogFunction g_log_function = nullptr;

void Logger::set_log_function(LogFunction func) { g_log_function = func; }

void Logger::log(LogLevel level, const char *tag, const char *message) {
  if (g_log_function) {
    g_log_function(level, tag, message);
  } else {
    // Default implementation
    const char *level_str = "UNK";
    switch (level) {
    case LogLevel::DEBUG:
      level_str = "DBG";
      break;
    case LogLevel::INFO:
      level_str = "INF";
      break;
    case LogLevel::WARNING:
      level_str = "WRN";
      break;
    case LogLevel::ERROR:
      level_str = "ERR";
      break;
    case LogLevel::FATAL:
      level_str = "FTL";
      break;
    default:
      break;
    }
    std::printf("[%s] [%s] %s\n", level_str, tag, message);
  }
}

// --- Mutex ---

Result Mutex::init(MutexHandle &mutex) {
  pthread_mutex_t *m = new pthread_mutex_t;
  if (pthread_mutex_init(m, nullptr) == 0) {
    mutex = m;
    return Result::SUCCESS;
  }
  delete m;
  return Result::INTERNAL_ERROR;
}

Result Mutex::destroy(MutexHandle mutex) {
  if (!mutex)
    return Result::INVALID_ARGUMENT;
  pthread_mutex_t *m = static_cast<pthread_mutex_t *>(mutex);
  pthread_mutex_destroy(m);
  delete m;
  return Result::SUCCESS;
}

Result Mutex::lock(MutexHandle mutex) {
  if (!mutex)
    return Result::INVALID_ARGUMENT;
  pthread_mutex_t *m = static_cast<pthread_mutex_t *>(mutex);
  if (pthread_mutex_lock(m) == 0) {
    return Result::SUCCESS;
  }
  return Result::INTERNAL_ERROR;
}

Result Mutex::unlock(MutexHandle mutex) {
  if (!mutex)
    return Result::INVALID_ARGUMENT;
  pthread_mutex_t *m = static_cast<pthread_mutex_t *>(mutex);
  if (pthread_mutex_unlock(m) == 0) {
    return Result::SUCCESS;
  }
  return Result::INTERNAL_ERROR;
}

// --- Semaphore ---

struct PosixSemaphore {
#ifdef __APPLE__
  // macOS doesn't support unnamed semaphores (sem_init) well, using
  // dispatch_semaphore or named semaphores is safer. However, for
  // simplicity/standard POSIX compliance, we might use named semaphores or
  // emulate with condition variables. Here we use a condition variable
  // emulation for portability on macOS/Linux.
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  uint32_t count;
#else
  sem_t sem;
#endif
};

Result Semaphore::create(void *&out, uint32_t initial_count) {
  PosixSemaphore *s = new PosixSemaphore;
#ifdef __APPLE__
  pthread_mutex_init(&s->mutex, nullptr);
  pthread_cond_init(&s->cond, nullptr);
  s->count = initial_count;
#else
  if (sem_init(&s->sem, 0, initial_count) != 0) {
    delete s;
    return Result::INTERNAL_ERROR;
  }
#endif
  out = s;
  return Result::SUCCESS;
}

void Semaphore::destroy(void *handle) {
  if (!handle)
    return;
  PosixSemaphore *s = static_cast<PosixSemaphore *>(handle);
#ifdef __APPLE__
  pthread_mutex_destroy(&s->mutex);
  pthread_cond_destroy(&s->cond);
#else
  sem_destroy(&s->sem);
#endif
  delete s;
}

Result Semaphore::take(void *handle, time_ms_t timeout_ms) {
  if (!handle)
    return Result::INVALID_ARGUMENT;
  PosixSemaphore *s = static_cast<PosixSemaphore *>(handle);

#ifdef __APPLE__
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  // Add timeout
  ts.tv_sec += timeout_ms / 1000;
  ts.tv_nsec += (timeout_ms % 1000) * 1000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }

  pthread_mutex_lock(&s->mutex);
  while (s->count == 0) {
    int res = pthread_cond_timedwait(&s->cond, &s->mutex, &ts);
    if (res == ETIMEDOUT) {
      pthread_mutex_unlock(&s->mutex);
      return Result::TIMEOUT;
    } else if (res != 0) {
      pthread_mutex_unlock(&s->mutex);
      return Result::INTERNAL_ERROR;
    }
  }
  s->count--;
  pthread_mutex_unlock(&s->mutex);
  return Result::SUCCESS;

#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout_ms / 1000;
  ts.tv_nsec += (timeout_ms % 1000) * 1000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }

  if (sem_timedwait(&s->sem, &ts) == 0) {
    return Result::SUCCESS;
  }
  if (errno == ETIMEDOUT) {
    return Result::TIMEOUT;
  }
  return Result::INTERNAL_ERROR;
#endif
}

void Semaphore::give(void *handle) {
  if (!handle)
    return;
  PosixSemaphore *s = static_cast<PosixSemaphore *>(handle);
#ifdef __APPLE__
  pthread_mutex_lock(&s->mutex);
  s->count++;
  pthread_cond_signal(&s->cond);
  pthread_mutex_unlock(&s->mutex);
#else
  sem_post(&s->sem);
#endif
}

// --- Thread ---

struct ThreadContext {
  ThreadEntry entry;
  void *arg;
};

static void *thread_start_routine(void *arg) {
  ThreadContext *ctx = static_cast<ThreadContext *>(arg);
  ctx->entry(ctx->arg);
  delete ctx;
  return nullptr;
}

Result Thread::create(ThreadHandle &handle, ThreadEntry entry, void *arg,
                      const ThreadConfig &config) {
  pthread_t thread;
  ThreadContext *ctx = new ThreadContext{entry, arg};

  // Attributes (stack size, logging name if supported)
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  if (config.stack_size > 0) {
    pthread_attr_setstacksize(&attr, config.stack_size);
  }

  if (pthread_create(&thread, &attr, thread_start_routine, ctx) == 0) {
    handle = reinterpret_cast<ThreadHandle>(thread);
    pthread_attr_destroy(&attr);
    return Result::SUCCESS;
  }

  pthread_attr_destroy(&attr);
  delete ctx;
  return Result::INTERNAL_ERROR;
}

Result Thread::join(ThreadHandle handle) {
  if (!handle)
    return Result::INVALID_ARGUMENT;
  pthread_t thread = reinterpret_cast<pthread_t>(handle);
  if (pthread_join(thread, nullptr) == 0) {
    return Result::SUCCESS;
  }
  return Result::INTERNAL_ERROR;
}

Result Thread::detach(ThreadHandle handle) {
  if (!handle)
    return Result::INVALID_ARGUMENT;
  pthread_t thread = reinterpret_cast<pthread_t>(handle);
  if (pthread_detach(thread) == 0) {
    return Result::SUCCESS;
  }
  return Result::INTERNAL_ERROR;
}

} // namespace embbinder::port
