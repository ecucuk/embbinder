/**
 * @file byteorder.cpp
 * @author Ergün CÜCÜK
 * @date 2026-01-23
 * @brief Implementation of Byte order utility functions.
 */

#include "embbinder/utils/byteorder.hpp"

namespace embbinder::utils {

bool ByteOrder::is_little_endian() {
  static const uint16_t val = 0x0001;
  return reinterpret_cast<const uint8_t *>(&val)[0] == 0x01;
}

bool ByteOrder::is_big_endian() { return !is_little_endian(); }

uint16_t ByteOrder::swap16(uint16_t val) {
  return static_cast<uint16_t>((val << 8) | (val >> 8));
}

uint32_t ByteOrder::swap32(uint32_t val) {
  return ((val << 24) & 0xFF000000) | ((val << 8) & 0x00FF0000) |
         ((val >> 8) & 0x0000FF00) | ((val >> 24) & 0x000000FF);
}

uint64_t ByteOrder::swap64(uint64_t val) {
  val = ((val << 8) & 0xFF00FF00FF00FF00ULL) |
        ((val >> 8) & 0x00FF00FF00FF00FFULL);
  val = ((val << 16) & 0xFFFF0000FFFF0000ULL) |
        ((val >> 16) & 0x0000FFFF0000FFFFULL);
  return (val << 32) | (val >> 32);
}

} // namespace embbinder::utils
