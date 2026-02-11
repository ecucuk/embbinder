/**
 * @file registry.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Service Registry generic class.
 */

#ifndef EMBBINDER_CORE_REGISTRY_HPP
#define EMBBINDER_CORE_REGISTRY_HPP

#include "dispatcher.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace embbinder::core {

class ServiceRegistry {
public:
  static ServiceRegistry &instance();

  bool add_service(uint32_t service_id, IService *service);
  IService *get_service(uint32_t service_id);
  void remove_service(uint32_t service_id);

  // Optional: string-based lookup if needed
  bool add_service(const std::string &name, IService *service);
  IService *get_service(const std::string &name);

private:
  ServiceRegistry() = default;
  ~ServiceRegistry() = default;

  ServiceRegistry(const ServiceRegistry &) = delete;
  ServiceRegistry &operator=(const ServiceRegistry &) = delete;

  // We can use the simple array-based approach from Dispatcher or a Map.
  // Since this is C++, let's use std::map for flexibility unless constrained.
  // Note: In embedded, map might be heavy, but we are designing generic C++
  // core.
  std::map<uint32_t, IService *> m_services_by_id;
  std::map<std::string, IService *> m_services_by_name;

  // Mutex for thread safety
  // Assuming std::mutex is available (standard C++11).
  // If on bare metal, this needs a port implementation layer.
  // For now, using standard mutex.
  std::mutex m_mutex;
};

} // namespace embbinder::core

#endif // EMBBINDER_CORE_REGISTRY_HPP
