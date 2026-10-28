# EmbBinder

C++17 RPC/binder library for embedded systems. A request is a 24-byte `MessageHeader` + payload; `Binder` looks up an `IService` by `service_id` and calls `handle()`.

## Commands

```bash
scripts/build.sh                         # Debug build into build/, exports compile_commands.json
cmake -S . -B build && cmake --build build
(cd build && ctest --output-on-failure)  # tests/: test_binder, test_uds_endpoint (Unix)
./build/basic_service                    # prints "Client: Result = 30"
scripts/format.sh                        # clang-format -i (LLVM style, .clang-format)
scripts/lint.sh                          # clang-tidy over src/ and examples/
```

In-source builds are rejected by CMake. CI (`.github/workflows/ci.yml`) builds with `-Werror` on Ubuntu (gcc) and macOS (clang), runs ctest, and checks clang-format. Tests are plain executables using `CHECK` from `tests/check.hpp` (no framework). Use the `verifying-changes` skill before calling work done.

## Layout

- `include/embbinder/core/` — `Binder` (singleton, entry point), `ServiceRegistry` (std::map + std::mutex), `IService` (service.hpp), `Serializer` (little-endian payloads), `Result<T>`/`ErrorCode`, `Endpoint` (transport poll loop), `MessageHeader`.
- `include/embbinder/port/port.hpp` — OS abstraction: `Clock`, `Thread`, `Mutex`, `Semaphore`, `Transport`, `Memory`, `Logger`. Static functions, `void*` handles, return `port::Result`.
- `include/embbinder/utils/` — header-only `Span`, `StaticVector`; `ByteOrder`.
- `src/ports/posix/` — built as `embbinder_port_posix` on Unix (pthread + UDS transport with framing).
- `src/ports/rtos/` — FreeRTOS port, **not built** and does not compile yet.

## Things that are not obvious

- `embbinder_core` contains `Endpoint`, which calls `port::Transport`. A program using `Endpoint` must also link a port (`embbinder_port_posix`), core first.
- `port::Logger` is defined by the core (`src/core/log_port.cpp`). Ports must not define it again (duplicate symbol).
- The RTOS port (`port_rtos.cpp`) does not compile against `port.hpp` (wrong signatures, `Result::Ok`, `TimeMs`). `transport_queue.cpp` is a stub.
- `IService::handle()` receives `response_capacity`; `Binder` turns a larger `response_size` into an `Error` reply.
- Failures after the header is validated (unknown service, oversized response) produce a `MsgType::Error` reply whose payload is a uint32 LE `ErrorCode`. `Endpoint::poll` sends it and then returns the error.
- `Result<T>` converts to `bool` as "is_ok", so `if (endpoint.poll())` is true even when no message arrived. Check `.value()`.
- `Result<T>::value()` on error returns a default-constructed `T` silently — check `is_ok()` first.

## Wire format invariants

- `sizeof(MessageHeader) == 24` is a static_assert. Changing the header means: bump `kVersion`, update `docs/message_format.md`.
- `MsgType` values are wire values; add new ones at the end.
- The header is `memcpy`'d in host byte order; the payload is little-endian via `Serializer`. Don't mix the two.
- Bound checks on untrusted lengths must not overflow on 32-bit `size_t`: write `len > total - pos`, not `pos + len > total`.

## Conventions

- LLVM clang-format style (2-space indent). Run `scripts/format.sh` on touched files.
- Namespaces `embbinder::core` / `port` / `utils` (C++17 nested form). Types `PascalCase`, functions `snake_case`, members `m_`, constants `kName`.
- Header guards `EMBBINDER_<DIR>_<FILE>_HPP`; Doxygen `@file`/`@brief` block at the top of each file.
- No exceptions: core returns `Result<T>` / `ErrorCode`, ports return `port::Result`.
- Code comments in English.
