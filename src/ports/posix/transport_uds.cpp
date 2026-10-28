/**
 * @file transport_uds.cpp
 * @brief Transport implementation using Unix Domain Sockets for POSIX.
 *
 * open() connects as a client if a server is listening at the path, otherwise
 * it becomes a server and accepts one peer at a time. Each receive() returns
 * exactly one framed message (MessageHeader + payload_size bytes).
 */

#include "embbinder/core/message.hpp"
#include "embbinder/port/port.hpp"
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace embbinder::port {

namespace {

using embbinder::core::MessageHeader;

// Once a frame has started, the rest must arrive within this time.
constexpr time_ms_t kFrameTimeoutMs = 1000;

struct UdsTransport {
  int listen_fd; // -1 in client mode
  int conn_fd;   // -1 while a server has no peer
};

int to_poll_timeout(time_ms_t timeout_ms) {
  return timeout_ms > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(timeout_ms);
}

Result wait_fd(int fd, short events, time_ms_t timeout_ms) {
  pollfd pfd{fd, events, 0};
  int rc;
  do {
    rc = ::poll(&pfd, 1, to_poll_timeout(timeout_ms));
  } while (rc < 0 && errno == EINTR);
  if (rc == 0)
    return Result::TIMEOUT;
  return rc > 0 ? Result::SUCCESS : Result::INTERNAL_ERROR;
}

// Reads exactly size bytes. Fails if the peer closes or stalls mid-frame.
Result read_exact(int fd, uint8_t *buf, size_t size, time_ms_t timeout_ms) {
  size_t done = 0;
  while (done < size) {
    Result r = wait_fd(fd, POLLIN, timeout_ms);
    if (r != Result::SUCCESS)
      return r;
    ssize_t n = ::recv(fd, buf + done, size - done, 0);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return Result::INTERNAL_ERROR;
    done += static_cast<size_t>(n);
  }
  return Result::SUCCESS;
}

void drop_connection(UdsTransport *t) {
  if (t->conn_fd >= 0 && t->listen_fd >= 0) {
    ::close(t->conn_fd);
    t->conn_fd = -1;
  }
}

void set_nosigpipe(int fd) {
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
  (void)fd;
#endif
}

} // namespace

Result Transport::open(TransportHandle &out, const char *endpoint_name,
                       const TransportConfig &cfg) {
  (void)cfg;
  if (!endpoint_name ||
      std::strlen(endpoint_name) >= sizeof(sockaddr_un::sun_path))
    return Result::INVALID_ARGUMENT;

  sockaddr_un addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, endpoint_name, sizeof(addr.sun_path) - 1);

  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
    return Result::INTERNAL_ERROR;

  // Client mode: a server is already listening.
  if (connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
    set_nosigpipe(sock);
    out = new UdsTransport{-1, sock};
    return Result::SUCCESS;
  }

  // Server mode. Only replace a stale socket file, never a regular file.
  struct stat st;
  if (lstat(endpoint_name, &st) == 0 && S_ISSOCK(st.st_mode))
    unlink(endpoint_name);

  ::close(sock); // a failed connect() leaves the socket unusable
  sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock >= 0 &&
      bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 &&
      listen(sock, 1) == 0) {
    out = new UdsTransport{sock, -1};
    return Result::SUCCESS;
  }

  if (sock >= 0)
    ::close(sock);
  return Result::INTERNAL_ERROR;
}

void Transport::close(TransportHandle handle) {
  if (!handle)
    return;
  UdsTransport *t = static_cast<UdsTransport *>(handle);
  if (t->conn_fd >= 0)
    ::close(t->conn_fd);
  if (t->listen_fd >= 0)
    ::close(t->listen_fd);
  delete t;
}

Result Transport::send(TransportHandle handle, const uint8_t *data, size_t size,
                       time_ms_t timeout_ms) {
  if (!handle || !data)
    return Result::INVALID_ARGUMENT;
  UdsTransport *t = static_cast<UdsTransport *>(handle);
  if (t->conn_fd < 0)
    return Result::INTERNAL_ERROR;

#ifdef MSG_NOSIGNAL
  const int flags = MSG_NOSIGNAL;
#else
  const int flags = 0; // SO_NOSIGPIPE set on the socket instead
#endif

  size_t done = 0;
  while (done < size) {
    Result r = wait_fd(t->conn_fd, POLLOUT, timeout_ms);
    if (r != Result::SUCCESS)
      return r;
    ssize_t n = ::send(t->conn_fd, data + done, size - done, flags);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      drop_connection(t);
      return Result::INTERNAL_ERROR;
    }
    done += static_cast<size_t>(n);
  }
  return Result::SUCCESS;
}

Result Transport::receive(TransportHandle handle, uint8_t *buffer,
                          size_t buffer_size, size_t &out_size,
                          time_ms_t timeout_ms) {
  out_size = 0;
  if (!handle || !buffer || buffer_size < sizeof(MessageHeader))
    return Result::INVALID_ARGUMENT;
  UdsTransport *t = static_cast<UdsTransport *>(handle);

  if (t->conn_fd < 0) {
    Result r = wait_fd(t->listen_fd, POLLIN, timeout_ms);
    if (r != Result::SUCCESS)
      return r;
    t->conn_fd = ::accept(t->listen_fd, nullptr, nullptr);
    if (t->conn_fd < 0)
      return Result::INTERNAL_ERROR;
    set_nosigpipe(t->conn_fd);
  }

  // Wait for the first byte with the caller's timeout, then bound the frame.
  Result r = wait_fd(t->conn_fd, POLLIN, timeout_ms);
  if (r != Result::SUCCESS)
    return r;

  r = read_exact(t->conn_fd, buffer, sizeof(MessageHeader), kFrameTimeoutMs);
  if (r != Result::SUCCESS) {
    // Peer closed or stalled: a server goes back to accepting.
    drop_connection(t);
    return t->listen_fd >= 0 ? Result::TIMEOUT : Result::INTERNAL_ERROR;
  }

  MessageHeader hdr;
  std::memcpy(&hdr, buffer, sizeof(hdr));
  if (hdr.payload_size > buffer_size - sizeof(MessageHeader)) {
    drop_connection(t); // can't skip the payload and stay in sync
    return Result::NO_MEMORY;
  }

  r = read_exact(t->conn_fd, buffer + sizeof(MessageHeader), hdr.payload_size,
                 kFrameTimeoutMs);
  if (r != Result::SUCCESS) {
    drop_connection(t);
    return Result::INTERNAL_ERROR;
  }

  out_size = sizeof(MessageHeader) + hdr.payload_size;
  return Result::SUCCESS;
}

} // namespace embbinder::port
