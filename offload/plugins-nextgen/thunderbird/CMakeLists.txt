# Create the library and add the default arguments.
add_target_library(omptarget.rtl.thunderbird THUNDERBIRD)

target_sources(omptarget.rtl.thunderbird PRIVATE src/rtl.cpp)

# Offload platform integration (optional)
set(OFFLOAD_PLATFORM_PATH "" CACHE PATH "Path to offload-platform repository")
if(OFFLOAD_PLATFORM_PATH)
  message(STATUS "Using offload-platform from: ${OFFLOAD_PLATFORM_PATH}")
  target_include_directories(omptarget.rtl.thunderbird PRIVATE
    ${OFFLOAD_PLATFORM_PATH}/simplified-api/include
    ${OFFLOAD_PLATFORM_PATH}/include
  )
  # Link against libtbird_host.so
  if(EXISTS "${OFFLOAD_PLATFORM_PATH}/simplified-api/host/libtbird_host.so")
    target_link_libraries(omptarget.rtl.thunderbird PRIVATE
      ${OFFLOAD_PLATFORM_PATH}/simplified-api/host/libtbird_host.so
    )
    message(STATUS "Linked against libtbird_host.so")
  else()
    message(WARNING "libtbird_host.so not found at ${OFFLOAD_PLATFORM_PATH}/simplified-api/host/")
  endif()
else()
  message(STATUS "OFFLOAD_PLATFORM_PATH not set - building without offload-platform integration")
endif()

# TODO: add logic to go find the ZephyrSDK

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utils/host-runtime/DataTransferEngine")
  add_subdirectory(utils/host-runtime/DataTransferEngine)
  target_link_libraries(omptarget.rtl.thunderbird PRIVATE DataTransferEngine)
  target_include_directories(omptarget.rtl.thunderbird PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/host-runtime/DataTransferEngine)
else()
  message(FATAL_ERROR "Cannot find thunderbird/utils/host-runtime/DataTransferEngine libary")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utils/host-runtime/ThunderbirdHostRuntime")
  add_subdirectory(utils/host-runtime/ThunderbirdHostRuntime)
  target_link_libraries(omptarget.rtl.thunderbird PRIVATE ThunderbirdHostRuntime)
else()
  message(FATAL_ERROR "Cannot find thunderbird/utils/host-runtime/ThunderbirdHostRuntime libary")
endif()


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
