/**
 * @file service.hpp
 * @author Ergün CÜCÜK
 * @brief Service interface implemented by user services.
 */

#ifndef EMBBINDER_CORE_SERVICE_HPP
#define EMBBINDER_CORE_SERVICE_HPP

#include <cstdint>

namespace embbinder::core {

class IService {
public:
  virtual ~IService() = default;

  /**
   * @brief Handle one request.
   *
   * @param response_capacity Bytes available at response_buf; never write more.
   * @param response_size [out] Bytes written, 0 for no payload.
   */
  virtual void handle(uint32_t method_id, const uint8_t *payload,
                      uint32_t payload_size, uint8_t *response_buf,
                      uint32_t response_capacity, uint32_t &response_size) = 0;
};

} // namespace embbinder::core

#endif // EMBBINDER_CORE_SERVICE_HPP
