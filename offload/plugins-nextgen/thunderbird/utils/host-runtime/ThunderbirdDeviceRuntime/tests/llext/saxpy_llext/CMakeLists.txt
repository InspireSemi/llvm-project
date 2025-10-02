# SPDX-License-Identifier: Apache-2.0

# Build the llext module
set(ext_name saxpy_llext)
set(ext_src ${CMAKE_CURRENT_SOURCE_DIR}/src/${ext_name}_ext.c)
set(ext_bin ${PROJECT_BINARY_DIR}/llext/${ext_name}_ext.llext)
set(ext_inc ${PROJECT_BINARY_DIR}/include/generated/${ext_name}_ext.inc)

add_llext_target(${ext_name}_ext
  OUTPUT  ${ext_bin}
  SOURCES ${ext_src}
)

# Add include directories to the llext target
target_include_directories(${ext_name}_ext_llext_lib PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/lib/memory/include
  ${CMAKE_SOURCE_DIR}/lib/common/include
  ${CMAKE_SOURCE_DIR}/lib/device/include
  ${CMAKE_SOURCE_DIR}/lib/ivshmem/include
  ${CMAKE_SOURCE_DIR}/lib/messaging/include
)

generate_inc_file_for_target(app ${ext_bin} ${ext_inc})

