---
name: porting-to-a-platform
description: Use when implementing or fixing an EmbBinder Transport (UART, socket, queue) or porting EmbBinder to a platform (FreeRTOS, Zephyr, bare metal).
---

# Porting EmbBinder

`src/transport.rs` is the contract. A port is one type that implements `Transport`. The core needs nothing else from the platform.

## Contract checklist

- Open the link in a constructor. Close it in `Drop`.
- `receive` gives `Err(TransportError::Timeout)` when nothing arrived. `Endpoint::poll` turns this into `Ok(false)`.
- Code that needs `std` goes behind a feature. The core must stay `no_std` with no heap.
- Keep `unsafe` out of the core. If a driver call needs it, use the smallest possible block with a `// SAFETY:` comment and `#[allow(unsafe_code)]`.

## Transport must deliver whole messages

`Binder::process_message` expects one complete message per buffer. Stream transports (UART, TCP) need framing: read 24 header bytes, then `payload_size` bytes, and drop the connection when the size exceeds the buffer (you can't resync). A server transport must accept before it reads. `src/uds.rs` is the reference: `poll(2)` for timeouts, `read_exact`, peer loss → back to accepting.

Never delete whatever sits at the endpoint path; check it is a socket first.

## Done

Add a test like `tests/uds_endpoint.rs` and follow the `verifying-changes` skill.
