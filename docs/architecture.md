# System Architecture

EmbBinder is designed with a layered architecture to ensure separation of concerns, portability, and ease of use.

```mermaid
graph TD
    UserApp[User Application] --> Services[User Services]
    Services --> |Inherits| IService[IService Interface]
    UserApp --> Binder[Binder Core]
    
    subgraph "Core Layer"
        Endpoint --> Binder
        Binder --> |Looks up| Registry[Service Registry]
    end
    
    subgraph "Porting Layer"
        Endpoint --> Port[Port Interface (OS/HW)]
    end

    subgraph "Transport"
        Port --> |Read/Write| TransportDriver[Transport Driver (UART/SPI/TCP)]
    end
```

## Core Components

### 1. Binder
The `Binder` class is the main entry point of the library. It is a singleton responsible for:
- Initializing the system.
- Managing the service registry.
- Processing incoming raw data buffers (`process_message`).
- directing requests to the appropriate service.

### 2. Service Registry (`Registry`)
The `ServiceRegistry` maintains a mapping of Service IDs to Service Instances. It allows for dynamic registration of services at runtime.

### 3. IService Interface
All user services must inherit from `IService`. This interface defines a pure virtual handle method:
```cpp
virtual void handle(uint32_t method_id, const uint8_t *payload,
                    uint32_t payload_size, uint8_t *response_buf,
                    uint32_t response_capacity, uint32_t &response_size) = 0;
```
Services implement this method to deserialize arguments, execute logic, and serialize responses. A service must never write more than `response_capacity` bytes; `Binder` rejects a larger `response_size` with an `Error` message.

### 4. Endpoint
`Endpoint` owns a port `Transport`, reads one message per `poll()`, passes it to `Binder::process_message`, and sends back the response — including `Error` responses for unknown services.

### 5. Serializer
A helper class to safely serialize and deserialize primitive types (int, float, string) and raw buffers into/from a byte stream. It handles endianness (if implemented) and buffer bounds checking.
