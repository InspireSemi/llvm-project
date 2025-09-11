cmake_minimum_required(VERSION 3.16)
project(host-runtime LANGUAGES C CXX)

# Enable testing support
include(CTest)
# This automatically creates BUILD_TESTING option and sets it to ON by default

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_C_COMPILER "clang" CACHE STRING "" FORCE)
#set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "" FORCE)

add_subdirectory(DataTransferEngine)
add_subdirectory(ThunderbirdHostRuntime)
