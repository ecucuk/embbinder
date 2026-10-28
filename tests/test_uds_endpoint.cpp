/**
 * @file test_uds_endpoint.cpp
 * @brief Endpoint over the POSIX UDS transport: accept, framing, errors.
 */

#include "check.hpp"
#include "embbinder/core/binder.hpp"
#include "embbinder/core/endpoint.hpp"
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <vector>

using namespace embbinder::core;
namespace port = embbinder::port;

namespace {

const char *kPath = "embb_test.sock";
const port::TransportConfig kCfg{256};

class AddService : public IService {
public:
  void handle(uint32_t, const uint8_t *payload, uint32_t payload_size,
              uint8_t *response_buf, uint32_t response_capacity,
              uint32_t &response_size) override {
    Serializer req;
    req.attach_buffer(payload, payload_size);
    auto a = req.read_int32();
    auto b = req.read_int32();
    if (!a.is_ok() || !b.is_ok() || response_capacity < 4)
      return;
    Serializer resp;
    resp.write_int32(a.value() + b.value());
    std::memcpy(response_buf, resp.data(), resp.size());
    response_size = static_cast<uint32_t>(resp.size());
  }
};

std::vector<uint8_t> make_request(uint32_t service_id, uint32_t payload_size) {
  Serializer payload;
  payload.write_int32(10);
  payload.write_int32(20);
  MessageHeader hdr{
      kMagic,     kVersion, static_cast<uint16_t>(MsgType::Request),
      service_id, 1,        payload_size,
      7};
  std::vector<uint8_t> buf(sizeof(hdr) + payload.size());
  std::memcpy(buf.data(), &hdr, sizeof(hdr));
  std::memcpy(buf.data() + sizeof(hdr), payload.data(), payload.size());
  return buf;
}

port::TransportHandle connect_client() {
  port::TransportHandle h = nullptr;
  CHECK(port::Transport::open(h, kPath, kCfg) == port::Result::SUCCESS);
  return h;
}

MessageHeader receive_reply(port::TransportHandle client, uint8_t *buf) {
  size_t n = 0;
  CHECK(port::Transport::receive(client, buf, 256, n, 1000) ==
        port::Result::SUCCESS);
  MessageHeader hdr;
  std::memcpy(&hdr, buf, sizeof(hdr));
  CHECK(n == sizeof(hdr) + hdr.payload_size);
  return hdr;
}

} // namespace

int main() {
  AddService svc;
  CHECK(Binder::instance().register_service(1, &svc));

  Endpoint server;
  CHECK(server.open(kPath, kCfg));

  // No client yet: poll times out without error.
  auto polled = server.poll(0);
  CHECK(polled.is_ok() && !polled.value());

  // Request split across two writes is reassembled into one message.
  port::TransportHandle client = connect_client();
  auto req = make_request(1, 8);
  CHECK(port::Transport::send(client, req.data(), 10, 1000) ==
        port::Result::SUCCESS);
  CHECK(port::Transport::send(client, req.data() + 10, req.size() - 10, 1000) ==
        port::Result::SUCCESS);
  polled = server.poll(1000);
  CHECK(polled.is_ok() && polled.value());

  uint8_t buf[256];
  MessageHeader reply = receive_reply(client, buf);
  CHECK(reply.type == static_cast<uint16_t>(MsgType::Response));
  Serializer result;
  result.attach_buffer(buf + sizeof(MessageHeader), reply.payload_size);
  CHECK(result.read_int32().value() == 30);

  // Unknown service: the client still gets an Error reply.
  auto unknown = make_request(999, 8);
  CHECK(port::Transport::send(client, unknown.data(), unknown.size(), 1000) ==
        port::Result::SUCCESS);
  CHECK(server.poll(1000).error() == ErrorCode::SERVICE_NOT_FOUND);
  CHECK(receive_reply(client, buf).type ==
        static_cast<uint16_t>(MsgType::Error));

  // Oversized frame: server drops the peer and keeps serving new clients.
  auto oversized = make_request(1, 1000);
  CHECK(port::Transport::send(client, oversized.data(), oversized.size(),
                              1000) == port::Result::SUCCESS);
  CHECK(server.poll(1000).is_error());
  port::Transport::close(client);

  client = connect_client();
  CHECK(port::Transport::send(client, req.data(), req.size(), 1000) ==
        port::Result::SUCCESS);
  polled = server.poll(1000);
  CHECK(polled.is_ok() && polled.value());
  CHECK(receive_reply(client, buf).type ==
        static_cast<uint16_t>(MsgType::Response));
  port::Transport::close(client);
  server.close();
  unlink(kPath);

  // A regular file at the endpoint path is never deleted.
  const char *plain = "embb_plain.txt";
  std::ofstream(plain) << "keep";
  port::TransportHandle h = nullptr;
  CHECK(port::Transport::open(h, plain, kCfg) != port::Result::SUCCESS);
  CHECK(access(plain, F_OK) == 0);
  unlink(plain);

  std::printf("test_uds_endpoint: OK\n");
  return 0;
}
