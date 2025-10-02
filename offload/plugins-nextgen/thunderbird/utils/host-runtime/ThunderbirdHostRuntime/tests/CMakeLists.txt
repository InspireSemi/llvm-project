# Set possible external build directories for device runtime artifacts
set(DEVICE_RUNTIME_BUILD_DIR_1 "/home/mbrothers/localstore/zephyr/riscv-inspire/build/tmp_zephyr/work/riscv64-inspire-elf/zephyr-shell-loader/4.1.0+git/build")
set(DEVICE_RUNTIME_BUILD_DIR_2 "/scratch/ksuarez/tbird_runtime_Demo/build/tmp_zephyr/work/riscv64-inspire-elf/zephyr-shell-loader/4.1.0+git/build")

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
set(SAXPY_LLEXT_EXT_PATH "${DEVICE_RUNTIME_BUILD_DIR}/llext/saxpy_llext_ext.llext")
set(SAXPY_LLEXT_INC_PATH "${DEVICE_RUNTIME_BUILD_DIR}/include/generated/saxpy_llext_ext.inc")

if(NOT EXISTS ${SAXPY_LLEXT_INC_PATH})
    message(FATAL_ERROR "Required artifact not found: ${SAXPY_LLEXT_INC_PATH}\n"
                       "Please build ThunderbirdDeviceRuntime first.")
endif()


# Test executable for flat binary runtime flow
add_executable(test_module_w_rt_args
    test_module_w_rt_args.cpp
)

target_link_libraries(test_module_w_rt_args
    ThunderbirdHostRuntime
    DataTransferEngine
)

target_include_directories(test_module_w_rt_args
    PRIVATE
        ../include
        ../../../DataTransferEngine/include
        ../../../ThunderbirdDeviceRuntime/include
        ${DEVICE_RUNTIME_BUILD_DIR}/include/generated  # Include generated headers like the inc version of an llext.
)

# Test executable for BatchUtils internal API
add_executable(test_batch_utils
    test_batch_utils.cpp
)

target_link_libraries(test_batch_utils
    ThunderbirdHostRuntime
)

target_include_directories(test_batch_utils
    PRIVATE
        ../include
)

