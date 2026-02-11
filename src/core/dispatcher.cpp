/**
 * @file dispatcher.cpp
 * @author Ergün CÜCÜK
 * @brief
 * @version 0.1
 * @date 2025-12-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "embbinder/core/dispatcher.hpp"

namespace embbinder::core {

void Dispatcher::register_service(uint32_t service_id, IService *service) {
  m_services[m_count++] = {service_id, service};
}

bool Dispatcher::dispatch(const MessageHeader &hdr, const uint8_t *payload,
                          uint8_t *response, uint32_t &response_size) {
  for (uint32_t i = 0; i < m_count; ++i) {
    if (m_services[i].service_id == hdr.service_id) {
      m_services[i].service->handle(hdr.method_id, payload, hdr.payload_size,
                                    response, response_size);
      return true;
    }
  }
  return false;
}

} // namespace embbinder::core