---
name: adding-a-service
description: Use when implementing or changing an EmbBinder IService, adding a method id, or writing client code that builds request messages for a service.
---

# Adding an EmbBinder service

A service is an `embbinder::core::IService` registered with `Binder::instance().register_service(id, &svc)`. The payload is untrusted bytes from a transport.

## Handler rules

```cpp
void handle(uint32_t method_id, const uint8_t *payload, uint32_t payload_size,
            uint8_t *response_buf, uint32_t response_capacity,
            uint32_t &response_size) override {
  if (method_id != ADD) {
    return;                                   // unknown method: response_size stays 0
  }
  Serializer req;
  req.attach_buffer(payload, payload_size);   // non-owning, no copy

  auto a = req.read_int32();
  auto b = req.read_int32();
  if (!a.is_ok() || !b.is_ok()) {
    return;                                   // response_size stays 0
  }

  Serializer resp;
  resp.write_int32(a.value() + b.value());
  if (resp.size() > response_capacity) {
    return;
  }
  std::memcpy(response_buf, resp.data(), resp.size());
  response_size = static_cast<uint32_t>(resp.size());
}
```

- Check every `read_*()` result. `value()` on an error returns `0`/empty silently.
- Never write more than `response_capacity` bytes. Reporting a larger `response_size` makes `Binder` reply with `MsgType::Error` / `INTERNAL_ERROR`.
- Unknown `method_id` → leave `response_size = 0`.
- Read fields in exactly the order the client wrote them; there are no tags or padding.
- Service ids must be unique: `register_service` returns `false` on duplicates — check it.
- The registry stores a raw pointer. The service must outlive its registration (`ServiceRegistry::remove_service`).

## Client side

Build the header like `examples/basic_service.cpp`: `magic = kMagic`, `version = kVersion`, `type = MsgType::Request`, `payload_size = serializer.size()`, then header bytes + payload bytes in one buffer. Handle both reply types: `Response`, and `Error` with a uint32 LE `ErrorCode` payload.

## Done

Cover the new method in `tests/` and follow the `verifying-changes` skill.
