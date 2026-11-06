# EmbBinder

`no_std`, heap-free Rust RPC/binder library for embedded systems. A request is a 24-byte `MessageHeader` + payload; `Binder` looks up a `Service` by `service_id` and calls `handle()`.

## Commands

```bash
cargo test                                # tests/: binder, endpoint, serializer, uds_endpoint, wire_format
cargo run --example basic_service         # prints "Client: Result = 30"
cargo clippy --all-targets -- -D warnings
cargo fmt
cargo build --no-default-features --target thumbv7em-none-eabihf   # proves no_std
```

CI (`.github/workflows/ci.yml`) runs all of these on Ubuntu and macOS. Use the `verifying-changes` skill before calling work done.

## Layout

- `src/message.rs` — `MessageHeader`, `MsgType`, `MAGIC`, `VERSION`.
- `src/error.rs` — `ErrorCode` (wire values).
- `src/serializer.rs` — `Writer`, `Reader` (little-endian, over caller buffers).
- `src/binder.rs` — `Service`, `Binder<'a, N>`, `ProcessError`.
- `src/transport.rs` — `Transport`, `TransportError`.
- `src/endpoint.rs` — `Endpoint<T, M>` poll loop.
- `src/uds.rs` — `UdsTransport`, only with `std` on Unix.

## Things that are not obvious

- `Binder` is a value, not a singleton. `register_service` holds a `&mut` to the service for the life of the `Binder`, so a test reads service state after the `Binder` is gone.
- `Service: Send` lets an application put a `Binder` in a mutex. The crate has no lock of its own.
- `ProcessError.reply_size > 0` means an Error reply is in the output buffer. `Endpoint::poll` sends it and then returns the code.
- `Endpoint::poll` gives `Ok(false)` when no message arrived. Check the `bool`, not only `is_ok()`.
- `unsafe` is denied in the crate. The only exceptions are the `libc::poll` and `libc::send` calls in `src/uds.rs`.
- `UdsTransport` uses `libc::send`, not `std` writes: a `std` write can raise `SIGPIPE` on Linux. `set_write_timeout` bounds the send, because macOS ignores `MSG_DONTWAIT` on `send`.
- Test socket paths go in `/tmp`, because a long repository path can exceed `sun_path`.

## Wire format invariants

- `MessageHeader::SIZE == 24`. Changing the header means: bump `VERSION`, update `docs/message_format.md`.
- `MsgType` and `ErrorCode` values are wire values; add new ones at the end.
- All header fields and payload values are little-endian. `tests/wire_format.rs` holds golden bytes from the former C++ implementation; do not change them to make a test pass.
- Input bytes must not cause a panic in library code: no `unwrap`, no unchecked slice index on an untrusted length.
- Bound checks on untrusted lengths must not overflow on 32-bit `usize`: write `len > total - pos`, not `pos + len > total`.

## Conventions

- `rustfmt` default style. Run `cargo fmt`.
- The core must not use `std` or `alloc`. New `std` code goes behind `#[cfg(feature = "std")]`.
- No panics for runtime failures: return `Result<T, ErrorCode>` or `Result<T, TransportError>`.
- Code comments in English.
