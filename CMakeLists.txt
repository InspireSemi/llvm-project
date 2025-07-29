cmake_minimum_required(VERSION 3.16)
project(host-runtime LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_C_COMPILER "clang" CACHE STRING "" FORCE)
#set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "" FORCE)

add_subdirectory(DataTransferEngine)
