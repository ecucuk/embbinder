/**
 * @file serializer.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Serializer class for data serialization/deserialization.
 */

#ifndef EMBBINDER_CORE_SERIALIZER_HPP
#define EMBBINDER_CORE_SERIALIZER_HPP

#include "result.hpp"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace embbinder::core {

class Serializer {
public:
  Serializer();
  ~Serializer();

  // Prevent copying to avoid expensive buffer copies for now if not needed
  Serializer(const Serializer &) = delete;
  Serializer &operator=(const Serializer &) = delete;

  Serializer(Serializer &&) noexcept;
  Serializer &operator=(Serializer &&) noexcept;

  // Reset buffer
  void reset();

  // Writers
  void write_int32(int32_t val);
  void write_uint32(uint32_t val);
  void write_int64(int64_t val);
  void write_uint64(uint64_t val);
  void write_float(float val);
  void write_double(double val);
  void write_string(const std::string &str);
  void write_string(const char *str);
  void write_bool(bool val);

  // Explicit buffer writing
  void write_data(const void *data, size_t size);

  // Readers
  Result<int32_t> read_int32();
  Result<uint32_t> read_uint32();
  Result<int64_t> read_int64();
  Result<uint64_t> read_uint64();
  Result<float> read_float();
  Result<double> read_double();
  Result<std::string> read_string();
  Result<bool> read_bool();

  // Accessors
  const uint8_t *data() const;
  size_t size() const;

  // For reading from an existing buffer (deserialization only mode)
  void attach_buffer(const uint8_t *buffer, size_t size);

private:
  std::vector<uint8_t> m_buffer;
  size_t m_read_pos{0};

  // Non-owning mode for deserialization from external buffer
  const uint8_t *m_external_buffer{nullptr};
  size_t m_external_size{0};

  template <typename T> void write_pod(T val);

  template <typename T> Result<T> read_pod();
};

} // namespace embbinder::core

#endif // EMBBINDER_CORE_SERIALIZER_HPP
