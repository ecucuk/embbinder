# EmbBinder Rust Conversion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the C++17 library with a `no_std`, heap-free Rust crate that keeps the wire format.

**Architecture:** One crate. `Binder` is a value that the application owns, with `N` fixed service slots. `Endpoint` joins a `Transport` trait to a `Binder`. `UdsTransport` is the only module that needs `std`.

**Tech Stack:** Rust edition 2024 (toolchain 1.93), `log` 0.4, `libc` 0.2 (optional).

**Spec:** `docs/superpowers/specs/2026-11-06-rust-conversion-design.md`

## Global Constraints

- The crate root has `#![cfg_attr(not(feature = "std"), no_std)]` and `#![deny(unsafe_code)]`.
- `unsafe` occurs in one function only: the `libc::poll` wrapper in `src/uds.rs`, with `#[allow(unsafe_code)]`.
- The core does not use `alloc`.
- Features: `default = ["std"]`, `std = ["dep:libc"]`. `log` has `default-features = false`.
- Library code must not panic on input bytes: no `unwrap`, no unchecked slice index on untrusted lengths.
- Bound checks use `len > total - pos`, not `pos + len > total`.
- Do not remove C++ code before the interop run in Task 6 passes.
- `cargo clippy --all-targets -- -D warnings` and `cargo fmt --check` pass after each task.
- Code comments are in English.
- Commits use Conventional Commits and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Do not push.

## Review Focus

1. Corrupt or truncated bytes at `process_message`: each prefix of a valid frame gives `Err` and no panic, because a panic resets an MCU.
2. A service that returns a very large size such as `usize::MAX`: the client gets an `InternalError` reply and the slice operation does not panic.
3. A client that closes before it reads the reply: `poll` gives `Err(InternalError)`, the process lives, and the next client gets service.
4. A client that closes or stalls in the middle of a frame: the server does not stop, and the next client gets service.
5. Bad endpoint paths (too long, NUL byte, a stale socket file, a regular file): `open` gives a clear error or replaces only the stale socket.

---

### Task 1: Crate, wire types and golden bytes

**Files:**
- Create: `Cargo.toml`, `src/lib.rs`, `src/error.rs`, `src/message.rs`, `tests/wire_format.rs`
- Modify: `.gitignore` (add `/target`)
- Add to the commit: `docs/superpowers/specs/2026-11-06-rust-conversion-design.md`, `docs/superpowers/plans/2026-11-06-rust-conversion.md`

**Interfaces:**
- Produces:
  - `embbinder::ErrorCode`: `#[repr(u32)]`, derives `Debug, Clone, Copy, PartialEq, Eq`, values from the spec table.
  - `embbinder::{MAGIC: u32, VERSION: u16}`.
  - `embbinder::MsgType`: `#[repr(u16)]`, `Request = 1, Response = 2, Event = 3, Error = 4`.
  - `embbinder::MessageHeader`: public fields `magic: u32, version: u16, msg_type: u16, service_id: u32, method_id: u32, payload_size: u32, request_id: u32`, derives `Debug, Clone, Copy, PartialEq, Eq`.
  - `MessageHeader::SIZE: usize = 24`, `MessageHeader::from_bytes(&[u8; 24]) -> Self`, `MessageHeader::to_bytes(&self) -> [u8; 24]`.

- [ ] **Step 1: Get the golden bytes from the C++ build**

Build the C++ library in a temporary directory:

```bash
B=$(mktemp -d) && cmake -S . -B "$B" >/dev/null && cmake --build "$B" -j8 --target embbinder_core >/dev/null && echo "$B"
```

In the same temporary directory, write and compile a small C++ program that links `embbinder_core`, runs the `basic_service` request (`service_id` 100, method 1, `request_id` 1, payload 10 and 20) and an unknown service request (`service_id` 999, method 1, `request_id` 42), and prints the request and the two replies as hex. Compare the output with the constants in Step 2. If they differ, the C++ output is correct: use it and stop to report the difference.

- [ ] **Step 2: Write the failing test `tests/wire_format.rs`**

