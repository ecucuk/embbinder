/**
 * @file transport_queue.cpp
 * @brief Transport implementation using RTOS queues (Template/Stub).
 */

#include "embbinder/port/port.hpp"
#include <cstring>

// Note: In a real RTOS (FreeRTOS, etc.), we would include the RTOS headers
// here. #include "FreeRTOS.h" #include "queue.h"

namespace embbinder::port {

struct QueueTransport {
  // QueueHandle_t rx_queue;
  // QueueHandle_t tx_queue;
  void *dummy_queue_handle;
};

// Simulated mock for compilation and structure
Result Transport::open(TransportHandle &out, const char *endpoint_name,
                       const TransportConfig &cfg) {
  // In RTOS, "opening" a transport might mean creating or retrieving a queue
  // handle.
  (void)endpoint_name;
  (void)cfg;

  QueueTransport *t = new QueueTransport;
  // t->rx_queue = xQueueCreate(10, cfg.max_message_size);
  t->dummy_queue_handle = nullptr; // implementations would set this

  out = t;
  return Result::SUCCESS;
}

void Transport::close(TransportHandle handle) {
  if (!handle)
    return;
  QueueTransport *t = static_cast<QueueTransport *>(handle);
  // vQueueDelete(t->rx_queue);
  delete t;
}

Result Transport::send(TransportHandle handle, const uint8_t *data, size_t size,
                       time_ms_t timeout_ms) {
  if (!handle || !data)
    return Result::INVALID_ARGUMENT;
  // QueueTransport *t = static_cast<QueueTransport *>(handle);

  // xQueueSend(t->tx_queue, data, pdMS_TO_TICKS(timeout_ms));

  // Stub behavior
  (void)size;
  (void)timeout_ms;
  return Result::SUCCESS;
}

Result Transport::receive(TransportHandle handle, uint8_t *buffer,
                          size_t buffer_size, size_t &out_size,
                          time_ms_t timeout_ms) {
  if (!handle || !buffer)
    return Result::INVALID_ARGUMENT;
  // QueueTransport *t = static_cast<QueueTransport *>(handle);

  // xQueueReceive(t->rx_queue, buffer, pdMS_TO_TICKS(timeout_ms));

  // Stub behavior
  (void)buffer_size;
  (void)timeout_ms;
  out_size = 0;
  return Result::TIMEOUT; // Nothing to receive in stub
}

} // namespace embbinder::port
