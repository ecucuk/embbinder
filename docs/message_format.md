# Message Format

EmbBinder uses a simple, binary message format designed for low overhead and ease of parsing.

## Message Structure

Every message consists of a fixed-size **Header** followed by a variable-length **Payload**.

```
+---------------------------------------------------------------+
|                        Message Header                         |
|                          (24 Bytes)                           |
+---------------------------------------------------------------+
|                           Payload                             |
|                       (Variable Size)                         |
+---------------------------------------------------------------+
```

### Header Fields

The header is defined in `embbinder::core::MessageHeader`. All fields are distinct structure members.

| Offset | Field        | Size (Bytes) | Description                                      |
|--------|--------------|--------------|--------------------------------------------------|
| 0      | `magic`      | 4            | Magic Value: `0x454D4242` ('EMBB')               |
| 4      | `version`    | 2            | Protocol Version (Currently 1)                   |
| 6      | `type`       | 2            | Message Type (1=Request, 2=Response, 3=Event)    |
| 8      | `service_id` | 4            | ID of the target service                         |
| 12     | `method_id`  | 4            | ID of the method to invoke                       |
| 16     | `payload_size`| 4           | Length of the payload in bytes                   |
| 20     | `request_id` | 4            | Unique ID to match requests with responses       |

### Message Types

- **Request (1)**: A client asking a service to perform an action.
- **Response (2)**: A service replying to a client's request.
- **Event (3)**: An asynchronous notification from a service.

## Payload Serialization

The payload contains the arguments for the method (in a Request) or the return values (in a Response).
Data is serialized sequentially using the `Serializer` class.

### Example

Calling `add(int32_t a, int32_t b)` with `a=10`, `b=20`.

**Payload content (Hex):**
```
0A 00 00 00 14 00 00 00
|---------| |---------|
   10          20
 (int32)     (int32)
```
*(Assuming Little Endian)*
