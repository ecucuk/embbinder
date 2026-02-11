/**
 * @file dispatcher.hpp
 * @author Ergün CÜCÜK
 * @brief
 * @version 0.1
 * @date 2025-12-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef EMBBINDER_CORE_DISPATCHER_HPP
#define EMBBINDER_CORE_DISPATCHER_HPP

#include "message.hpp"
#include <cstdint>

namespace embbinder::core {

class IService {
public:
  virtual ~IService() = default;

  virtual void handle(uint32_t method_id, const uint8_t *payload,
                      uint32_t payload_size, uint8_t *response_buf,
                      uint32_t &response_size) = 0;
};

class Dispatcher {
public:
  void register_service(uint32_t service_id, IService *service);

  bool dispatch(const MessageHeader &hdr, const uint8_t *payload,
                uint8_t *response, uint32_t &response_size);

private:
  struct Entry {
    uint32_t service_id;
    IService *service;
  };

  Entry m_services[8U]; // MCU-friendly fixed table
  uint32_t m_count{0U};
};

} // namespace embbinder::core

#endif // EMBBINDER_CORE_DISPATCHER_HPP