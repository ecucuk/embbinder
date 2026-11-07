# EmbBinder

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Language](https://img.shields.io/badge/rust-2024-blue.svg)

**EmbBinder** is a lightweight RPC/Binder library for embedded systems. A request is a 24-byte header and a payload. `Binder` finds a `Service` by its ID and calls `handle()`.

## Features

- **`no_std`, no heap**: The core runs on a microcontroller with no allocator.
- **No global state**: The application owns the `Binder`, the services and the buffers.
- **Zero copy**: `Reader` and `Writer` operate on caller buffers. `read_string` returns a `&str` into the payload.
- **Portable**: A platform implements the `Transport` trait.
- **Unix domain sockets**: `UdsTransport` is available with the `std` feature.

## Project Structure

- **`src/message.rs`**: Wire header, `MsgType`, `MAGIC`, `VERSION`.
- **`src/error.rs`**: `ErrorCode`.
- **`src/serializer.rs`**: `Writer` and `Reader`.
- **`src/binder.rs`**: `Service`, `Binder`, `ProcessError`.
- **`src/transport.rs`**: `Transport`, `TransportError`.
- **`src/endpoint.rs`**: `Endpoint`, the poll loop.
- **`src/uds.rs`**: `UdsTransport` (`std` on Unix).
- **`examples/`**: Usage examples.
- **`tests/`**: Integration tests.

## Build Instructions

```bash
git clone https://github.com/erguncucuk/embbinder.git
cd embbinder
cargo test
cargo run --example basic_service
```

For a target with no operating system, disable the default features:

```toml
[dependencies]
embbinder = { path = "...", default-features = false }
```

## Usage

### 1. Define a Service

```rust
use embbinder::{ErrorCode, Reader, Service, Writer};

struct CalculatorService;

impl Service for CalculatorService {
    fn handle(
        &mut self,
        method_id: u32,
        payload: &[u8],
        response: &mut [u8],
    ) -> Result<usize, ErrorCode> {
        if method_id != 1 {
            return Err(ErrorCode::MethodNotFound);
        }
        let mut request = Reader::new(payload);
        let sum = request.read_i32()?.wrapping_add(request.read_i32()?);

        let mut writer = Writer::new(response);
        writer.write_i32(sum)?;
        Ok(writer.len())
    }
}
```

`Err` sends a `MsgType::Error` reply to the client.

### 2. Serve Requests

```rust,ignore
use embbinder::{Binder, Endpoint, UdsTransport};

let mut calculator = CalculatorService;
let mut binder = Binder::<4>::new(); // at most 4 services
binder.register_service(100, &mut calculator)?;

// 256 is the maximum message size.
let mut endpoint = Endpoint::<_, 256>::new(UdsTransport::open("/tmp/calc.sock")?);
loop {
    if let Err(code) = endpoint.poll(&mut binder, 1000) {
        log::warn!("request failed: {code:?}");
    }
}
```

For a complete example, see `examples/basic_service.rs`.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
