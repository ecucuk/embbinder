/**
 * @file registry.cpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Implementation of ServiceRegistry.
 */

#include "embbinder/core/registry.hpp"

namespace embbinder::core {

ServiceRegistry &ServiceRegistry::instance() {
  static ServiceRegistry s_instance;
  return s_instance;
}

bool ServiceRegistry::add_service(uint32_t service_id, IService *service) {
  if (!service)
    return false;
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_services_by_id.find(service_id) != m_services_by_id.end()) {
    return false; // Already exists
  }
  m_services_by_id[service_id] = service;
  return true;
}

IService *ServiceRegistry::get_service(uint32_t service_id) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_services_by_id.find(service_id);
  if (it != m_services_by_id.end()) {
    return it->second;
  }
  return nullptr;
}

void ServiceRegistry::remove_service(uint32_t service_id) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_services_by_id.erase(service_id);
}

bool ServiceRegistry::add_service(const std::string &name, IService *service) {
  if (!service || name.empty())
    return false;
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_services_by_name.find(name) != m_services_by_name.end()) {
    return false;
  }
  m_services_by_name[name] = service;
  return true;
}

IService *ServiceRegistry::get_service(const std::string &name) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_services_by_name.find(name);
  if (it != m_services_by_name.end()) {
    return it->second;
  }
  return nullptr;
}

} // namespace embbinder::core
