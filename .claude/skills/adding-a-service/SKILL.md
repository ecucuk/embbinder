---
name: adding-a-service
description: Use when implementing or changing an EmbBinder Service, adding a method id, or writing client code that builds request messages for a service.
---

# Adding an EmbBinder service

A service implements `embbinder::Service` and is registered with `binder.register_service(id, &mut service)`. The payload is untrusted bytes from a transport.

## Handler rules

```rust
fn handle(
    &mut self,
    method_id: u32,
    payload: &[u8],
    response: &mut [u8],
) -> Result<usize, ErrorCode> {
    if method_id != ADD {
        return Err(ErrorCode::MethodNotFound);
    }
    let mut request = Reader::new(payload);       // no copy
    let sum = request.read_i32()?.wrapping_add(request.read_i32()?);

    let mut writer = Writer::new(response);
    writer.write_i32(sum)?;
    Ok(writer.len())
}
```

- Propagate read and write errors with `?`. `Err` sends an Error reply to the client.
- Do not panic on client input: use wrapping or checked arithmetic, and do not `unwrap`.
- Read fields in exactly the order the client wrote them; there are no tags or padding.
- Service ids must be unique: `register_service` gives `InvalidOperation` on duplicates and `OutOfMemory` when `N` slots are full — check it.
- The `Binder` holds `&mut` to the service, so the compiler makes the service outlive the `Binder`.

## Client side

Build the header like `examples/basic_service.rs`: `magic: MAGIC`, `version: VERSION`, `msg_type: MsgType::Request as u16`, `payload_size` from `Writer::len()`, then `to_bytes()` and the payload in one buffer. Handle both reply types: `Response`, and `Error` with a `u32` LE `ErrorCode` payload.

## Done

Cover the new method in `tests/` and follow the `verifying-changes` skill.
