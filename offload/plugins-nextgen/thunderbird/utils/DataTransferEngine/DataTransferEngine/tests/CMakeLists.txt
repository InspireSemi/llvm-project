# Tests for DataTransferEngine

# Find required threading library
find_package(Threads REQUIRED)

# Test for ThunderbirdQEMU classes
add_executable(test_thunderbird_qemu
    test_thunderbird_qemu.cpp
)

target_link_libraries(test_thunderbird_qemu
    DataTransferEngine
    Threads::Threads
)

target_include_directories(test_thunderbird_qemu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Benchmark for ThunderbirdQEMU classes
add_executable(benchmark_thunderbird_qemu
    benchmark_thunderbird_qemu.cpp
)

target_link_libraries(benchmark_thunderbird_qemu
    DataTransferEngine
    Threads::Threads
)

target_include_directories(benchmark_thunderbird_qemu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Prepare isolated temp directories
set(THUNDERBIRD_QEMU_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/thunderbird_qemu_test)
set(THUNDERBIRD_QEMU_BENCH_DIR ${CMAKE_CURRENT_BINARY_DIR}/thunderbird_qemu_bench)
file(MAKE_DIRECTORY ${THUNDERBIRD_QEMU_TEST_DIR})
file(MAKE_DIRECTORY ${THUNDERBIRD_QEMU_BENCH_DIR})

# Register tests with CTest
add_test(NAME ThunderbirdQEMUTest 
    COMMAND test_thunderbird_qemu 
    WORKING_DIRECTORY ${THUNDERBIRD_QEMU_TEST_DIR})

add_test(NAME ThunderbirdQEMUBenchmark
    COMMAND benchmark_thunderbird_qemu
    WORKING_DIRECTORY ${THUNDERBIRD_QEMU_BENCH_DIR})

# Set test properties
set_tests_properties(ThunderbirdQEMUTest PROPERTIES
    TIMEOUT 30
    LABELS "unit;thunderbird"
)

set_tests_properties(ThunderbirdQEMUBenchmark PROPERTIES
    TIMEOUT 120
    LABELS "benchmark;performance;thunderbird"
)