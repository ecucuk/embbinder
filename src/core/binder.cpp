/**
 * @file binder.cpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Implementation of Binder class.
 */

#include "embbinder/core/binder.hpp"
#include "embbinder/core/message.hpp"
#include "embbinder/utils/byteorder.hpp"
#include <algorithm>
#include <cstring>

namespace embbinder::core {

Binder &Binder::instance() {
  static Binder s_instance;
  return s_instance;
}

bool Binder::register_service(uint32_t service_id, IService *service) {
  return ServiceRegistry::instance().add_service(service_id, service);
}

IService *Binder::get_service(uint32_t service_id) {
  return ServiceRegistry::instance().get_service(service_id);
}

Result<void> Binder::process_message(const uint8_t *input_buffer,
                                     size_t input_size, uint8_t *output_buffer,
                                     size_t output_capacity,
                                     uint32_t &output_size) {
  output_size = 0;

  if (!input_buffer || !output_buffer || input_size < sizeof(MessageHeader)) {
    return ErrorCode::INVALID_ARGUMENT;
  }

  // Parse Header
  MessageHeader header;
  std::memcpy(&header, input_buffer, sizeof(MessageHeader));

  if (header.magic != kMagic) {
    return ErrorCode::INVALID_ARGUMENT; // Invalid magic
  }

  // TODO: Check version compatibility

  // Subtract instead of add: payload_size + header can wrap on 32-bit size_t
  if (header.payload_size > input_size - sizeof(MessageHeader)) {
    return ErrorCode::BUFFER_TOO_SMALL; // Incomplete message
  }

  const uint8_t *payload = input_buffer + sizeof(MessageHeader);

  IService *service =
      ServiceRegistry::instance().get_service(header.service_id);
  if (!service) {
    send_error_response(header, ErrorCode::SERVICE_NOT_FOUND, output_buffer,
                        output_capacity, output_size);
    return ErrorCode::SERVICE_NOT_FOUND;
  }

  // Prepare response buffer
  // We reserve space for the header in the response
  if (output_capacity < sizeof(MessageHeader)) {
    return ErrorCode::BUFFER_TOO_SMALL;
  }

  uint8_t *resp_payload_ptr = output_buffer + sizeof(MessageHeader);
  uint32_t resp_payload_cap = static_cast<uint32_t>(
      std::min<size_t>(output_capacity - sizeof(MessageHeader), UINT32_MAX));
  uint32_t resp_payload_size = 0;

  // Dispatch
  service->handle(header.method_id, payload, header.payload_size,
                  resp_payload_ptr, resp_payload_cap, resp_payload_size);

  if (resp_payload_size > resp_payload_cap) {
    send_error_response(header, ErrorCode::INTERNAL_ERROR, output_buffer,
                        output_capacity, output_size);
    return ErrorCode::INTERNAL_ERROR;
  }

  // Construct response header
  MessageHeader resp_header;
  resp_header.magic = kMagic;
  resp_header.version = kVersion;
  resp_header.type = static_cast<uint16_t>(MsgType::Response);
  resp_header.service_id = header.service_id;
  resp_header.method_id = header.method_id;
  resp_header.request_id = header.request_id;
  resp_header.payload_size = resp_payload_size;

  std::memcpy(output_buffer, &resp_header, sizeof(MessageHeader));
  output_size = sizeof(MessageHeader) + resp_payload_size;

  return Result<void>();
}

void Binder::send_error_response(const MessageHeader &request, ErrorCode error,
                                 uint8_t *out_buf, size_t out_cap,
                                 uint32_t &out_size) {
  uint32_t code =
      utils::ByteOrder::to_little_endian(static_cast<uint32_t>(error));
  if (out_cap < sizeof(MessageHeader) + sizeof(code)) {
    out_size = 0;
    return;
  }

  MessageHeader resp_header;
  resp_header.magic = kMagic;
  resp_header.version = kVersion;
  resp_header.type = static_cast<uint16_t>(MsgType::Error);
  resp_header.service_id = request.service_id;
  resp_header.method_id = request.method_id;
  resp_header.request_id = request.request_id;
  resp_header.payload_size = sizeof(code);

  std::memcpy(out_buf, &resp_header, sizeof(MessageHeader));
  std::memcpy(out_buf + sizeof(MessageHeader), &code, sizeof(code));
  out_size = sizeof(MessageHeader) + sizeof(code);
}

} // namespace embbinder::core
