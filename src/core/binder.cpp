/**
 * @file binder.cpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Implementation of Binder class.
 */

#include "embbinder/core/binder.hpp"
#include "embbinder/core/message.hpp"
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

  if (!input_buffer || input_size < sizeof(MessageHeader)) {
    return ErrorCode::INVALID_ARGUMENT;
  }

  // Parse Header
  MessageHeader header;
  std::memcpy(&header, input_buffer, sizeof(MessageHeader));

  if (header.magic != kMagic) {
    return ErrorCode::INVALID_ARGUMENT; // Invalid magic
  }

  // TODO: Check version compatibility

  if (header.payload_size + sizeof(MessageHeader) > input_size) {
    return ErrorCode::BUFFER_TOO_SMALL; // Incomplete message
  }

  const uint8_t *payload = input_buffer + sizeof(MessageHeader);

  IService *service =
      ServiceRegistry::instance().get_service(header.service_id);
  if (!service) {
    send_error_response(header.request_id, ErrorCode::SERVICE_NOT_FOUND,
                        output_buffer, output_capacity, output_size);
    return ErrorCode::SERVICE_NOT_FOUND;
  }

  // Prepare response buffer
  // We reserve space for the header in the response
  if (output_capacity < sizeof(MessageHeader)) {
    return ErrorCode::BUFFER_TOO_SMALL;
  }

  uint8_t *resp_payload_ptr = output_buffer + sizeof(MessageHeader);
  uint32_t resp_payload_cap =
      static_cast<uint32_t>(output_capacity - sizeof(MessageHeader));
  uint32_t resp_payload_size = 0;

  // Dispatch
  service->handle(header.method_id, payload, header.payload_size,
                  resp_payload_ptr, resp_payload_size);

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

void Binder::send_error_response(uint32_t request_id, ErrorCode error,
                                 uint8_t *out_buf, size_t out_cap,
                                 uint32_t &out_size) {
  if (out_cap < sizeof(MessageHeader)) {
    out_size = 0;
    return;
  }

  MessageHeader resp_header;
  resp_header.magic = kMagic;
  resp_header.version = kVersion;
  resp_header.type = static_cast<uint16_t>(MsgType::Response); // Or Error type?
  // Using high bit of type or specific error code in payload?
  // For simplicity, let's treat it as a response with 0 payload but maybe a
  // specific flag or we need an error field. The current MessageHeader doesn't
  // have an error code field. We could use method_id = 0xFFFFFFFF or similar as
  // convention, or payload contains error info.

  // Let's assume payload contains error code as int32 if type indicates error,
  // but we don't have Error type defined in Message.hpp yet. Let's modify
  // Message.hpp later to include Error type or status.

  // For now, minimal valid header.
  resp_header.service_id = 0;
  resp_header.method_id = 0;
  resp_header.request_id = request_id;
  resp_header.payload_size = 0;

  std::memcpy(out_buf, &resp_header, sizeof(MessageHeader));
  out_size = sizeof(MessageHeader);
}

} // namespace embbinder::core
