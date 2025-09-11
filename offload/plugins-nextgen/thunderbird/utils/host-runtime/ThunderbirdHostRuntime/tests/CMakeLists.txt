# Set possible external build directories for device runtime artifacts
set(DEVICE_RUNTIME_BUILD_DIR_1 "/home/mbrothers/localstore/zephyr/riscv-inspire/build/tmp_zephyr/work/riscv64-inspire-elf/zephyr-shell-loader/4.1.0+git/build")
set(DEVICE_RUNTIME_BUILD_DIR_2 "/mnt/localstore/zephyr/riscv-inspire/build/tmp_zephyr/work/riscv64-inspire-elf/zephyr-shell-loader/4.1.0+git/build")

# Check which build directory exists and use it
if(EXISTS "${DEVICE_RUNTIME_BUILD_DIR_1}/include/generated")
    set(DEVICE_RUNTIME_BUILD_DIR "${DEVICE_RUNTIME_BUILD_DIR_1}")
elseif(EXISTS "${DEVICE_RUNTIME_BUILD_DIR_2}/include/generated")
    set(DEVICE_RUNTIME_BUILD_DIR "${DEVICE_RUNTIME_BUILD_DIR_2}")
else()
    message(FATAL_ERROR "Neither device runtime build directory found:\n"
                       "  ${DEVICE_RUNTIME_BUILD_DIR_1}/include/generated\n"
                       "  ${DEVICE_RUNTIME_BUILD_DIR_2}/include/generated\n"
                       "Please ensure ThunderbirdDeviceRuntime is built first.")
endif()

# Check that the required artifacts exist
set(HELLO_WORLD_EXT_PATH "${DEVICE_RUNTIME_BUILD_DIR}/llext/hello_world.llext")
set(HELLO_WORLD_INC_PATH "${DEVICE_RUNTIME_BUILD_DIR}/include/generated/hello_world_ext.inc")

if(NOT EXISTS ${HELLO_WORLD_INC_PATH})
    message(FATAL_ERROR "Required artifact not found: ${HELLO_WORLD_INC_PATH}\n"
                       "Please build ThunderbirdDeviceRuntime first.")
endif()

# Test executable
add_executable(test_thunderbird_runtime
    test_thunderbird_runtime.cpp
)

target_link_libraries(test_thunderbird_runtime
    ThunderbirdHostRuntime
    DataTransferEngine
)

target_include_directories(test_thunderbird_runtime
    PRIVATE
        ../include
        ../../../DataTransferEngine/include
        ../../../ThunderbirdDeviceRuntime/include
        ${DEVICE_RUNTIME_BUILD_DIR}/include/generated  # Include generated headers
)


# Test executable for flat binary runtime flow
add_executable(test_flat_binary
    test_flat_binary.cpp
)

target_link_libraries(test_flat_binary
    ThunderbirdHostRuntime
    DataTransferEngine
)

target_include_directories(test_flat_binary
    PRIVATE
        ../include
        ../../../DataTransferEngine/include
        ../../../ThunderbirdDeviceRuntime/include
)


