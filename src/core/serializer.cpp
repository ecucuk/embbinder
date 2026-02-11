/**
 * @file serializer.cpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Implementation of Serializer class.
 */

#include "embbinder/core/serializer.hpp"
#include "embbinder/utils/byteorder.hpp"

namespace embbinder::core {

Serializer::Serializer() = default;
Serializer::~Serializer() = default;

Serializer::Serializer(Serializer &&) noexcept = default;
Serializer &Serializer::operator=(Serializer &&) noexcept = default;

void Serializer::reset() {
  m_buffer.clear();
  m_read_pos = 0;
  m_external_buffer = nullptr;
  m_external_size = 0;
}

const uint8_t *Serializer::data() const {
  if (m_external_buffer) {
    return m_external_buffer;
  }
  return m_buffer.data();
}

size_t Serializer::size() const {
  if (m_external_buffer) {
    return m_external_size;
  }
  return m_buffer.size();
}

void Serializer::attach_buffer(const uint8_t *buffer, size_t size) {
  m_buffer.clear();
  m_read_pos = 0;
  m_external_buffer = buffer;
  m_external_size = size;
}

// Writers

template <typename T> void Serializer::write_pod(T val) {
  T le_val = embbinder::utils::ByteOrder::to_little_endian(val);
  const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&le_val);
  m_buffer.insert(m_buffer.end(), ptr, ptr + sizeof(T));
}

void Serializer::write_int32(int32_t val) { write_pod(val); }
void Serializer::write_uint32(uint32_t val) { write_pod(val); }
void Serializer::write_int64(int64_t val) { write_pod(val); }
void Serializer::write_uint64(uint64_t val) { write_pod(val); }
void Serializer::write_float(float val) { write_pod(val); }
void Serializer::write_double(double val) { write_pod(val); }
void Serializer::write_bool(bool val) { write_pod<uint8_t>(val ? 1 : 0); }

void Serializer::write_string(const std::string &str) {
  write_uint32(static_cast<uint32_t>(str.size()));
  if (!str.empty()) {
    write_data(str.data(), str.size());
  }
  // Null terminator? Binder usually doesn't, but Pascal-style strings.
  // Let's stick to size + data + null terminator for safety if C-string
  // compatibility needed? For now: size + data. Actually, usually it's helpful
  // to add a null terminator for easy C-string usage, but we wrote size, so
  // strict binary. Let's implement padding if needed (Binder usually pads to 4
  // bytes). Skipping padding for simplicity in this initial version.
}

void Serializer::write_string(const char *str) {
  if (str) {
    write_string(std::string(str));
  } else {
    write_uint32(0);
  }
}

void Serializer::write_data(const void *data, size_t size) {
  const uint8_t *ptr = static_cast<const uint8_t *>(data);
  m_buffer.insert(m_buffer.end(), ptr, ptr + size);
}

// Readers

template <typename T> Result<T> Serializer::read_pod() {
  const uint8_t *base = m_external_buffer ? m_external_buffer : m_buffer.data();
  size_t total_size = m_external_buffer ? m_external_size : m_buffer.size();

  if (m_read_pos + sizeof(T) > total_size) {
    return ErrorCode::BUFFER_TOO_SMALL;
  }

  T val;
  std::memcpy(&val, base + m_read_pos, sizeof(T));
  m_read_pos += sizeof(T);
  return embbinder::utils::ByteOrder::from_little_endian(val);
}

Result<int32_t> Serializer::read_int32() { return read_pod<int32_t>(); }
Result<uint32_t> Serializer::read_uint32() { return read_pod<uint32_t>(); }
Result<int64_t> Serializer::read_int64() { return read_pod<int64_t>(); }
Result<uint64_t> Serializer::read_uint64() { return read_pod<uint64_t>(); }
Result<float> Serializer::read_float() { return read_pod<float>(); }
Result<double> Serializer::read_double() { return read_pod<double>(); }

Result<bool> Serializer::read_bool() {
  auto res = read_pod<uint8_t>();
  if (res.is_error())
    return res.error();
  return (res.value() != 0);
}

Result<std::string> Serializer::read_string() {
  auto size_res = read_uint32();
  if (size_res.is_error())
    return size_res.error();
  uint32_t len = size_res.value();

  const uint8_t *base = m_external_buffer ? m_external_buffer : m_buffer.data();
  size_t total_size = m_external_buffer ? m_external_size : m_buffer.size();

  if (m_read_pos + len > total_size) {
    return ErrorCode::BUFFER_TOO_SMALL;
  }

  std::string str(reinterpret_cast<const char *>(base + m_read_pos), len);
  m_read_pos += len;
  return str;
}

} // namespace embbinder::core
