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
    src/core/dispatcher.cpp
    src/core/registry.cpp
    src/core/serializer.cpp
    src/core/log_port.cpp
    src/utils/byteorder.cpp
    src/core/endpoint.cpp
)

add_library(embbinder_core ${CORE_SOURCES})

# Example
add_executable(basic_service examples/basic_service.cpp)
target_link_libraries(basic_service embbinder_core)
