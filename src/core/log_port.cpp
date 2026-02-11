/**
 * @file log_port.cpp
 * @author Ergün CÜCÜK
 * @brief
 * @version 0.1
 * @date 2025-12-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "embbinder/core/log_port.hpp"
#include <cstdio>

namespace embbinder::port {
static constexpr const char *TAG = "EMBBINDER";

static LogFunction g_log_function = nullptr;

void Logger::set_log_function(LogFunction func) { g_log_function = func; }

void Logger::log(LogLevel level, const char *tag, const char *message) {
  if (g_log_function) {
    g_log_function(level, tag, message);
  } else {
    // Default logging to standard output
    const char *level_str = "";
    switch (level) {
    case LogLevel::DEBUG:
      level_str = "DEBUG";
      break;
    case LogLevel::INFO:
      level_str = "INFO";
      break;
    case LogLevel::WARNING:
      level_str = "WARNING";
      break;
    case LogLevel::ERROR:
      level_str = "ERROR";
      break;
    case LogLevel::FATAL:
      level_str = "FATAL";
      break;
    default:
      level_str = "UNKNOWN";
      break;
    }
    std::printf("[%s] [%s]: %s\n", level_str, tag ? tag : TAG, message);
  }
}
} // namespace embbinder::port
