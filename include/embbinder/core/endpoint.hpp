/**
 * @file endpoint.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-23
 * @brief Endpoint class for managing transport and message processing.
 */

#ifndef EMBBINDER_CORE_ENDPOINT_HPP
#define EMBBINDER_CORE_ENDPOINT_HPP

#include "embbinder/core/result.hpp"
#include "embbinder/port/port.hpp"
#include <vector>

namespace embbinder::core {

class Endpoint {
public:
  Endpoint();
  ~Endpoint();

  Endpoint(const Endpoint &) = delete;
  Endpoint &operator=(const Endpoint &) = delete;

  /**
   * @brief Open the endpoint transport.
   * @param endpoint_name Name of the endpoint/device to open.
   * @param cfg Transport configuration.
   * @return Result<void>
   */
  Result<void> open(const char *endpoint_name,
                    const embbinder::port::TransportConfig &cfg);

  /**
   * @brief Close the endpoint.
   */
  void close();

  /**
   * @brief Poll for incoming messages and process them.
   *        This reads from transport, calls Binder to process, and sends
   * response if any.
   * @param timeout_ms Timeout for receiving.
   * @return Result<bool> True if a message was processed, False if timeout/no
   * message.
   */
  Result<bool> poll(embbinder::port::time_ms_t timeout_ms = 0);

  /**
   * @brief Check if endpoint is open.
   */
  bool is_open() const;

private:
  embbinder::port::TransportHandle m_transport = nullptr;
  std::vector<uint8_t> m_rx_buffer;
  std::vector<uint8_t> m_tx_buffer;
  size_t m_max_message_size = 0;
};

} // namespace embbinder::core

#endif // EMBBINDER_CORE_ENDPOINT_HPP
