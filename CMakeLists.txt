cmake_minimum_required(VERSION 3.10)
project(embbinder)

set(CMAKE_CXX_STANDARD 17)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called build) and run CMake from there.")
endif()

include_directories(include)

# Core Library
set(CORE_SOURCES
    src/core/binder.cpp
    src/core/registry.cpp
    src/core/serializer.cpp
    src/core/log_port.cpp
    src/utils/byteorder.cpp
    src/core/endpoint.cpp
)

add_library(embbinder_core ${CORE_SOURCES})

# POSIX port (Clock, Thread, Mutex, Semaphore, Memory, UDS Transport).
# Link it next to embbinder_core when using Endpoint or other port APIs.
if(UNIX)
    find_package(Threads REQUIRED)
    add_library(embbinder_port_posix
        src/ports/posix/src/port_posix.cpp
        src/ports/posix/transport_uds.cpp
    )
    target_include_directories(embbinder_port_posix PUBLIC src/ports/posix/include)
    target_link_libraries(embbinder_port_posix PUBLIC Threads::Threads)
endif()

# Example
add_executable(basic_service examples/basic_service.cpp)
target_link_libraries(basic_service embbinder_core)

# Tests
enable_testing()
add_executable(test_binder tests/test_binder.cpp)
target_link_libraries(test_binder embbinder_core)
add_test(NAME test_binder COMMAND test_binder)

if(UNIX)
    add_executable(test_uds_endpoint tests/test_uds_endpoint.cpp)
    target_link_libraries(test_uds_endpoint embbinder_core embbinder_port_posix)
    add_test(NAME test_uds_endpoint COMMAND test_uds_endpoint)
endif()
