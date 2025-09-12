# Create the library and add the default arguments.
add_target_library(omptarget.rtl.thunderbird THUNDERBIRD)

target_sources(omptarget.rtl.thunderbird PRIVATE src/rtl.cpp)

# TODO: add logic to go find the ZephyrSDK

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utils/host-runtime/DataTransferEngine")
  add_subdirectory(utils/host-runtime/DataTransferEngine)
  target_link_libraries(omptarget.rtl.thunderbird PRIVATE DataTransferEngine)
  target_include_directories(omptarget.rtl.thunderbird PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/host-runtime/DataTransferEngine)
else()
  message(FATAL_ERROR "Cannot find thunderbird/utils/host-runtime/DataTransferEngine libary")
endif()

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
