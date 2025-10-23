/**
 * @file test_thunderbird_qemu.cpp
 * @brief Unit tests for ThunderbirdQEMU DataTransferEngine classes
 * @date 2025-07-27
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "test_utilities.hpp"
#include "DataTransferEngineFactory.hpp"
#include "DataTransferBackend.hpp"
#include <thread>
#include <future>
#include <atomic>

// Test constants
namespace {
    constexpr size_t SMALL_SHARED_MEMORY_SIZE = 4096;   // 4KB
    constexpr size_t LARGE_SHARED_MEMORY_SIZE = 8192;   // 8KB
    constexpr size_t MAX_BUFFER_SIZE = 1024;             // 1KB
    constexpr size_t MAX_FILE_OPERATION_SIZE = 2048;     // 2KB
    constexpr size_t MIN_FILE_OPERATION_SIZE = 256;      // 256B
    constexpr size_t NUM_RANDOM_TESTS = 10;
    constexpr size_t NUM_FILE_TESTS = 5;
    constexpr size_t LARGE_BUFFER_SIZE = 32 * 1024;     // 32KB
    constexpr size_t LARGE_FILE_SIZE = 64 * 1024;       // 64KB
}

// Global test utilities
TestUtils::FileManager g_file_manager;
TestUtils::DataGenerator g_data_gen;

// Test that the classes can be constructed successfully with shared memory files
bool test_shared_memory_construction() {
    std::string shared_mem_file = g_file_manager.createFile(8192);
    
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    TestUtils::Assertion::assertTrue(reader != nullptr && writer != nullptr, 
                                   "Factory should create valid instances");
    return true;
}

// Test API functionality including successful operations and error handling
bool test_basic_api_functionality() {
    std::string shared_mem_file = g_file_manager.createFile(4096);
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    std::vector<uint8_t> buffer = g_data_gen.patternBytes(1024, 0xCC);
    
    // Test successful buffer operations
    size_t written = writer->transfer(0x100, buffer.data(), buffer.size());
    TestUtils::Assertion::assertEqual(written, buffer.size(), "Writer should transfer all data");
    
    std::vector<uint8_t> read_buffer(1024, 0x00);
    size_t read = reader->transfer(0x100, read_buffer.data(), read_buffer.size());
    TestUtils::Assertion::assertEqual(read, read_buffer.size(), "Reader should transfer correct amount");
    TestUtils::Assertion::assertEqual(buffer, read_buffer, "Data integrity should be maintained");
    
    // Test successful file operations
    std::vector<uint8_t> test_data = g_data_gen.patternBytes(512, 0xBB);
    std::string source_file = g_file_manager.createFileWithData(test_data);
    std::string output_file = g_file_manager.createFile(0);
    
    size_t file_written = writer->transferFile(0x200, source_file, 512);
    size_t file_read = reader->transferFile(0x200, output_file, 512);
    TestUtils::Assertion::assertEqual(file_written, size_t(512), "File write should complete");
    TestUtils::Assertion::assertEqual(file_read, size_t(512), "File read should complete");
    
    return true;
}

bool test_null_pointer_errors() {
    std::string shared_mem_file = g_file_manager.createFile(4096);
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    // Test null pointer handling for buffer operations
    TestUtils::Assertion::expectException([&]() { reader->transfer(0x100, nullptr, 1024); }, 
                                        "Reader should throw error for null buffer");
    TestUtils::Assertion::expectException([&]() { writer->transfer(0x100, nullptr, 1024); }, 
                                        "Writer should throw error for null buffer");
    
    // Test invalid file paths
    TestUtils::Assertion::expectException([&]() { 
        reader->transferFile(0x100, "/nonexistent/path/file.bin", 1024); 
    }, "Reader should throw error for invalid file path");
    
    TestUtils::Assertion::expectException([&]() { 
        writer->transferFile(0x100, "/nonexistent/path/file.bin", 1024); 
    }, "Writer should throw error for invalid file path");
    
    return true;
}

// Test comprehensive buffer operations using real API
bool test_random_buffer_integrity() {
    std::string shared_mem_file = g_file_manager.createFile(LARGE_SHARED_MEMORY_SIZE);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    for (size_t test = 0; test < NUM_RANDOM_TESTS; ++test) {
        size_t buffer_size = g_data_gen.randomSize(1, MAX_BUFFER_SIZE);
        size_t offset = g_data_gen.randomOffset(LARGE_SHARED_MEMORY_SIZE - buffer_size);
        std::vector<uint8_t> write_data = g_data_gen.randomBytes(buffer_size);
        std::vector<uint8_t> read_buffer(buffer_size, 0x00);
        
        size_t written = writer->transfer(offset, write_data.data(), buffer_size);
        size_t read = reader->transfer(offset, read_buffer.data(), buffer_size);
        
        TestUtils::Assertion::assertEqual(written, buffer_size, "Transfer should complete successfully");
        TestUtils::Assertion::assertEqual(read, buffer_size, "Read should complete successfully");
        TestUtils::Assertion::assertEqual(write_data, read_buffer, "Data integrity should be maintained");
        
        // Test multiple reads from same location return identical data
        std::vector<uint8_t> second_read(buffer_size, 0x00);
        size_t second_read_size = reader->transfer(offset, second_read.data(), buffer_size);
        TestUtils::Assertion::assertEqual(second_read_size, buffer_size, "Second read should complete");
        TestUtils::Assertion::assertEqual(write_data, second_read, "Multiple reads should return identical data");
    }
    
    return true;
}

// Test comprehensive file operations using real API
bool test_file_transfer_integrity() {
    std::string shared_mem_file = g_file_manager.createFile(LARGE_SHARED_MEMORY_SIZE);
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    for (size_t test = 0; test < NUM_FILE_TESTS; ++test) {
        size_t data_size = g_data_gen.randomSize(MIN_FILE_OPERATION_SIZE, MAX_FILE_OPERATION_SIZE);
        size_t offset = g_data_gen.randomOffset(LARGE_SHARED_MEMORY_SIZE - data_size);
        std::vector<uint8_t> test_data = g_data_gen.randomBytes(data_size);
        
        // Create input file and write to shared memory
        std::string input_file = g_file_manager.createFileWithData(test_data, "input");
        size_t written = writer->transferFile(offset, input_file, data_size);
        
        // Read from shared memory to output file
        std::string output_file = g_file_manager.createFile(0, 0x00, "output");
        size_t read = reader->transferFile(offset, output_file, data_size);
        
        // Verify integrity
        std::vector<uint8_t> final_data = g_file_manager.readFile(output_file);
        TestUtils::Assertion::assertEqual(written, data_size, "File write should complete");
        TestUtils::Assertion::assertEqual(read, data_size, "File read should complete");
        TestUtils::Assertion::assertEqual(test_data, final_data, "File operations should preserve data integrity");
    }
    
    return true;
}

// Test boundary conditions and edge cases
bool test_boundary_conditions() {
    std::string shared_mem_file = g_file_manager.createFile(1024);
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    std::vector<uint8_t> buffer = g_data_gen.patternBytes(100, 0xAA);
    
    // Test reading at exact file boundary
    size_t written = writer->transfer(924, buffer.data(), 100);  // 924 + 100 = 1024
    TestUtils::Assertion::assertEqual(written, size_t(100), "Should write exactly to file boundary");
    
    std::vector<uint8_t> read_buffer(100, 0x00);
    size_t read = reader->transfer(924, read_buffer.data(), 100);
    TestUtils::Assertion::assertEqual(read, size_t(100), "Should read exactly to file boundary");
    TestUtils::Assertion::assertEqual(buffer, read_buffer, "Data should match at boundary");
    
    // Test reading beyond file boundary
    std::vector<uint8_t> large_buffer(200, 0x00);
    TestUtils::Assertion::expectException([&]() { 
        reader->transfer(1000, large_buffer.data(), 200); 
    }, "Should error when read would exceed file boundary");
    
    // Test writing beyond file boundary
    std::vector<uint8_t> write_buffer = g_data_gen.patternBytes(200, 0xBB);
    TestUtils::Assertion::expectException([&]() { 
        writer->transfer(1000, write_buffer.data(), 200); 
    }, "Should error when write would exceed file boundary");
    
    // Test invalid addresses
    TestUtils::Assertion::expectException([&]() { 
        reader->transfer(1024, buffer.data(), 1); 
    }, "Should throw error for address at file size");
    
    TestUtils::Assertion::expectException([&]() { 
        reader->transfer(2000, buffer.data(), 1); 
    }, "Should throw error for address beyond file size");
    
    // Test transferFile operations that would exceed boundaries
    std::vector<uint8_t> test_data = g_data_gen.patternBytes(200, 0xCC);
    std::string test_file = g_file_manager.createFileWithData(test_data, "boundary_input");
    std::string output_file = g_file_manager.createFile(0, 0x00, "boundary_output");
    
    TestUtils::Assertion::expectException([&]() { 
        writer->transferFile(900, test_file, 200); 
    }, "transferFile should error when write would exceed boundary");
    
    TestUtils::Assertion::expectException([&]() { 
        reader->transferFile(900, output_file, 200); 
    }, "transferFile should error when read would exceed boundary");
    
    return true;
}

// Test concurrent access scenarios with actual multi-threading
bool test_concurrent_read_write() {
    std::string shared_mem_file = g_file_manager.createFile(LARGE_SHARED_MEMORY_SIZE);
    
    // Helper function to reduce repetition
    auto run_concurrent_operations = [&](const std::string& test_name, 
                                        std::vector<std::function<bool()>> operations) {
        std::vector<std::future<bool>> futures;
        std::atomic<size_t> successful_ops{0};
        
        for (auto& op : operations) {
            futures.emplace_back(std::async(std::launch::async, [&, op]() -> bool {
                if (op()) {
                    successful_ops.fetch_add(1);
                    return true;
                }
                return false;
            }));
        }
        
        bool all_successful = true;
        for (auto& future : futures) {
            if (!future.get()) all_successful = false;
        }
        
        TestUtils::Assertion::assertTrue(all_successful, test_name + " should succeed");
        TestUtils::Assertion::assertEqual(successful_ops.load(), operations.size(), 
                                        test_name + " should complete all operations");
        return all_successful;
    };
    
    // Test 1: Concurrent writes with post-verification
    {
        std::vector<std::function<bool()>> write_ops;
        for (size_t i = 0; i < 3; ++i) {
            write_ops.push_back([&, i]() -> bool {
                try {
                    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
                    std::vector<uint8_t> data = g_data_gen.patternBytes(512, static_cast<uint8_t>(0xAA + i));
                    return writer->transfer(i * 1024, data.data(), data.size()) == 512;
                } catch (...) { return false; }
            });
        }
        
        run_concurrent_operations("Concurrent writes", write_ops);
        
        // Verify data integrity after concurrent writes
        auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
        for (size_t i = 0; i < 3; ++i) {
            std::vector<uint8_t> verify_data(512, 0x00);
            std::vector<uint8_t> expected_data = g_data_gen.patternBytes(512, static_cast<uint8_t>(0xAA + i));
            
            size_t read = reader->transfer(i * 1024, verify_data.data(), 512);
            TestUtils::Assertion::assertEqual(read, size_t(512), "Post-write verification should complete");
            TestUtils::Assertion::assertEqual(verify_data, expected_data, "Data integrity after concurrent writes");
        }
    }
    
    // Test 2: Concurrent reads from known data
    {
        std::vector<std::function<bool()>> read_ops;
        for (size_t i = 0; i < 3; ++i) {
            read_ops.push_back([&, i]() -> bool {
                try {
                    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
                    std::vector<uint8_t> read_data(512, 0x00);
                    std::vector<uint8_t> expected_data = g_data_gen.patternBytes(512, static_cast<uint8_t>(0xAA + i));
                    
                    size_t read = reader->transfer(i * 1024, read_data.data(), 512);
                    return (read == 512 && read_data == expected_data);
                } catch (...) { return false; }
            });
        }
        
        run_concurrent_operations("Concurrent reads with data integrity", read_ops);
    }
    
    // Test 3: Mixed read/write operations
    {
        // Setup known read region
        auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
        std::vector<uint8_t> read_region = g_data_gen.patternBytes(256, 0xBB);
        writer->transfer(4096, read_region.data(), 256);
        
        std::vector<std::function<bool()>> mixed_ops;
        for (size_t i = 0; i < 4; ++i) {
            mixed_ops.push_back([&, i]() -> bool {
                try {
                    if (i % 2 == 0) {
                        // Write operation with verification
                        auto w = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
                        auto r = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
                        std::vector<uint8_t> data = g_data_gen.patternBytes(128, static_cast<uint8_t>(0xDD + i));
                        std::vector<uint8_t> verify(128, 0x00);
                        
                        uint64_t offset = 5120 + (i * 256);
                        size_t written = w->transfer(offset, data.data(), 128);
                        size_t read_back = r->transfer(offset, verify.data(), 128);
                        
                        return (written == 128 && read_back == 128 && data == verify);
                    } else {
                        // Read operation with verification
                        auto r = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
                        std::vector<uint8_t> read_data(256, 0x00);
                        
                        size_t read = r->transfer(4096, read_data.data(), 256);
                        return (read == 256 && read_data == read_region);
                    }
                } catch (...) { return false; }
            });
        }
        
        run_concurrent_operations("Mixed concurrent operations with data integrity", mixed_ops);
    }
    
    return true;
}

// Test large buffer operations
bool test_large_buffer_operations() {
    std::string shared_mem_file = g_file_manager.createFile(LARGE_FILE_SIZE);
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    // Generate large test data
    std::vector<uint8_t> large_data = g_data_gen.randomBytes(LARGE_BUFFER_SIZE);
    std::vector<uint8_t> read_buffer(LARGE_BUFFER_SIZE, 0x00);
    
    // Test large write operation
    size_t written = writer->transfer(0x1000, large_data.data(), large_data.size());
    TestUtils::Assertion::assertEqual(written, LARGE_BUFFER_SIZE, "Large write should complete");
    
    // Test large read operation
    size_t read = reader->transfer(0x1000, read_buffer.data(), read_buffer.size());
    TestUtils::Assertion::assertEqual(read, LARGE_BUFFER_SIZE, "Large read should complete");
    TestUtils::Assertion::assertEqual(large_data, read_buffer, "Large data integrity should be maintained");
    
    return true;
}

// Test zero-length operations
bool test_zero_length_operations() {
    std::string shared_mem_file = g_file_manager.createFile(4096);
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    std::vector<uint8_t> buffer = g_data_gen.patternBytes(100, 0xAA);
    
    // Test zero-length operations
    size_t written = writer->transfer(0x100, buffer.data(), 0);
    size_t read = reader->transfer(0x100, buffer.data(), 0);
    
    TestUtils::Assertion::assertEqual(written, size_t(0), "Zero-length write should return 0");
    TestUtils::Assertion::assertEqual(read, size_t(0), "Zero-length read should return 0");
    
    // Test zero-length file operations
    std::string test_file = g_file_manager.createFile(0);
    std::string output_file = g_file_manager.createFile(0);
    
    size_t file_written = writer->transferFile(0x100, test_file, 0);
    size_t file_read = reader->transferFile(0x100, output_file, 0);
    
    TestUtils::Assertion::assertEqual(file_written, size_t(0), "Zero-length file write should return 0");
    TestUtils::Assertion::assertEqual(file_read, size_t(0), "Zero-length file read should return 0");
    
    return true;
}

int main() {
    std::cout << "ThunderbirdQEMU API Tests" << std::endl;
    std::cout << "========================" << std::endl;
    
    struct TestCase {
        std::string name;
        std::function<bool()> func;
    };
    
    std::vector<TestCase> tests = {
        {"Shared Memory Construction", test_shared_memory_construction},
        {"Basic API Functionality", test_basic_api_functionality},
        {"Null Pointer Error Handling", test_null_pointer_errors},
        {"Random Buffer Integrity", test_random_buffer_integrity},
        {"File Transfer Integrity", test_file_transfer_integrity},
        {"Boundary Conditions", test_boundary_conditions},
        {"Concurrent Read/Write", test_concurrent_read_write},
        {"Large Buffer Operations", test_large_buffer_operations},
        {"Zero-Length Operations", test_zero_length_operations}
    };
    
    size_t passed = 0;
    for (const auto& test : tests) {
        std::cout << "  " << test.name << "... ";
        try {
            if (test.func()) {
                std::cout << "PASS" << std::endl;
                passed++;
            } else {
                std::cout << "FAIL" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "FAIL" << std::endl;
            std::cout << "    Error: " << e.what() << std::endl;
        }
    }
    
    std::cout << "\nResults: " << passed << "/" << tests.size() << " tests passed";
    if (passed == tests.size()) {
        std::cout << " ✓" << std::endl;
    } else {
        std::cout << " ✗" << std::endl;
    }
    
    std::cout << "Cleaning up " << g_file_manager.getManagedFileCount() << " test files..." << std::endl;
    g_file_manager.cleanup();
    
    return (passed == tests.size()) ? 0 : 1;
}
