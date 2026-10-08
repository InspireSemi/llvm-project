# Create the library and add the default arguments.
add_target_library(omptarget.rtl.thunderbird THUNDERBIRD)

target_sources(omptarget.rtl.thunderbird PRIVATE
  src/rtl.cpp
  src/MemoryPool.cpp
  src/LaunchSlots.cpp
  src/DeviceGlobals.cpp
)

# The plugin uses the offload-platform host library: its headers under
# OFFLOAD_PLATFORM_PATH, and the built library named by LIBTBIRD_HOST_SO.
set(OFFLOAD_PLATFORM_PATH "" CACHE PATH "Path to the offload-platform repository")
set(LIBTBIRD_HOST_SO "" CACHE FILEPATH "Path to the built libtbird_host.so")
if(NOT OFFLOAD_PLATFORM_PATH)
  message(FATAL_ERROR "OFFLOAD_PLATFORM_PATH must name the offload-platform repository")
endif()
if(NOT EXISTS "${LIBTBIRD_HOST_SO}")
  message(FATAL_ERROR "LIBTBIRD_HOST_SO must name the built libtbird_host.so "
                      "(got '${LIBTBIRD_HOST_SO}')")
endif()
message(STATUS "Using offload-platform from: ${OFFLOAD_PLATFORM_PATH}")
target_include_directories(omptarget.rtl.thunderbird PRIVATE
  ${OFFLOAD_PLATFORM_PATH}/simplified-api/include
  ${OFFLOAD_PLATFORM_PATH}/include
)
target_link_libraries(omptarget.rtl.thunderbird PRIVATE "${LIBTBIRD_HOST_SO}")
message(STATUS "Linked against libtbird_host.so: ${LIBTBIRD_HOST_SO}")

# Configure testing for the THUNDERBIRD plugin. We will build tests if we could a
# functional THUNDERBIRD on the system, or if manually specifies by the user.
option(LIBOMPTARGET_FORCE_THUNDERBIRD_TESTS "Build THUNDERBIRD libomptarget tests" OFF)
if (LIBOMPTARGET_FORCE_THUNDERBIRD_TESTS)
  message(STATUS "Enable tests using THUNDERBIRD plugin")
  set(LIBOMPTARGET_SYSTEM_TARGETS
      "${LIBOMPTARGET_SYSTEM_TARGETS} riscv64-inspire-linux-gnu riscv64-inspire-linux-gnu-LTO" PARENT_SCOPE)
  list(APPEND LIBOMPTARGET_TESTED_PLUGINS "omptarget.rtl.thunderbird")
  set(LIBOMPTARGET_TESTED_PLUGINS "${LIBOMPTARGET_TESTED_PLUGINS}" PARENT_SCOPE)
else()
  message(STATUS "Not generating THUNDERBIRD tests, no supported devices detected."
                   " Use 'LIBOMPTARGET_FORCE_THUNDERBIRD_TESTS' to override.")
endif()