```rust
const REQUEST: [u8; 32] = [
    0x42, 0x42, 0x4D, 0x45, 0x01, 0x00, 0x01, 0x00, 0x64, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
];
const RESPONSE: [u8; 28] = [
    0x42, 0x42, 0x4D, 0x45, 0x01, 0x00, 0x02, 0x00, 0x64, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x1E, 0x00, 0x00, 0x00,
];
const ERROR_REPLY: [u8; 28] = [
    0x42, 0x42, 0x4D, 0x45, 0x01, 0x00, 0x04, 0x00, 0xE7, 0x03, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00,
];
```

Tests in this task:

- `header_round_trip`: `MessageHeader::from_bytes(first 24 bytes of REQUEST)` has `magic == MAGIC`, `version == 1`, `msg_type == 1`, `service_id == 100`, `method_id == 1`, `payload_size == 8`, `request_id == 1`, and `to_bytes` gives the same 24 bytes.
- `error_code_wire_values`: `Unknown as u32 == 1`, `ServiceNotFound as u32 == 5`, `InvalidOperation as u32 == 10`.

Task 3 adds the `RESPONSE` and `ERROR_REPLY` tests to this file.

- [ ] **Step 3: Run the test to make sure it fails**

Run: `cargo test --test wire_format`
Expected: FAIL. The compiler reports that the items do not exist. First create `Cargo.toml` and an empty `src/lib.rs` if this step needs them.

- [ ] **Step 4: Write `Cargo.toml`, `src/lib.rs`, `src/error.rs`, `src/message.rs`**

`Cargo.toml` has `name = "embbinder"`, `version = "0.1.0"`, `edition = "2024"`, `license-file = "LICENSE"`, the features and dependencies from Global Constraints. `src/lib.rs` re-exports public items at the crate root. Commit `Cargo.lock`.

All integer fields use `to_le_bytes` and `from_le_bytes`.

- [ ] **Step 5: Run the test to make sure it passes**

