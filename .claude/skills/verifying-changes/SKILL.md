---
name: verifying-changes
description: Use when about to report that a change to EmbBinder works, is fixed, or is ready to commit, or when adding logic that needs a test.
---

# Verifying changes in EmbBinder

Tests are small: `tests/test_binder.cpp` (core) and `tests/test_uds_endpoint.cpp` (Endpoint over the POSIX port). The RTOS port is never compiled.

## Steps

Use a build dir outside the source tree (in-source builds are rejected):

```bash
B=$(mktemp -d)
cmake -S . -B "$B" -DCMAKE_CXX_FLAGS="-Wall -Wextra" && cmake --build "$B" -j8
(cd "$B" && ctest --output-on-failure)
"$B/basic_service"   # last line must be: Client: Result = 30
```

The build must produce zero warnings. `src/ports/rtos/` needs FreeRTOS headers and cannot be checked here — say so in your report.

Format check on touched files (must print nothing):

```bash
clang-format --dry-run <files>
```

## When the change has logic

Add a `CHECK` to the matching test file, or a new `tests/test_<area>.cpp` registered with `add_test` in `CMakeLists.txt`. For wire-format code, test a truncated message and a `payload_size` larger than the input.

Prove the test bites: temporarily revert the fix, see the `CHECK` fail, restore.

## Report

State: ctest result, warnings (expected: none), what could not be compiled.
