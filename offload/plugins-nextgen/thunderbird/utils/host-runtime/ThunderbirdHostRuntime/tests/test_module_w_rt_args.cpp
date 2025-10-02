/**
 * @file test_module_w_rt_args.cpp
 * @author Michael Brothers
 * @brief Tests executing a kernel from llext buffer
 * @details Tests allocating memory for llext buffer, transferring the llext data, and executing the kernel on the device.
 *          Assumes the device runtime supports loading llext and provides a simple but non-trivial kernel, saxpy.
 * @version 0.3
 * @date 2025-09-29
 *
 * @copyright Copyright (c) 2025 InspireSemi
 */

 // From host runtime headers
#include "ThunderbirdRuntime.hpp"
#include "DataTransferEngineFactory.hpp"
#include "DataTransferBackend.hpp"
#include "MailboxUtils.hpp"
#include "BatchUtils.hpp"
#include "BatchUtils.tpp"
#include "MessageUtils.hpp"
#include "DataTransferUtils.hpp"
#include "BufferUtils.hpp"
#include "TestUtils.hpp"

// From device runtime headers
#include "ivshmem_config.h"
#include "ivshmem_abi.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <chrono>
#include <random>
#include <filesystem>  // For std::filesystem
#include <unordered_map>

// Group all constants together at the top
constexpr size_t PAGE_SIZE = 4096;

// SAXPY problem configuration
constexpr size_t N = 1024;
constexpr size_t VECTOR_BUFFER_SIZE = N * sizeof(float);

// Kernel execution configuration  
constexpr size_t NUM_BLOCKS_X = 1;
constexpr size_t NUM_BLOCKS_Y = 1;
constexpr size_t NUM_BLOCKS_Z = 1;
constexpr size_t NUM_THREADS_PER_BLOCKDIM = 2;
constexpr size_t TOTAL_THREADS = NUM_BLOCKS_X * NUM_BLOCKS_Y * NUM_BLOCKS_Z * 
                                 NUM_THREADS_PER_BLOCKDIM * NUM_THREADS_PER_BLOCKDIM * NUM_THREADS_PER_BLOCKDIM;

// Polling configuration
constexpr int DEFAULT_MAX_ITERATIONS = 100;
constexpr int DEFAULT_POLL_MS = 100;
constexpr float FLOAT_TOLERANCE = 1e-6f;
constexpr float RESULT_TOLERANCE = 1e-5f;

static const std::vector<uint8_t> saxpy_llext_buffer = {
    #include "saxpy_llext_ext.inc"
};

// Packed structure for SAXPY kernel arguments
typedef struct __attribute__((packed)) {
    const float * const x;
    const float * const y;
    float * const out;
    float a;
    int n;
    int num_threads;
} args_t;

// Global test state
std::unique_ptr<ThunderbirdHostRuntime> g_runtime;

// Forward declarations for test-specific helper functions
bool read_and_verify_results(ThunderbirdHostRuntime &runtime, 
                            uint64_t out_address,
                            const std::vector<float>& x_data,
                            const std::vector<float>& y_data,
                            float scalar_a);

bool test_thunderbird_runtime_construction() {
    // Use the shared memory file initialized by external entity
    std::string shared_mem_file = "/dev/shm/ivshmem";
    
    // Check file permissions
    if (!TestUtils::check_file_permissions(shared_mem_file)) {
        return false;
    }
    
    // Check QEMU process
    if (!TestUtils::check_qemu_process()) {
        return false;
    }
    
    // Create reader and writer using QEMU backend
    auto reader = DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    auto writer = DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, shared_mem_file);
    
    if (!reader || !writer) {
        std::cerr << "Failed to create DataTransferEngine instances" << std::endl;
        return false;
    }
    
    // Instantiate ThunderbirdHostRuntime
    try {
        g_runtime = std::make_unique<ThunderbirdHostRuntime>(std::move(reader), std::move(writer));
        std::cout << "ThunderbirdHostRuntime constructed successfully" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to construct ThunderbirdHostRuntime: " << e.what() << std::endl;
        return false;
    }
}

