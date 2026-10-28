/**
 * @file check.hpp
 * @brief Minimal assert that also works with NDEBUG.
 */

#ifndef EMBBINDER_TESTS_CHECK_HPP
#define EMBBINDER_TESTS_CHECK_HPP

#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,    \
                   #cond);                                                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

#endif // EMBBINDER_TESTS_CHECK_HPP
