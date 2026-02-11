/**
 * @file span.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-23
 * @brief A lightweight, non-owning reference to a contiguous sequence of
 * objects. Compatible with C++17.
 */

#ifndef EMBBINDER_UTILS_SPAN_HPP
#define EMBBINDER_UTILS_SPAN_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace embbinder::utils {

// A simplified implementation of std::span (C++20) for C++17
template <typename T> class Span {
public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using iterator = pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;

  // Constructors
  constexpr Span() noexcept : m_ptr(nullptr), m_size(0) {}

  constexpr Span(pointer ptr, size_type count) : m_ptr(ptr), m_size(count) {}

  constexpr Span(pointer first, pointer last)
      : m_ptr(first), m_size(last - first) {}

  template <std::size_t N>
  constexpr Span(T (&arr)[N]) noexcept : m_ptr(arr), m_size(N) {}

  template <typename U, std::size_t N>
  constexpr Span(std::array<U, N> &arr) noexcept
      : m_ptr(arr.data()), m_size(N) {}

  template <typename U, std::size_t N>
  constexpr Span(const std::array<U, N> &arr) noexcept
      : m_ptr(arr.data()), m_size(N) {}

  // Observers
  constexpr pointer data() const noexcept { return m_ptr; }
  constexpr size_type size() const noexcept { return m_size; }
  constexpr size_type size_bytes() const noexcept { return m_size * sizeof(T); }
  constexpr bool empty() const noexcept { return m_size == 0; }

  // Element access
  constexpr reference operator[](size_type idx) const { return m_ptr[idx]; }
  constexpr reference front() const { return m_ptr[0]; }
  constexpr reference back() const { return m_ptr[m_size - 1]; }

  // Iterator support
  constexpr iterator begin() const noexcept { return m_ptr; }
  constexpr iterator end() const noexcept { return m_ptr + m_size; }
  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }
  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  // Subviews
  constexpr Span<T> first(size_type count) const {
    return Span<T>(m_ptr, count);
  }
  constexpr Span<T> last(size_type count) const {
    return Span<T>(m_ptr + (m_size - count), count);
  }
  constexpr Span<T>
  subspan(size_type offset,
          size_type count = static_cast<size_type>(-1)) const {
    if (count == static_cast<size_type>(-1)) {
      return Span<T>(m_ptr + offset, m_size - offset);
    }
    return Span<T>(m_ptr + offset, count);
  }

  // Bytes view (as const uint8_t)
  Span<const uint8_t> as_bytes() const noexcept {
    return Span<const uint8_t>(reinterpret_cast<const uint8_t *>(m_ptr),
                               size_bytes());
  }

private:
  pointer m_ptr;
  size_type m_size;
};

} // namespace embbinder::utils

#endif // EMBBINDER_UTILS_SPAN_HPP