// Placeholder for future tests
bool test_runtime_functionality() {
    if (!g_runtime) {
        std::cerr << "Error: Runtime not initialized" << std::endl;
        return false;
    }

    std::cout << "Using llext buffer for kernel" << std::endl;

    std::cout << "Llext buffer size: " << saxpy_llext_buffer.size() << " bytes" << std::endl;

    try {
        // Phase 0: ensure mailboxes are clear
        MailboxUtils::clearAllD2HSlots(*g_runtime->getWriter());

        std::vector<std::pair<size_t, std::string>> buffer_specs = {
            {saxpy_llext_buffer.size(), "kernel"},  // Use llext buffer size
            {sizeof(args_t), "args"},
            {VECTOR_BUFFER_SIZE, "x"},       // SAXPY input vector x (N elements)
            {VECTOR_BUFFER_SIZE, "y"},       // SAXPY input vector y (N elements)
            {VECTOR_BUFFER_SIZE, "out"}      // SAXPY output vector (N elements)
        };
        
        std::unordered_map<std::string, BufferUtils::AllocatedBuffer> allocated_buffers;
        if (!BufferUtils::allocate_buffers(g_runtime->getWriter(), g_runtime->getReader(), buffer_specs, allocated_buffers)) {
            std::cerr << "Error: Failed to allocate required buffers" << std::endl;
            return false;
        }

        std::cout << "✓ Kernel buffer allocated at: 0x" << std::hex << allocated_buffers["kernel"].address << std::dec << " (" << allocated_buffers["kernel"].size << " bytes)" << std::endl;
        std::cout << "✓ Args buffer allocated at: 0x" << std::hex << allocated_buffers["args"].address << std::dec << " (" << allocated_buffers["args"].size << " bytes)" << std::endl;
        std::cout << "✓ SAXPY X vector buffer allocated at: 0x" << std::hex << allocated_buffers["x"].address << std::dec << " (" << allocated_buffers["x"].size << " bytes, " << N << " floats)" << std::endl;
        std::cout << "✓ SAXPY Y vector buffer allocated at: 0x" << std::hex << allocated_buffers["y"].address << std::dec << " (" << allocated_buffers["y"].size << " bytes, " << N << " floats)" << std::endl;
        std::cout << "✓ SAXPY Output buffer allocated at: 0x" << std::hex << allocated_buffers["out"].address << std::dec << " (" << allocated_buffers["out"].size << " bytes, " << N << " floats)" << std::endl;

        // Generate random scalar for SAXPY: out = a*x + y
        float scalar_a = DataTransferUtils::generate_random_vector(1, -5.0f, 5.0f)[0];
        std::cout << "✓ Generated random scalar a = " << scalar_a << " for SAXPY computation" << std::endl;

        args_t kernel_args = {
            .x = reinterpret_cast<const float*>(allocated_buffers["x"].address),
            .y = reinterpret_cast<const float*>(allocated_buffers["y"].address),
            .out = reinterpret_cast<float*>(allocated_buffers["out"].address),
            .a = scalar_a,
            .n = static_cast<int>(N),
            .num_threads = static_cast<int>(TOTAL_THREADS)
        };

        std::cout << "✓ SAXPY kernel arguments prepared:" << std::endl;
        std::cout << "  - x vector address: 0x" << std::hex << reinterpret_cast<uint64_t>(kernel_args.x) << std::dec << std::endl;
        std::cout << "  - y vector address: 0x" << std::hex << reinterpret_cast<uint64_t>(kernel_args.y) << std::dec << std::endl;
        std::cout << "  - output address: 0x" << std::hex << reinterpret_cast<uint64_t>(kernel_args.out) << std::dec << std::endl;
        std::cout << "  - scalar a: " << kernel_args.a << std::endl;
        std::cout << "  - vector size n: " << kernel_args.n << std::endl;
        std::cout << "  - num_threads: " << kernel_args.num_threads << std::endl;
        
        // Phase 4: generate and transfer vector data
        std::cout << "\nGenerating random vector data for SAXPY problem (N=" << N << ")..." << std::endl;
        std::vector<float> x_data = DataTransferUtils::generate_random_vector(N, -10.0f, 10.0f);
        std::vector<float> y_data = DataTransferUtils::generate_random_vector(N, -5.0f, 5.0f);
        std::vector<float> out_data(N, 1.0f); // Initialize output vector with 1
        

        if (!DataTransferUtils::write_data_to_device(*g_runtime->getWriter(), allocated_buffers["kernel"].address, saxpy_llext_buffer.data(), saxpy_llext_buffer.size(), "llext buffer")) {
            return false;
        }

        if (!DataTransferUtils::write_data_to_device(*g_runtime->getWriter(), allocated_buffers["args"].address, &kernel_args, sizeof(args_t), "kernel arguments")) {
            return false;
        }

        if (!DataTransferUtils::write_vector_to_device(*g_runtime->getWriter(), allocated_buffers["x"].address, x_data, "X")) {
            return false;
        }
        
        if (!DataTransferUtils::write_vector_to_device(*g_runtime->getWriter(), allocated_buffers["y"].address, y_data, "Y")) {
            return false;
        }

        if (!DataTransferUtils::write_vector_to_device(*g_runtime->getWriter(), allocated_buffers["out"].address, out_data, "Output")) {
            return false;
        }

        // Verify that x and y vectors were transmitted correctly by reading them back
        std::cout << "\nVerifying vector data transmission..." << std::endl;
        
        if (!DataTransferUtils::verify_vector_transmission(*g_runtime->getReader(), allocated_buffers["x"].address, x_data, "X") ||
            !DataTransferUtils::verify_vector_transmission(*g_runtime->getReader(), allocated_buffers["y"].address, y_data, "Y") || 
            !DataTransferUtils::verify_vector_transmission(*g_runtime->getReader(), allocated_buffers["out"].address, out_data, "Output")) {
            return false;
        }
        
        // Phase 5: execute kernel (launch only, don't free memory yet)
        uint64_t entry_address = allocated_buffers["kernel"].address;
        std::cout << "\nLaunching kernel..." << std::endl;
        std::cout << "Kernel entry address: 0x" << std::hex << entry_address << std::dec 
                  << " (base of llext buffer)" << std::endl;
        
        // Create and send launch batch
        std::vector<std::pair<int, message_slot_t>> launch_batch;
        if (!TestUtils::create_and_send_launch_batch(*g_runtime, entry_address, allocated_buffers["args"].address,
                                                     NUM_BLOCKS_X, NUM_BLOCKS_Y, NUM_BLOCKS_Z,
                                                     NUM_THREADS_PER_BLOCKDIM, NUM_THREADS_PER_BLOCKDIM, NUM_THREADS_PER_BLOCKDIM,
                                                     launch_batch)) {
            std::cerr << "Error: Failed to create/send launch batch" << std::endl;
            return false;
        }
        
        // Wait for launch response
        std::vector<std::pair<int, message_slot_t>> launch_response_batch;
        std::vector<std::pair<int, message_slot_t>> launch_body_slots;
        auto reader = g_runtime->getReader();
        if (!BatchUtils::waitForResponseBatch(*reader, launch_batch, launch_response_batch, launch_body_slots, 
                                    100, 100, "launch response")) {
            std::cerr << "Error: Launch batch failed or timed out" << std::endl;
            return false;
        }
        
        // Extract launch response
        int launch_slot;
        launch_rsp_t launch_rsp;
        if (!BatchUtils::extractValidResponseFromBatch(launch_body_slots, MSG_RSP_LAUNCH, launch_slot, launch_rsp)) {
            std::cerr << "Error: Failed to extract valid launch response" << std::endl;
            return false;
        }
        
        std::cout << "✓ Kernel launched successfully (slot " << launch_slot << ")" << std::endl;
        MailboxUtils::clearResponseSlots(*g_runtime->getWriter(), launch_response_batch);
        
        // Phase 6: Read and verify results while memory is still allocated
        std::cout << "\nReading and verifying computation results..." << std::endl;
        if (!read_and_verify_results(*g_runtime, allocated_buffers["out"].address, x_data, y_data, scalar_a)) {
            std::cerr << "Error: Result verification failed" << std::endl;
            return false;
        }
        
        // Phase 7: Now free all allocated memory
        std::cout << "\nFreeing allocated memory..." << std::endl;
        if (!BufferUtils::free_buffers(g_runtime->getWriter(), g_runtime->getReader(), allocated_buffers)) {
            std::cerr << "Error: Failed to free allocated buffers" << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception during runtime functionality test: " << e.what() << std::endl;
        return false;
    }

    std::cout << "✓ Runtime functionality test completed successfully" << std::endl;
    return true;
}

int main(int argc, char **argv) {
    std::cout << "Starting test_llext..." << std::endl;

    // Construct runtime
    std::cout << "Attempting to construct ThunderbirdHostRuntime..." << std::endl;
    if (!test_thunderbird_runtime_construction()) {
        std::cerr << "Error: Failed to construct ThunderbirdHostRuntime" << std::endl;
        return 2;
    }
    std::cout << "✓ ThunderbirdHostRuntime construction successful" << std::endl;

    // Run the functionality test
    std::cout << "Starting runtime functionality test..." << std::endl;
    if (!test_runtime_functionality()) {
        std::cerr << "Error: Runtime functionality test failed" << std::endl;
        return 3;
    }
    std::cout << "✓ Runtime functionality test completed" << std::endl;

    std::cout << "All tests passed" << std::endl;
    return 0;
}

// Implementations moved here to keep main near the top of the file

// Helper: read results from device memory and verify SAXPY computation
bool read_and_verify_results(ThunderbirdHostRuntime &runtime, 
                                    uint64_t out_address,
                                    const std::vector<float>& x_data,
                                    const std::vector<float>& y_data,
                                    float scalar_a) {
    auto reader = runtime.getReader();
    std::cout << "Reading computation results from device memory..." << std::endl;
    
    // Read output vector from device
    std::vector<float> device_results(N);
    size_t byte_size = device_results.size() * sizeof(float);
    
    int64_t bytes_read = reader->transfer(out_address - IVSHMEM_SHM_BASE,
                                         reinterpret_cast<uint8_t*>(device_results.data()),
                                         byte_size);
    
    if (bytes_read != static_cast<int64_t>(byte_size)) {
        std::cerr << "Error: Failed to read results from device memory (read=" 
                  << bytes_read << ", expected=" << byte_size << ")" << std::endl;
        return false;
    }
    
    std::cout << "✓ Successfully read " << device_results.size() << " floats from device" << std::endl;
    
    // Compute expected results on host for verification
    std::vector<float> expected_results(N);
    for (size_t i = 0; i < N; ++i) {
        expected_results[i] = scalar_a * x_data[i] + y_data[i];
    }
    
    // Verify results with tolerance for floating point precision
    const float tolerance = 1e-5f;
    bool results_match = true;
    size_t mismatch_count = 0;
    
    std::cout << "Verifying SAXPY results (out[i] = " << scalar_a << " * x[i] + y[i]):" << std::endl;
    
    for (size_t i = 0; i < N; ++i) {
        float diff = std::abs(device_results[i] - expected_results[i]);
        if (diff > tolerance) {
            if (mismatch_count < 10) {  // Only print first 5 mismatches
                std::cerr << "  Mismatch at index " << i << ": device=" << device_results[i] 
                          << ", expected=" << expected_results[i] << ", diff=" << diff << std::endl;
            }
            results_match = false;
            mismatch_count++;
        }
    }
    
    if (results_match) {
        std::cout << "✓ All " << N << " results match expected values within tolerance" << std::endl;
        
        // Print sample results
        std::cout << "Sample results:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), N); ++i) {
            std::cout << "  out[" << i << "] = " << device_results[i] 
                      << " (expected: " << expected_results[i] << ")" << std::endl;
        }
    } else {
        std::cerr << "✗ " << mismatch_count << " out of " << N 
                  << " results don't match expected values" << std::endl;
        return false;
    }
    
    return true;
}
