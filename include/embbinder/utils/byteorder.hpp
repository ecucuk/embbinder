/**
 * @file byteorder.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-23
 * @brief Byte order utility functions.
 */

#ifndef EMBBINDER_UTILS_BYTEORDER_HPP
#define EMBBINDER_UTILS_BYTEORDER_HPP

#include <cstdint>
#include <type_traits>

namespace embbinder::utils {

enum class Endian {
  Little,
  Big,
  Native = Little // Assumption for most embedded ARM/x86. We can detect this at
                  // runtime or compile time.
};

class ByteOrder {
public:
  static bool is_little_endian();
  static bool is_big_endian();

  static uint16_t swap16(uint16_t val);
  static uint32_t swap32(uint32_t val);
  static uint64_t swap64(uint64_t val);

  // Host to Network (Little Endian as network order for EmbBinder?)
  // Binder typically uses LE. Let's assume LE is the wire format.
  template <typename T> static T to_little_endian(T val) {
    if (is_little_endian())
      return val;
    return swap_type(val);
  }

  template <typename T> static T from_little_endian(T val) {
    if (is_little_endian())
      return val;
    return swap_type(val);
  }

private:
  template <typename T> static T swap_type(T val) {
    if constexpr (sizeof(T) == 2)
      return static_cast<T>(swap16(static_cast<uint16_t>(val)));
    if constexpr (sizeof(T) == 4)
      return static_cast<T>(swap32(static_cast<uint32_t>(val)));
    if constexpr (sizeof(T) == 8)
      return static_cast<T>(swap64(static_cast<uint64_t>(val)));
    return val;
  }
};

} // namespace embbinder::utils

#endif // EMBBINDER_UTILS_BYTEORDER_HPP
