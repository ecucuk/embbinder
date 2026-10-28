/**
 * @file test_binder.cpp
 * @brief Binder message handling and Serializer bounds checks.
 */

#include "check.hpp"
#include "embbinder/core/binder.hpp"
#include <cstring>
#include <vector>

using namespace embbinder::core;

namespace {

// Writes `claim` bytes of 0xAB, or only reports them if `lie` is set.
class FillService : public IService {
public:
  uint32_t seen_capacity = 0;
  uint32_t claim = 4;
  bool lie = false;

  void handle(uint32_t, const uint8_t *, uint32_t, uint8_t *response_buf,
              uint32_t response_capacity, uint32_t &response_size) override {
    seen_capacity = response_capacity;
    if (!lie) {
      if (claim > response_capacity)
        return;
      std::memset(response_buf, 0xAB, claim);
    }
    response_size = claim;
  }
};

std::vector<uint8_t> make_request(uint32_t service_id, uint32_t payload_size,
                                  size_t actual_payload) {
  MessageHeader hdr{
      kMagic,     kVersion, static_cast<uint16_t>(MsgType::Request),
      service_id, 1,        payload_size,
      42};
  std::vector<uint8_t> buf(sizeof(hdr) + actual_payload, 0);
  std::memcpy(buf.data(), &hdr, sizeof(hdr));
  return buf;
}

MessageHeader header_of(const uint8_t *buf) {
  MessageHeader hdr;
  std::memcpy(&hdr, buf, sizeof(hdr));
  return hdr;
}

} // namespace

int main() {
  Binder &binder = Binder::instance();
  FillService svc;
  CHECK(binder.register_service(10, &svc));

  uint8_t out[sizeof(MessageHeader) + 8];
  uint32_t out_size = 0;

  // Normal request: service sees the real capacity, response is well formed.
  auto req = make_request(10, 0, 0);
  CHECK(binder.process_message(req.data(), req.size(), out, sizeof(out),
                               out_size));
  CHECK(svc.seen_capacity == 8);
  CHECK(out_size == sizeof(MessageHeader) + 4);
  CHECK(header_of(out).type == static_cast<uint16_t>(MsgType::Response));
  CHECK(header_of(out).request_id == 42);

  // Service reporting more than the capacity is turned into an error.
  svc.lie = true;
  svc.claim = 9;
  auto res = binder.process_message(req.data(), req.size(), out, sizeof(out),
                                    out_size);
  CHECK(res.error() == ErrorCode::INTERNAL_ERROR);
  CHECK(header_of(out).type == static_cast<uint16_t>(MsgType::Error));
  svc.lie = false;

  // Unknown service: Error message carrying the code and request id.
  auto unknown = make_request(999, 0, 0);
  res = binder.process_message(unknown.data(), unknown.size(), out, sizeof(out),
                               out_size);
  CHECK(res.error() == ErrorCode::SERVICE_NOT_FOUND);
  CHECK(out_size == sizeof(MessageHeader) + 4);
  MessageHeader err = header_of(out);
  CHECK(err.type == static_cast<uint16_t>(MsgType::Error));
  CHECK(err.request_id == 42 && err.service_id == 999 && err.payload_size == 4);
  Serializer code;
  code.attach_buffer(out + sizeof(MessageHeader), 4);
  CHECK(code.read_uint32().value() ==
        static_cast<uint32_t>(ErrorCode::SERVICE_NOT_FOUND));

  // payload_size larger than the input (incl. wrap-around values) is rejected.
  auto huge = make_request(10, 0xFFFFFFF0u, 0);
  res = binder.process_message(huge.data(), huge.size(), out, sizeof(out),
                               out_size);
  CHECK(res.error() == ErrorCode::BUFFER_TOO_SMALL && out_size == 0);

  // Bad arguments.
  res = binder.process_message(req.data(), req.size(), nullptr, 0, out_size);
  CHECK(res.error() == ErrorCode::INVALID_ARGUMENT);

  // Serializer: oversized string length and reads past the end fail cleanly.
  uint8_t bad_string[] = {0xFF, 0xFF, 0xFF, 0xFF, 'x'};
  Serializer s;
  s.attach_buffer(bad_string, sizeof(bad_string));
  CHECK(s.read_string().is_error());
  Serializer short_buf;
  short_buf.attach_buffer(bad_string, 3);
  CHECK(short_buf.read_int32().is_error());

  std::printf("test_binder: OK\n");
  return 0;
}
