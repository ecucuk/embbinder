# Portability Guide

EmbBinder is designed to be platform-agnostic. The core logic depends only on the standard C++17 library. However, for OS-specific features like threading, synchronization, and timing, it relies on a **Porting Layer**.

## Port Interface

The porting interface is defined in `include/embbinder/port/port.hpp`. To run EmbBinder on a new platform (e.g., FreeRTOS, Zephyr, bare-metal), you must likely provide implementations for the following abstractions.

*Note: The core Binder logic is mostly synchronous and single-threaded. These porting abstractions are primarily used if you use the optional Transport or Client layers that require blocking waits or background threads.*

### 1. Clock
```cpp
struct Clock {
  static time_ms_t now_ms();
};
```
- **now_ms()**: Returns the current system time in milliseconds.

### 2. Threading (Optional)
Required only if using background listeners.
```cpp
struct Thread {
  static Result create(ThreadHandle &handle, ThreadEntry entry, void *arg, const ThreadConfig &config);
  static Result join(ThreadHandle handle);
  // ...
};
```

### 3. Synchronization
Mutexes and Semaphores for thread safety and blocking operations.
```cpp
struct Mutex { ... };
struct Semaphore { ... };
```

### 4. Transport
Abstraction for sending and receiving byte streams (UART, socket, etc.).
```cpp
struct Transport {
  static Result open(...);
  static Result send(..., const uint8_t *data, size_t size, time_ms_t timeout_ms);
  static Result receive(..., uint8_t *buffer, ...);
};
```

### 5. Memory
Wrappers for `malloc` and `free` (or OS equivalents).
```cpp
struct Memory {
  static void *alloc(size_t size);
  static void free(void *ptr);
};
```

### 6. Logging
```cpp
struct Logger {
  static void log(LogLevel level, const char *tag, const char *message);
};
```

## How to Port

1.  Create a new directory: `src/ports/my_platform/`.
2.  Implement the functions defined in `embbinder::port` namespace in a `.cpp` file.
3.  Configure your build system to compile your port file instead of the default/dummy one.

### Example: POSIX Port

If you look at `src/ports/posix/`, you will see how these are implemented using `pthread`, `usleep`, and standard socket APIs.
