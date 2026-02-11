/**
 * @file log_port.hpp
 * @author Ergün CÜCÜK
 * @brief
 * @version 0.1
 * @date 2025-12-12
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef EMBBINDER_CORE_LOG_PORT_HPP
#define EMBBINDER_CORE_LOG_PORT_HPP

#include "embbinder/port/port.hpp"
#include <cstdint>
#include <cstdio>
#include <string>

namespace embbinder::core {

template <typename... Args>
void log(embbinder::port::LogLevel level, const char *tag, const char *fmt,
         Args... args) {
  char buffer[256];
  std::snprintf(buffer, sizeof(buffer), fmt, args...);
  embbinder::port::Logger::log(level, tag, buffer);
}

inline void log(embbinder::port::LogLevel level, const char *tag,
                const char *msg) {
  embbinder::port::Logger::log(level, tag, msg);
}

} // namespace embbinder::core

#endif // EMBBINDER_CORE_LOG_PORT_HPP