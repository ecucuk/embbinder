/**
 * @file binder.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Main Binder class for service management and message processing.
 */

#ifndef EMBBINDER_CORE_BINDER_HPP
#define EMBBINDER_CORE_BINDER_HPP

#include "dispatcher.hpp"
#include "registry.hpp"
#include "result.hpp"
#include "serializer.hpp"
#include <cstddef>
#include <cstdint>

namespace embbinder {
namespace core {

class Binder {
public:
  /**
   * @brief Get the singleton instance of Binder
   *
   * @return Binder&
   */
  static Binder &instance();

  /**
   * @brief Register a service with a numeric ID
   *
   * @param service_id Unique service ID
   * @param service Pointer to service instance
   * @return true if successful
   */
  bool register_service(uint32_t service_id, IService *service);

  /**
   * @brief Get a service by ID
   *
   * @param service_id Service ID
   * @return IService* or nullptr
   */
  IService *get_service(uint32_t service_id);

  /**
   * @brief Process an incoming raw message packet
   *
   * Parses the header, looks up the service, and dispatches the request.
   *
   * @param input_buffer Raw input data
   * @param input_size Size of input data
   * @param output_buffer Buffer to write response to
   * @param output_capacity Capacity of output buffer
   * @param output_size [out] Actual size of response written
   * @return Result<void> Status of operation
   */
  Result<void> process_message(const uint8_t *input_buffer, size_t input_size,
                               uint8_t *output_buffer, size_t output_capacity,
                               uint32_t &output_size);

private:
  Binder() = default;
  ~Binder() = default;

  Binder(const Binder &) = delete;
  Binder &operator=(const Binder &) = delete;

  // Helper to send error response
  void send_error_response(uint32_t request_id, ErrorCode error,
                           uint8_t *out_buf, size_t out_cap,
                           uint32_t &out_size);
};

} // namespace core
} // namespace embbinder

#endif // EMBBINDER_CORE_BINDER_HPP
