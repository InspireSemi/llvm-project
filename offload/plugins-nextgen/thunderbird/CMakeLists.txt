# Create the library and add the default arguments.
add_target_library(omptarget.rtl.thunderbird THUNDERBIRD)

target_sources(omptarget.rtl.thunderbird PRIVATE
  src/rtl.cpp
  src/MemoryPool.cpp
  src/ArgumentConversion.cpp
)

# Offload platform integration (required - Phase 5 cleanup complete)
set(OFFLOAD_PLATFORM_PATH "" CACHE PATH "Path to offload-platform repository")
if(OFFLOAD_PLATFORM_PATH)
  message(STATUS "Using offload-platform from: ${OFFLOAD_PLATFORM_PATH}")
  target_include_directories(omptarget.rtl.thunderbird PRIVATE
    ${OFFLOAD_PLATFORM_PATH}/simplified-api/include
    ${OFFLOAD_PLATFORM_PATH}/include
  )
  # LIBTBIRD_HOST_SO can be set externally (e.g. by a Yocto recipe) to override
  # the default path so the caller can supply a pre-built .so from a staging
  # sysroot instead of a path relative to OFFLOAD_PLATFORM_PATH.
  if(NOT LIBTBIRD_HOST_SO)
    set(LIBTBIRD_HOST_SO "${OFFLOAD_PLATFORM_PATH}/simplified-api/host/libtbird_host.so")
  endif()
  if(EXISTS "${LIBTBIRD_HOST_SO}")
    target_link_libraries(omptarget.rtl.thunderbird PRIVATE
      "${LIBTBIRD_HOST_SO}"
    )
    message(STATUS "Linked against libtbird_host.so: ${LIBTBIRD_HOST_SO}")
  else()
    message(FATAL_ERROR "libtbird_host.so not found at ${LIBTBIRD_HOST_SO} - required for new API")
  endif()
else()
  message(FATAL_ERROR "OFFLOAD_PLATFORM_PATH must be set for offload-platform API integration")
endif()

# TODO: add logic to go find the ZephyrSDK

# Phase 5: Removed old message-based infrastructure dependencies
# - DataTransferEngine (replaced by tbird_offload_api.h)
# - ThunderbirdHostRuntime (replaced by libtbird_host.so)

target_include_directories(omptarget.rtl.thunderbird PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Configure testing for the THUNDERBIRD plugin. We will build tests if we could a
# functional THUNDERBIRD on the system, or if manually specifies by the user.
option(LIBOMPTARGET_FORCE_THUNDERBIRD_TESTS "Build THUNDERBIRD libomptarget tests" OFF)
if (LIBOMPTARGET_FOUND_THUNDERBIRD OR LIBOMPTARGET_FORCE_THUNDERBIRD_TESTS)
  message(STATUS "Enable tests using THUNDERBIRD plugin")
  set(LIBOMPTARGET_SYSTEM_TARGETS
      "${LIBOMPTARGET_SYSTEM_TARGETS} riscv64-inspire-elf" "riscv64-inspire-elf-LTO" PARENT_SCOPE)
  #-- LEAVING HERE FOR NOTES "${LIBOMPTARGET_SYSTEM_TARGETS} riscv64-unknown-linux-gnu" "riscv64-unknown-linux-gnu-LTO" PARENT_SCOPE)
  list(APPEND LIBOMPTARGET_TESTED_PLUGINS "omptarget.rtl.thunderbird")
  set(LIBOMPTARGET_TESTED_PLUGINS "${LIBOMPTARGET_TESTED_PLUGINS}" PARENT_SCOPE)
else()
  message(STATUS "Not generating THUNDERBIRD tests, no supported devices detected."
                   " Use 'LIBOMPTARGET_FORCE_THUNDERBIRD_TESTS' to override.")
endif()
