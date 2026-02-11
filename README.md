# EmbBinder

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Standard](https://img.shields.io/badge/c%2B%2B-17-blue.svg)

**EmbBinder** is a lightweight, high-performance RPC/Binder mechanism designed specifically for embedded systems. It facilitates seamless communication between different software components or processes with minimal overhead.

## Features

- **Lightweight & Efficient**: Optimized for resource-constrained environments.
- **C++17 Standard**: leveraging modern C++ features for safety and expressiveness.
- **Zero-Copy Friendly**: Utilizes `Span` and `StaticVector` to minimize memory allocations.
- **Easy Integration**: Simple `IService` interface for quick service implementation.
- **Robust Serialization**: Built-in `Serializer` for handling primitive types and buffers.
- **Modular Architecture**: Clean separation between core logic, porting layer, and utilities.

## Project Structure

The project is organized as follows:

- **`include/embbinder/core`**: Core logic (Binder, Registry, Dispatcher, Serializer).
- **`include/embbinder/port`**: Platform-specific porting layer.
- **`include/embbinder/utils`**: Utility classes (Span, StaticVector, ByteOrder).
- **`src`**: Implementation files.
- **`examples`**: Usage examples.
- **`tests`**: Unit tests.

## Build Instructions

This project uses CMake for building.

### Prerequisites

- CMake 3.10 or higher
- C++17 compatible compiler (GCC, Clang, MSVC)

### Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/erguncucuk/embbinder.git
   cd embbinder
   ```

2. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

3. Configure and build:
   ```bash
   cmake ..
   make
   ```

## Usage

Here is a basic example of how to define a service and invoke its methods.

### 1. Define a Service

Inherit from `embbinder::core::IService` and implement the `handle` method.

```cpp
#include "embbinder/core/binder.hpp"
#include "embbinder/core/serializer.hpp"

using namespace embbinder::core;

class CalculatorService : public IService {
public:
    enum Methods { ADD = 1 };

    void handle(uint32_t method_id, const uint8_t *payload, uint32_t payload_size,
                uint8_t *response_buf, uint32_t &response_size) override {
        
        Serializer req;
        // Assuming payload contains two int32_t
        req.attach_buffer(payload, payload_size);
        
        int32_t a = req.read_int32().value();
        int32_t b = req.read_int32().value();
        int32_t result = 0;

        if (method_id == ADD) {
            result = a + b;
        }

        Serializer resp;
        resp.write_int32(result);
        
        std::memcpy(response_buf, resp.data(), resp.size());
        response_size = static_cast<uint32_t>(resp.size());
    }
};
```

### 2. Register and Use the Service

Initialize the Binder, register your service, and process messages.

```cpp
int main() {
    // Initialize Binder
    Binder &binder = Binder::instance();

    // Register Service
    CalculatorService calcService;
    binder.register_service(100, &calcService);

    // Create a Request (Add 10 + 20)
    Serializer req_payload;
    req_payload.write_int32(10);
    req_payload.write_int32(20);

    // Prepare Message Header
    MessageHeader hdr;
    hdr.magic = kMagic;
    hdr.version = kVersion;
    hdr.type = static_cast<uint16_t>(MsgType::Request);
    hdr.service_id = 100;
    hdr.method_id = CalculatorService::ADD;
    hdr.request_id = 1;
    hdr.payload_size = static_cast<uint32_t>(req_payload.size());

    // Combine Header and Payload
    std::vector<uint8_t> message_buffer(sizeof(MessageHeader) + req_payload.size());
    std::memcpy(message_buffer.data(), &hdr, sizeof(MessageHeader));
    std::memcpy(message_buffer.data() + sizeof(MessageHeader), req_payload.data(), req_payload.size());

    // Process Message
    uint8_t response_buffer[256];
    uint32_t response_size = 0;
    
    binder.process_message(message_buffer.data(), message_buffer.size(), 
                           response_buffer, sizeof(response_buffer), response_size);

    return 0;
}
```

For a complete runnable example, check `examples/basic_service.cpp`.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
