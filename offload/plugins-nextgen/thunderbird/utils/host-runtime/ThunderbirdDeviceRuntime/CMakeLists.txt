# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Enable C11 for better static assertions while maintaining C99 compatibility
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)  # Keep ON for __attribute__((packed))

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fs_shell)

# Main application sources
FILE(GLOB app_sources src/*.c)

# Library sources
FILE(GLOB ivshmem_sources lib/ivshmem/src/*.c)
FILE(GLOB messaging_sources lib/messaging/src/*.c)
FILE(GLOB device_sources lib/device/src/*.c)
FILE(GLOB memory_sources lib/memory/src/*.c)
# Test sources
FILE(GLOB common_test_sources tests/common/src/*.c)
FILE(GLOB unit_test_sources tests/unit/src/*.c)
FILE(GLOB integration_test_sources tests/integration/src/*.c)
FILE(GLOB demo_test_sources tests/demo/src/*.c)

# Add main sources to the app target
target_sources(app PRIVATE 
    ${app_sources}
    ${ivshmem_sources}
    ${messaging_sources}
    ${device_sources}
    ${memory_sources}
    ${common_test_sources}
    ${unit_test_sources}
)

# Include directories for main application
target_include_directories(app PRIVATE 
    .                        # Project root for simplified includes
    include
    lib/ivshmem/include
    lib/messaging/include
    lib/common/include
    lib/device/include
    lib/memory/include
    tests/common/include
    tests/unit/include
    tests/integration/include
    tests/demo/include
    ${PROJECT_BINARY_DIR}/include/generated  # Generated include files
)

# Add llext subdirectory
add_subdirectory(tests/llext/hello_world_mod)

# Ensure the llext is built before the main app so the generated include is available
add_dependencies(app hello_world_ext)

