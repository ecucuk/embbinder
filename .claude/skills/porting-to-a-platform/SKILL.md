---
name: porting-to-a-platform
description: Use when implementing or fixing an EmbBinder port (POSIX, FreeRTOS, Zephyr, bare metal), a Transport (UART, socket, queue), or wiring a port into the CMake build.
---

# Porting EmbBinder

`include/embbinder/port/port.hpp` is the contract. A port is a set of `.cpp` files that define those static functions for one platform. Exactly one port is linked per binary.

## Contract checklist

- Match `port.hpp` signatures exactly, including return types (`Mutex::lock` returns `Result`, `Thread::join` returns `Result`, `Mutex::init` not `create`). Enum is `Result::SUCCESS`, time type is `time_ms_t`.
- Define every function, or the link fails only for programs that call the missing one.
- `Logger` already lives in `src/core/log_port.cpp`. Do not define it again in the port unless you remove the core one.
- Handles are `void*`. Own the allocation in `open`/`create`/`init`, free it in `close`/`destroy`.
- Map timeouts: `Transport::receive` returns `Result::TIMEOUT` (not an error) when nothing arrived; `Endpoint::poll` relies on that.

## Transport must deliver whole messages

`Binder::process_message` expects one complete message per buffer. Stream transports (UART, TCP) need framing: read 24 header bytes, then `payload_size` bytes, and drop the connection when the size exceeds the buffer (you can't resync). A server transport must `accept()` before `recv()`. `src/ports/posix/transport_uds.cpp` is the reference: `poll()` for timeouts, `read_exact`, peer loss → back to accepting.

Never `unlink`/delete whatever sits at the endpoint path; check it is a socket first.

## Wiring into CMake

Follow `embbinder_port_posix` in the root `CMakeLists.txt`: its own library, `target_include_directories(... PUBLIC <port>/include)`, guarded by a condition (the RTOS port needs FreeRTOS headers). Consumers link `embbinder_core` first, then the port.

## Done

Add a test like `tests/test_uds_endpoint.cpp` and follow the `verifying-changes` skill.
