# System Architecture

EmbBinder is designed with a layered architecture to ensure separation of concerns, portability, and ease of use.

```mermaid
graph TD
    UserApp[User Application] --> Services[User Services]
    Services --> |Inherits| IService[IService Interface]
    UserApp --> Binder[Binder Core]
    
    subgraph "Core Layer"
        Binder --> Dispatcher
        Binder --> Registry[Service Registry]
        Dispatcher --> |Looks up| Registry
    end
    
    subgraph "Porting Layer"
        Binder --> Port[Port Interface (OS/HW)]
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
                    uint32_t &response_size) = 0;
```
Services implement this method to deserialized arguments, execute logic, and serialize responses.

### 4. Dispatcher
The `Dispatcher` parses the `MessageHeader`, identifies the target service and method, and invokes the service's handler. It also handles error reporting if a service or method is not found.

### 5. Serializer
A helper class to safely serialize and deserialize primitive types (int, float, string) and raw buffers into/from a byte stream. It handles endianness (if implemented) and buffer bounds checking.