Run: `cargo test --test wire_format`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add Cargo.toml Cargo.lock .gitignore src tests/wire_format.rs docs/superpowers
git commit -m "feat: add Rust crate with wire types"
```

### Task 2: Writer and Reader

**Files:**
- Create: `src/serializer.rs`
- Modify: `src/lib.rs`
- Test: `tests/serializer.rs`

**Interfaces:**
- Consumes: `ErrorCode`.
- Produces:
  - `Writer::new(buf: &'a mut [u8]) -> Self`, `len(&self) -> usize`, `is_empty(&self) -> bool`.
  - `Writer::{write_i32, write_u32, write_i64, write_u64, write_f32, write_f64, write_bool}`, `write_string(&mut self, &str)`, `write_bytes(&mut self, &[u8])`: all return `Result<(), ErrorCode>`.
  - `Reader::new(data: &'a [u8]) -> Self`.
  - `Reader::{read_i32, read_u32, read_i64, read_u64, read_f32, read_f64, read_bool}`: return `Result<T, ErrorCode>`.
  - `Reader::read_string(&mut self) -> Result<&'a str, ErrorCode>`.

- [ ] **Step 1: Write the failing tests**

- `writer_formats_match_wire`: `write_i32(10)`, `write_string("hi")`, `write_bool(true)`, `write_f32(1.5)`, `write_f64(1.5)`, `write_i64(-1)` give these bytes in sequence: `0A 00 00 00`, `02 00 00 00 68 69`, `01`, `00 00 C0 3F`, `00 00 00 00 00 00 F8 3F`, `FF FF FF FF FF FF FF FF`.
- `reader_reads_writer_output`: reads each value back, and `read_string` returns `"hi"`.
- `write_past_end_gives_buffer_too_small`: 3-byte buffer, `write_u32(1)` gives `Err(BufferTooSmall)`. A 5-byte buffer with `write_string("hi")` gives the same error.
- `oversized_string_length_is_rejected`: bytes `FF FF FF FF 78`, `read_string` gives `Err(BufferTooSmall)`, and a next `read_u32` gives `Err` with no panic.
- `read_past_end_gives_buffer_too_small`: 3 bytes, `read_i32` gives `Err(BufferTooSmall)`.
- `invalid_utf8_gives_deserialization_error`: bytes `01 00 00 00 FF`, `read_string` gives `Err(DeserializationError)`.
- `read_bool_treats_nonzero_as_true`: bytes `02` give `Ok(true)`.

- [ ] **Step 2: Run the tests to make sure they fail**

Run: `cargo test --test serializer`
Expected: FAIL. `Writer` and `Reader` do not exist.

- [ ] **Step 3: Write `src/serializer.rs`**

`Writer` has one private `put(&[u8])`. `Reader` has one private `take(n) -> Result<&'a [u8], ErrorCode>`. Both check with `n > total - pos`. `write_string` gives `SerializationError` if the length does not fit in `u32`. `read_string` consumes the length first, as C++ does. Convert bytes with `core::str::from_utf8`.

- [ ] **Step 4: Run the tests to make sure they pass**

Run: `cargo test --test serializer`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src tests/serializer.rs
git commit -m "feat: add Writer and Reader"
```

### Task 3: Binder

**Files:**
- Create: `src/binder.rs`
- Modify: `src/lib.rs`, `tests/wire_format.rs`
- Test: `tests/binder.rs`

**Interfaces:**
- Consumes: `MessageHeader`, `MsgType`, `MAGIC`, `VERSION`, `ErrorCode`.
- Produces: `Service`, `ProcessError { code, reply_size }` (derives `Debug, Clone, Copy, PartialEq, Eq`), `Binder<'a, const N: usize>` with the spec signatures, plus `impl Default`.

- [ ] **Step 1: Write the failing tests in `tests/binder.rs`**

Use a `FillService { seen_capacity: usize, claim: usize, lie: bool, fail: Option<ErrorCode> }`. If `lie` is false, it fills `claim` bytes with `0xAB`. `register_service` holds a `&mut` borrow, so read `seen_capacity` after the `Binder` goes out of scope.

- `normal_request_gives_response`: output 32 bytes, `claim` 4. Result is `Ok(28)`, `seen_capacity == 8`, reply `msg_type == Response as u16`, `request_id == 42`.
- `oversized_claim_gives_internal_error`: `lie`, `claim` 9. Result is `Err(ProcessError { code: InternalError, reply_size: 28 })`, reply type is `Error`.
- `usize_max_claim_gives_internal_error`: as above with `claim = usize::MAX`.
- `service_error_gives_error_reply`: `fail = Some(MethodNotFound)`. Result code is `MethodNotFound`, `reply_size` 28, the last 4 bytes are `06 00 00 00`.
- `unknown_service_gives_error_reply`: `service_id` 999. Code is `ServiceNotFound`, reply type is `Error`, `request_id == 42`, `service_id == 999`, `payload_size == 4`, and the payload reads back as 5.
- `huge_payload_size_is_rejected`: header claims `0xFFFF_FFF0`, no payload. Result is `ProcessError { code: BufferTooSmall, reply_size: 0 }`.
- `short_input_is_rejected`: 23 bytes give `ProcessError { code: InvalidArgument, reply_size: 0 }`.
- `bad_magic_is_rejected`: same result as `short_input_is_rejected`.
- `small_output_is_rejected`: 23-byte output with a registered service gives `ProcessError { code: BufferTooSmall, reply_size: 0 }`.
- `error_reply_needs_28_bytes`: 27-byte output and service 999 give `ProcessError { code: ServiceNotFound, reply_size: 0 }`.
- `every_request_prefix_is_rejected`: for each `len` in `0..frame.len()`, `process_message(&frame[..len], ..)` gives `Err`.
- `register_rejects_duplicate_and_full`: `Binder<'_, 1>` gives `Ok`, then `Err(InvalidOperation)` for the same ID, then `Err(OutOfMemory)` for a new ID.
- `zero_slot_binder_finds_no_service`: `Binder<'_, 0>` gives `Err(OutOfMemory)` on register and `ServiceNotFound` on process.
- `binder_is_send`: compile-time check with `fn assert_send<T: Send>() {}` for `Binder<'static, 2>`.

Add to `tests/wire_format.rs`:

- `response_matches_cpp`: an add service on ID 100 with `REQUEST` as input gives exactly `RESPONSE`.
- `error_reply_matches_cpp`: an unknown service request gives exactly `ERROR_REPLY`.

- [ ] **Step 2: Run the tests to make sure they fail**

Run: `cargo test --test binder --test wire_format`
Expected: FAIL. `Binder` does not exist.

- [ ] **Step 3: Write `src/binder.rs`**

Slots are `[Option<(u32, &'a mut dyn Service)>; N]`, created with `core::array::from_fn`. Use `input.first_chunk::<24>()` for the header. The processing order is the flowchart in the spec. Take the capacity as `min(output.len() - 24, u32::MAX as usize)`. Check `n > capacity` before any slice operation on `n`.

Write the reply header with `split_at_mut(MessageHeader::SIZE)`.

- [ ] **Step 4: Run the tests to make sure they pass**

Run: `cargo test --test binder --test wire_format`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src tests/binder.rs tests/wire_format.rs
git commit -m "feat: add Binder and Service"
```

### Task 4: Transport trait and Endpoint

**Files:**
- Create: `src/transport.rs`, `src/endpoint.rs`
- Modify: `src/lib.rs`
- Test: `tests/endpoint.rs`

**Interfaces:**
- Consumes: `Binder`, `ProcessError`, `ErrorCode`.
- Produces: `Transport`, `TransportError { Timeout, NoMemory, InvalidArgument, Internal }` (derives `Debug, Clone, Copy, PartialEq, Eq`), `Endpoint<T: Transport, const M: usize>` with `new` and `poll` from the spec.

- [ ] **Step 1: Write the failing tests**

Use a mock transport with `Rc<RefCell<..>>` state: a queue of `receive` results, a list of sent frames and an optional send error.

- `timeout_gives_false`: `receive` gives `Err(Timeout)`. `poll` gives `Ok(false)`.
- `empty_receive_gives_false`: `receive` gives `Ok(0)`. `poll` gives `Ok(false)`.
- `request_sends_reply`: valid frame. `poll` gives `Ok(true)` and one frame was sent, with type `Response`.
- `unknown_service_sends_reply_then_errors`: `poll` gives `Err(ServiceNotFound)` and one frame was sent, with type `Error`.
- `malformed_frame_sends_nothing`: bad magic. `poll` gives `Err(InvalidArgument)` and no frame was sent.
- `receive_failure_gives_internal_error`: `receive` gives `Err(NoMemory)`. `poll` gives `Err(InternalError)`.
- `send_failure_gives_internal_error`: send error is set. `poll` gives `Err(InternalError)`.
- `send_uses_one_second_timeout`: the mock records `timeout_ms == 1000`.

- [ ] **Step 2: Run the tests to make sure they fail**

Run: `cargo test --test endpoint`
Expected: FAIL. `Endpoint` does not exist.

- [ ] **Step 3: Write `src/transport.rs` and `src/endpoint.rs`**

`Endpoint` owns `transport: T`, `rx: [u8; M]` and `tx: [u8; M]`. `new` has `const { assert!(M >= MessageHeader::SIZE) }`. `poll` follows the four steps in the spec. Log with `log::warn!` on receive errors and `log::error!` on send errors, as C++ does.

- [ ] **Step 4: Run the tests to make sure they pass**

Run: `cargo test --test endpoint`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src tests/endpoint.rs
git commit -m "feat: add Transport trait and Endpoint"
```

### Task 5: UdsTransport

**Files:**
- Create: `src/uds.rs`
- Modify: `src/lib.rs`
- Test: `tests/uds_endpoint.rs`

**Interfaces:**
- Consumes: `Transport`, `TransportError`, `MessageHeader`, `Endpoint`, `Binder`.
- Produces: `UdsTransport::open(path: &str) -> Result<Self, TransportError>`, only with `#[cfg(all(feature = "std", unix))]`.

- [ ] **Step 1: Write the failing tests**

Start the file with `#![cfg(all(feature = "std", unix))]`. Use `AddService` from `test_uds_endpoint.cpp`. Each test uses its own path, `/tmp/embb_<test>_<pid>.sock`, and removes it at the end. Do not use the working directory: the repository path can be too long for `sun_path`. Tests use `Endpoint<UdsTransport, 256>` as the server and a second `UdsTransport` as the client.

- `poll_without_client_times_out`: `poll(.., 0)` gives `Ok(false)`.
- `split_request_is_reassembled`: send 10 bytes, then the rest. `poll(.., 1000)` gives `Ok(true)` and the reply payload reads as 30.
- `unknown_service_gets_error_reply`: `poll` gives `Err(ServiceNotFound)` and the client receives type `Error`.
- `oversized_frame_drops_peer_and_keeps_serving`: header with `payload_size` 1000. `poll` gives `Err(InternalError)`. A new client gets a `Response`.
- `client_close_without_data_gives_false`: the client connects and closes. `poll(.., 1000)` gives `Ok(false)`. A new client gets a `Response`.
- `close_mid_header_gives_false`: the client sends 10 bytes and closes. `poll(.., 1000)` gives `Ok(false)`. A new client gets a `Response`.
- `stalled_payload_gives_error_and_keeps_serving`: the client sends the header and 2 of 8 payload bytes and stays open. `poll(.., 1000)` gives `Err(InternalError)` after about 1 s. A new client gets a `Response`.
- `client_gone_before_reply_gives_error`: the client sends a request and closes. `poll(.., 1000)` gives `Err(InternalError)`. A new client gets a `Response`.
- `regular_file_is_never_removed`: `open` on a regular file gives `Err`, and the file still contains `keep`.
- `stale_socket_file_is_replaced`: `UnixListener::bind(path)`, drop it, `open(path)` gives `Ok`, and a second `open(path)` connects as a client.
- `long_path_is_invalid_argument`: 200 characters give `Err(InvalidArgument)`.
- `nul_in_path_is_invalid_argument`: `"a\0b"` gives `Err(InvalidArgument)`.

- [ ] **Step 2: Run the tests to make sure they fail**

Run: `cargo test --test uds_endpoint`
Expected: FAIL. `UdsTransport` does not exist.

- [ ] **Step 3: Write `src/uds.rs`**

Port `src/ports/posix/transport_uds.cpp` function by function. Do not delete that file yet.

State: `listener: Option<UnixListener>`, `conn: Option<UnixStream>`. Constant: `FRAME_TIMEOUT_MS = 1000`.

```rust
#[allow(unsafe_code)]
fn wait_fd(fd: RawFd, events: i16, timeout_ms: u64) -> Result<(), TransportError>
```

`wait_fd` clamps the timeout to `i32::MAX` and retries on `EINTR`. `rc == 0` gives `Timeout`.

- `open`: `UnixStream::connect`. An error of kind `InvalidInput` gives `InvalidArgument`. Then use `fs::symlink_metadata` and `FileTypeExt::is_socket` to remove only a stale socket, and call `UnixListener::bind`.
- `send` and `read_exact`: call `wait_fd`, then use `Write::write` and `Read::read`, and loop until done. Retry on `ErrorKind::Interrupted`.
- `drop_connection`: sets `conn` to `None` only in server mode.
- `receive`: use the same order as `Transport::receive` in C++, with the same error values.

Add this comment at `send`:

```rust
// shortcut: relies on the Rust runtime ignoring SIGPIPE, upgrade to MSG_NOSIGNAL if a non-Rust host links this.
```

- [ ] **Step 4: Run the tests to make sure they pass**

Run: `cargo test --test uds_endpoint`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src tests/uds_endpoint.rs
git commit -m "feat: add UDS transport"
```

### Task 6: Example and interop run

**Files:**
- Create: `examples/basic_service.rs`
- Test: temporary files only (not in the repository)

**Interfaces:**
- Consumes: all public items.

- [ ] **Step 1: Write `examples/basic_service.rs`**

Port `examples/basic_service.cpp`. Keep the four methods and the division-by-zero path. Division by zero gives `Err(ErrorCode::InvalidArgument)`. An unknown method gives `Err(ErrorCode::MethodNotFound)`. Use `wrapping_add`, `wrapping_sub`, `wrapping_mul` and `checked_div`, because client input must not panic. Use `println!`.

- [ ] **Step 2: Run the example**

Run: `cargo run --example basic_service`
Expected: the output has `Client: Result = 30`.

- [ ] **Step 3: Run the C++ client against a Rust server**

In a temporary directory outside the repository:

1. Create a Cargo project that depends on this crate by `path`. Its `main` opens `UdsTransport` at `/tmp/embb_interop.sock`, registers an add service as ID 1, and calls `poll` in a loop until it serves two messages.
2. Write a C++ client that links `embbinder_core` and `embbinder_port_posix` from the Task 1 build. It sends the add request (10, 20) and a request for service 999 with `port::Transport`.
3. Start the server and then the client.

Expected: the client gets `MsgType::Response` with 30, then `MsgType::Error` with `ErrorCode::SERVICE_NOT_FOUND`.

If this fails, stop and report the bytes. Do not continue to Task 7.

- [ ] **Step 4: Commit**

```bash
git add examples/basic_service.rs
git commit -m "feat: port basic_service example"
```

### Task 7: Remove C++, update CI and docs

**Files:**
- Delete: `CMakeLists.txt`, `include/`, `src/core/`, `src/ports/`, `src/utils/`, `examples/basic_service.cpp`, `tests/test_binder.cpp`, `tests/test_uds_endpoint.cpp`, `tests/check.hpp`, `scripts/`, `.clang-format`
- Modify: `.github/workflows/ci.yml`, `.gitignore`, `README.md`, `docs/architecture.md`, `docs/message_format.md`, `docs/portability.md`, `CLAUDE.md`, `.claude/skills/adding-a-service/SKILL.md`, `.claude/skills/porting-to-a-platform/SKILL.md`, `.claude/skills/verifying-changes/SKILL.md`

**Interfaces:**
- Consumes: all public items, for code samples in the docs.

- [ ] **Step 1: Add the local target and prove `no_std` before removal**

```bash
rustup target add thumbv7em-none-eabihf
```

Run: `cargo build --no-default-features --target thumbv7em-none-eabihf`
Expected: PASS

- [ ] **Step 2: Remove the C++ files**

Use `git rm -r` on the list in **Files**. Then:

Run: `git ls-files | grep -E '\.(cpp|hpp|cmake)$|CMakeLists|clang'`
Expected: no output.

- [ ] **Step 3: Rewrite `.github/workflows/ci.yml`**

Use one `build-test` job on the `ubuntu-latest` and `macos-latest` matrix. Use `dtolnay/rust-toolchain@stable` with `components: clippy, rustfmt` and `targets: thumbv7em-none-eabihf`. Run the five commands from the CI section of the spec. Remove the `format` job.

- [ ] **Step 4: Update the docs**

Read each file first, then change only what the conversion makes wrong:

- `docs/message_format.md`: `MessageHeader` is in `src/message.rs`. The header is little-endian. A service error gives an Error reply. Add the UTF-8 rule for strings. The `Serializer` references become `Writer` and `Reader`.
- `docs/portability.md`: replace the six abstractions with `impl Transport` and a `log` logger. Add the `default-features = false` line.
- `docs/architecture.md`: replace the singleton, registry and port text with the structure diagram from the spec.
- `README.md`: Rust usage and `cargo` commands.
- `CLAUDE.md`: new commands, layout, and non-obvious rules. Keep the wire invariants, in Rust terms. Remove all C++ items. Keep the spec rule: `MessageHeader::SIZE == 24`, and a header change bumps `VERSION`.
- The three skills: `adding-a-service` uses `impl Service`, `porting-to-a-platform` uses `impl Transport`, and `verifying-changes` uses the five CI commands.

Run: `git grep -nE 'cmake|ctest|clang-|IService|Binder::instance|\.hpp|\.cpp' -- . ':!docs/superpowers'`
Expected: no output.

- [ ] **Step 5: Run all checks from a clean state**

```bash
cargo clean && cargo test && cargo clippy --all-targets -- -D warnings && cargo fmt --check
```

```bash
cargo build --no-default-features --target thumbv7em-none-eabihf
```

```bash
cargo run --example basic_service | grep -q "Result = 30" && echo OK
```

Expected: all pass, the last command prints `OK`.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "refactor!: replace C++ implementation with Rust"
```
