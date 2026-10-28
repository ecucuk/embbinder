/**
 * @file message.hpp
 * @author Ergün CÜCÜK
 * @brief
 * @version 0.1
 * @date 2025-12-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef EMBBINDER_CORE_MESSAGE_HPP
#define EMBBINDER_CORE_MESSAGE_HPP

#include <cstddef>
#include <cstdint>

namespace embbinder::core {

constexpr uint32_t kMagic = 0x454D4242; // 'EMBB'
constexpr uint16_t kVersion = 1U;

enum class MsgType : uint16_t {
  Request = 1,
  Response = 2,
  Event = 3,
  Error = 4, // payload: uint32 little-endian ErrorCode
};

struct MessageHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t type;
  uint32_t service_id;
  uint32_t method_id;
  uint32_t payload_size;
  uint32_t request_id;
};

static_assert(sizeof(MessageHeader) == 24, "ABI break!");

} // namespace embbinder::core

#endif // EMBBINDER_CORE_MESSAGE_HPP