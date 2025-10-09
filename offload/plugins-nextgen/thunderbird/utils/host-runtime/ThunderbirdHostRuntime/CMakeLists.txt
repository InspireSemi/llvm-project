cmake_minimum_required(VERSION 3.16)
project(ThunderbirdHostRuntime)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all .cpp files in src/
file(GLOB THR_SOURCES "src/*.cpp")

# Create the shared library
add_library(ThunderbirdHostRuntime SHARED
    ${THR_SOURCES}
)

target_include_directories(ThunderbirdHostRuntime
    PUBLIC
        include
        ../ThunderbirdDeviceRuntime/lib/ivshmem/include
        ../ThunderbirdDeviceRuntime/lib/common/include
        ../ThunderbirdDeviceRuntime/lib/messaging/include
        ../ThunderbirdDeviceRuntime/lib/device/include
)

# Link with DataTransferEngine - this brings in DTE's PUBLIC includes automatically
target_link_libraries(ThunderbirdHostRuntime
    PUBLIC
        DataTransferEngine
        elf  # Add libelf linkage here with PUBLIC to propagate to dependents
)

# Set library properties
set_target_properties(ThunderbirdHostRuntime PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

install(TARGETS ThunderbirdHostRuntime DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
