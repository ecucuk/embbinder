/**
 * @file basic_service.cpp
 * @brief Example demonstrating basic service registration and messaging.
 */

#include "embbinder/core/binder.hpp"
#include "embbinder/core/log_port.hpp"
#include "embbinder/core/serializer.hpp"
#include "embbinder/port/port.hpp"
#include "embbinder/utils/span.hpp"
#include "embbinder/utils/static_vector.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace embbinder::core;
using namespace embbinder::port;

// Define a simple Calculator Service
class CalculatorService : public IService {
public:
  enum Methods { ADD = 1, SUBTRACT = 2, MULTIPLY = 3, DIVIDE = 4 };

  void handle(uint32_t method_id, const uint8_t *payload, uint32_t payload_size,
              uint8_t *response_buf, uint32_t &response_size) override {

    Serializer req;
    req.attach_buffer(payload, payload_size);

    Serializer resp;

    int32_t a = req.read_int32().value();
    int32_t b = req.read_int32().value();

    int32_t result = 0;

    switch (method_id) {

    case ADD: {
      result = a + b;
      embbinder::core::log(LogLevel::INFO, "BasicService",
                           "Service: Adding %d + %d = %d", a, b, result);
    } break;

    case SUBTRACT: {
      result = a - b;
      embbinder::core::log(LogLevel::INFO, "BasicService",
                           "Service: Subtracting %d - %d = %d", a, b, result);
    } break;

    case MULTIPLY: {
      result = a * b;
      embbinder::core::log(LogLevel::INFO, "BasicService",
                           "Service: Multiplying %d * %d = %d", a, b, result);
    } break;

    case DIVIDE: {
      if (b != 0) {
        result = a / b;
        embbinder::core::log(LogLevel::INFO, "BasicService",
                             "Service: Dividing %d / %d = %d", a, b, result);
      } else {
        embbinder::core::log(LogLevel::ERROR, "BasicService",
                             "Service: Division by zero");
        return;
      }
    } break;

    default: {
      embbinder::core::log(LogLevel::INFO, "BasicService",
                           "Service: Unknown method");
      return;
    }
    }
    resp.write_int32(result);
    std::memcpy(response_buf, resp.data(), resp.size());
    response_size = static_cast<uint32_t>(resp.size());
  }
};

int main() {
  embbinder::core::log(LogLevel::INFO, "BasicService",
                       "EmbBinder Basic Service Example");

  // 1. Initialize Binder
  Binder &binder = Binder::instance();

  // 2. Register Service
  CalculatorService calcService;
  binder.register_service(100, &calcService);

  // 3. Simulate a Client Request (Add 10 + 20)
  embbinder::core::log(LogLevel::INFO, "BasicService",
                       "Client: Sending Request (10 + 20)");

  Serializer req_payload;
  req_payload.write_int32(10);
  req_payload.write_int32(20);

  MessageHeader hdr;
  hdr.magic = kMagic;
  hdr.version = kVersion;
  hdr.type = static_cast<uint16_t>(MsgType::Request);
  hdr.service_id = 100;
  hdr.method_id = CalculatorService::ADD;
  hdr.request_id = 1;
  hdr.payload_size = static_cast<uint32_t>(req_payload.size());

  std::vector<uint8_t> message_buffer;
  message_buffer.resize(sizeof(MessageHeader) + req_payload.size());

  std::memcpy(message_buffer.data(), &hdr, sizeof(MessageHeader));
  std::memcpy(message_buffer.data() + sizeof(MessageHeader), req_payload.data(),
              req_payload.size());

  // Output buffer for response
  uint8_t response_buffer[256];
  uint32_t response_size = 0;

  // 4. Send Message via Binder
  embbinder::core::Result<void> result = binder.process_message(
      message_buffer.data(), message_buffer.size(), response_buffer,
      sizeof(response_buffer), response_size);

  if (result.is_ok()) {
    embbinder::core::log(LogLevel::INFO, "BasicService",
                         "Client: Receive Success. Size: %d", response_size);

    // Parse Response
    MessageHeader *resp_hdr =
        reinterpret_cast<MessageHeader *>(response_buffer);
    if (resp_hdr->type == static_cast<uint16_t>(MsgType::Response)) {
      Serializer resp_reader;
      resp_reader.attach_buffer(response_buffer + sizeof(MessageHeader),
                                resp_hdr->payload_size);
      int32_t sum = resp_reader.read_int32().value();
      embbinder::core::log(LogLevel::INFO, "BasicService",
                           "Client: Result = %d", sum);
    }
  } else {
    embbinder::core::log(LogLevel::ERROR, "BasicService",
                         "Client: Error processing message");
  }

  return 0;
}
