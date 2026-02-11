/**
 * @file static_vector.hpp
 * @author Ergün CÜCÜK
 * @date 2026-01-23
 * @brief Fixed-capacity vector implementation that uses stack storage.
 *        Avoids dynamic memory allocation. Compatible with C++17.
 */

#ifndef EMBBINDER_UTILS_STATIC_VECTOR_HPP
#define EMBBINDER_UTILS_STATIC_VECTOR_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace embbinder::utils {

template <typename T, std::size_t Capacity> class StaticVector {
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T &;
  using const_reference = const T &;
  using pointer = T *;
  using const_pointer = const T *;
  using iterator = T *;
  using const_iterator = const T *;

  // Constructors
  constexpr StaticVector() noexcept : m_size(0) {}

  constexpr StaticVector(std::initializer_list<T> list) : m_size(0) {
    assert(list.size() <= Capacity && "Initializer list exceeds capacity");
    for (const auto &item : list) {
      push_back(item);
    }
  }

  // Copy/Move constructors
  // For simplicity, we implement default copy/move semantics if T permits
  // Or explicitly handle element-wise copy/move if needed.
  // Here we assume T is copyable/movable.

  // Assignment logic handled by copy/move assignment operators implicitly if
  // member m_data (array) handles it? No, raw storage needs manual management
  // for non-trivial types. For trivial types, simple copy works.
  // To keep implementation simple and portable, we'll use
  // `std::aligned_storage` or `std::array<T, N>` if T is default constructible.
  // Using `std::array<char, sizeof(T) * Capacity>` for uninitialized storage is
  // better but harder to implement in a single file quickly without placement
  // new boilerplate.
  // Let's assume T is default constructible for now and use std::array<T,
  // Capacity> for simplicity as per common embedded use cases (primitives,
  // structs).

  // Observers
  constexpr size_type size() const noexcept { return m_size; }
  constexpr size_type capacity() const noexcept { return Capacity; }
  constexpr bool empty() const noexcept { return m_size == 0; }
  constexpr bool full() const noexcept { return m_size == Capacity; }
  constexpr size_type max_size() const noexcept { return Capacity; }

  constexpr pointer data() noexcept { return m_data.data(); }
  constexpr const_pointer data() const noexcept { return m_data.data(); }

  // Element access
  constexpr reference operator[](size_type idx) {
    assert(idx < m_size);
    return m_data[idx];
  }

  constexpr const_reference operator[](size_type idx) const {
    assert(idx < m_size);
    return m_data[idx];
  }

  constexpr reference at(size_type idx) {
    if (idx >= m_size) {
      // In embedded we might not want exceptions, but for standard compliance:
      // throw std::out_of_range("StaticVector::at");
      // Fallback to assert/terminate
      assert(false && "StaticVector::at out of range");
    }
    return m_data[idx];
  }

  constexpr const_reference at(size_type idx) const {
    if (idx >= m_size) {
      assert(false && "StaticVector::at out of range");
    }
    return m_data[idx];
  }

  constexpr reference front() {
    assert(m_size > 0);
    return m_data[0];
  }
  constexpr const_reference front() const {
    assert(m_size > 0);
    return m_data[0];
  }

  constexpr reference back() {
    assert(m_size > 0);
    return m_data[m_size - 1];
  }
  constexpr const_reference back() const {
    assert(m_size > 0);
    return m_data[m_size - 1];
  }

  // Modifiers
  void push_back(const T &value) {
    assert(m_size < Capacity && "StaticVector::push_back full");
    if (m_size < Capacity) {
      m_data[m_size] = value;
      m_size++;
    }
  }

  void push_back(T &&value) {
    assert(m_size < Capacity && "StaticVector::push_back full");
    if (m_size < Capacity) {
      m_data[m_size] = std::move(value);
      m_size++;
    }
  }

  template <typename... Args> void emplace_back(Args &&...args) {
    assert(m_size < Capacity && "StaticVector::emplace_back full");
    if (m_size < Capacity) {
      // For std::array storage, we assign to the default-constructed element
      m_data[m_size] = T(std::forward<Args>(args)...);
      // If we used uninitialized storage, we would use placement new:
      // new (&m_data[m_size]) T(std::forward<Args>(args)...);
      m_size++;
    }
  }

  void pop_back() {
    assert(m_size > 0);
    if (m_size > 0) {
      m_size--;
      // Destructor call not needed for std::array based impl if we assume
      // trivial or we don't care about destruction order immediately on pop
      // (it will happen on vector destruction).
      // If T needs immediate destruction, this impl needs refinement.
    }
  }

  void clear() noexcept { m_size = 0; }

  // Iterators
  iterator begin() noexcept { return m_data.data(); }
  const_iterator begin() const noexcept { return m_data.data(); }
  const_iterator cbegin() const noexcept { return m_data.data(); }

  iterator end() noexcept { return m_data.data() + m_size; }
  const_iterator end() const noexcept { return m_data.data() + m_size; }
  const_iterator cend() const noexcept { return m_data.data() + m_size; }

private:
  // Using std::array requires T to be default constructible.
  // This is a trade-off for simplicity in a single-header "utils"
  // implementation.
  std::array<T, Capacity> m_data;
  size_type m_size;
};

} // namespace embbinder::utils

#endif // EMBBINDER_UTILS_STATIC_VECTOR_HPP
