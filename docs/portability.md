# Portability Guide

The EmbBinder core needs no operating system and no heap. To run it on a new platform, implement one trait.

## Use the crate with no operating system

```toml
[dependencies]
embbinder = { path = "...", default-features = false }
```

CI checks this configuration on `thumbv7em-none-eabihf`.

## Implement a Transport

```rust
pub trait Transport {
    fn send(&mut self, data: &[u8], timeout_ms: u64) -> Result<(), TransportError>;
    fn receive(&mut self, buf: &mut [u8], timeout_ms: u64) -> Result<usize, TransportError>;
}
```

Follow these rules:

1. Open the link in your constructor. Close it in `Drop`.
2. Return `TransportError::Timeout` from `receive` if no message arrived. `Endpoint::poll` then gives `Ok(false)`.
3. Return exactly one whole message from `receive`.
4. On a stream link (UART, TCP), read the 24-byte header, then read `payload_size` bytes.
5. If `payload_size` does not fit in the buffer, drop the connection and return `TransportError::NoMemory`. You cannot resynchronize a stream after this.

## Logging

The crate uses the `log` crate. To see the output, install a logger for your platform. No logger means no output.

## Threads and locks

The core does not use threads or locks. If an application shares a `Binder` between threads, it uses the mutex of its platform.

## Example: UDS

`src/uds.rs` implements `Transport` with Unix domain sockets. It is available with the `std` feature on Unix. Use it as a reference for framing, timeouts and peer loss.
