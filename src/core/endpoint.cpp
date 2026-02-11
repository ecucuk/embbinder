/**
 * @file endpoint.cpp
 * @author Ergün CÜCÜK
 * @date 2026-01-23
 * @brief Implementation of Endpoint class.
 */

#include "embbinder/core/endpoint.hpp"
#include "embbinder/core/binder.hpp"
#include "embbinder/core/log_port.hpp"

namespace embbinder::core {

using namespace embbinder::port;

Endpoint::Endpoint() = default;

Endpoint::~Endpoint() { close(); }

embbinder::core::Result<void> Endpoint::open(const char *endpoint_name,
                                             const TransportConfig &cfg) {
  if (m_transport) {
    close();
  }

  m_max_message_size = cfg.max_message_size;
  m_rx_buffer.resize(m_max_message_size);
  m_tx_buffer.resize(m_max_message_size);

  auto res = Transport::open(m_transport, endpoint_name, cfg);
  if (res != embbinder::port::Result::SUCCESS) {
    log(LogLevel::ERROR, "Endpoint", "Failed to open transport: %d", res);
    return ErrorCode::INTERNAL_ERROR; // Convert port Result to ErrorCode if
                                      // needed, or map it.
                                      // Simplified mapping:
                                      // TODO: Better error mapping
  }

  // Check if open succeeded
  if (!m_transport) {
    return ErrorCode::INTERNAL_ERROR;
  }

  return {};
}

void Endpoint::close() {
  if (m_transport) {
    Transport::close(m_transport);
    m_transport = nullptr;
  }
}

embbinder::core::Result<bool> Endpoint::poll(time_ms_t timeout_ms) {
  if (!m_transport) {
    return ErrorCode::INVALID_OPERATION;
  }

  size_t received_size = 0;
  auto res = Transport::receive(m_transport, m_rx_buffer.data(),
                                m_rx_buffer.size(), received_size, timeout_ms);

  if (res == embbinder::port::Result::TIMEOUT) {
    return false;
  }
  if (res != embbinder::port::Result::SUCCESS) {
    log(LogLevel::WARNING, "Endpoint", "Receive error: %d", res);
    return ErrorCode::INTERNAL_ERROR;
  }

  if (received_size == 0) {
    return false;
  }

  // Process via Binder
  uint32_t response_size = 0;
  // We reuse m_tx_buffer for response
  auto binder_res = Binder::instance().process_message(
      m_rx_buffer.data(), received_size, m_tx_buffer.data(), m_tx_buffer.size(),
      response_size);

  if (binder_res.is_ok() && response_size > 0) {
    // Send response
    auto send_res = Transport::send(m_transport, m_tx_buffer.data(),
                                    response_size, 1000); // 1s timeout for send
    if (send_res != embbinder::port::Result::SUCCESS) {
      log(LogLevel::ERROR, "Endpoint", "Failed to send response: %d", send_res);
      return ErrorCode::INTERNAL_ERROR;
    }
  }

  return true;
}

bool Endpoint::is_open() const { return m_transport != nullptr; }

} // namespace embbinder::core
