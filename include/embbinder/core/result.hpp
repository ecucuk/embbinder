/**
 * @file result.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-22
 * @brief Result generic class for error handling.
 */

#ifndef EMBBINDER_CORE_RESULT_HPP
#define EMBBINDER_CORE_RESULT_HPP

#include <type_traits>
#include <utility>

namespace embbinder::core {

enum class ErrorCode {
  NONE = 0,
  UNKNOWN,
  INVALID_ARGUMENT,
  OUT_OF_MEMORY,
  BUFFER_TOO_SMALL,
  SERVICE_NOT_FOUND,
  METHOD_NOT_FOUND,
  SERIALIZATION_ERROR,
  DESERIALIZATION_ERROR,
  INTERNAL_ERROR,
  INVALID_OPERATION,
};

template <typename T> class Result {
public:
  Result(const T &val) : m_value(val), m_error(ErrorCode::NONE) {}
  Result(T &&val) : m_value(std::move(val)), m_error(ErrorCode::NONE) {}
  Result(ErrorCode error) : m_value(), m_error(error) {}

  bool is_ok() const { return m_error == ErrorCode::NONE; }
  bool is_error() const { return !is_ok(); }

  const T &value() const { return m_value; }
  ErrorCode error() const { return m_error; }

  // Allow implicit conversion to bool for check
  operator bool() const { return is_ok(); }

private:
  T m_value;
  ErrorCode m_error;
};

// Specialization for void
template <> class Result<void> {
public:
  Result() : m_error(ErrorCode::NONE) {}
  Result(ErrorCode error) : m_error(error) {}

  bool is_ok() const { return m_error == ErrorCode::NONE; }
  bool is_error() const { return !is_ok(); }

  ErrorCode error() const { return m_error; }

  operator bool() const { return is_ok(); }

private:
  ErrorCode m_error;
};

} // namespace embbinder::core

#endif // EMBBINDER_CORE_RESULT_HPP
