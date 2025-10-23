# SPDX-License-Identifier: Apache-2.0
message(STATUS "CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

#message(FATAL_ERROR "Error: Build terminated as requested")

# Build the llext module
set(ext_name saxpy_bin)
set(ext_src ${CMAKE_CURRENT_SOURCE_DIR}/src/${ext_name}_ext.c)
set(ext_bin ${PROJECT_BINARY_DIR}/llext/${ext_name}_ext.llext)
#set(ext_inc ${PROJECT_BINARY_DIR}/include/generated/${ext_name}_ext.inc)

add_llext_target(${ext_name}_ext
  OUTPUT  ${ext_bin}
  SOURCES ${ext_src}
)

# Add include directories to the llext target
target_include_directories(${ext_name}_ext_llext_lib PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/lib/common/include # Here's where I get the device runtime headers from for the saxpy example
  ${CMAKE_SOURCE_DIR}/lib/ivshmem/include
)

#   ${CMAKE_SOURCE_DIR}/include
#   ${CMAKE_SOURCE_DIR}/lib/memory/include
#   ${CMAKE_SOURCE_DIR}/lib/common/include
#   ${CMAKE_SOURCE_DIR}/lib/device/include
#   ${CMAKE_SOURCE_DIR}/lib/messaging/include
#)

#generate_inc_file_for_target(app ${ext_bin} ${ext_inc})

find_program(OBJCOPY_EXECUTABLE NAMES riscv64-inspire-elf-objcopy)

if(NOT OBJCOPY_EXECUTABLE)
  message(FATAL_ERROR "objcopy not found!")
endif()

# Example: Use objcopy to convert the object file to binary, but to be honest its better to pull in the elf so the metadata can be used to identify the entry point.
add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/llext/${ext_name}_ext.bin
  COMMAND ${OBJCOPY_EXECUTABLE} -O binary -j .text ${ext_bin} ${PROJECT_BINARY_DIR}/llext/${ext_name}_ext.bin
  DEPENDS ${ext_bin}
  COMMENT "Converting ${ext_bin} to binary (text section only)"
)
 
add_custom_target(${ext_name}_ext_bin ALL
  DEPENDS ${PROJECT_BINARY_DIR}/llext/${ext_name}_ext.bin
)

