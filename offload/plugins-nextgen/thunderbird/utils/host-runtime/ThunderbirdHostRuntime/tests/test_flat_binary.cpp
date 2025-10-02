/**
 * @file test_flat_binary.cpp
 * @author Michael Brothers
 * @brief Tests executing a kernel from an elf file by converting it to a flat binary
 * @details Tests parsing an elf file, creating a flat binary, allocating memory, transferring the binary, and executing the kernel on the device.
 *          Assumes the device runtime supports loading flat binaries and provides a simple but non-trivial kernel, saxpy.
 * @version 0.2
 * @date 2025-09-17
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

 // From host runtime headers
#include "ThunderbirdRuntime.hpp"
#include "DataTransferEngineFactory.hpp"
#include "DataTransferBackend.hpp"
#include "MailboxUtils.hpp"
#include "BatchUtils.hpp"
#include "MessageUtils.hpp"
#include "ElfParser.hpp"

// From device runtime headers
#include "ivshmem_config.h"

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

// Group all constants together at the top
constexpr uint64_t IVSHMEM_BASE_ADDRESS = 0x82000000000000ull;
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

static std::vector<uint8_t> flatbin_vec;

static const std::string elf_path = "/home/mbrothers/saxpy_bin_ext.llext";

// Enhanced buffer tracking structure
struct AllocatedBuffer {
    uint64_t address;
    size_t size;
    std::string purpose;  // "binary", "args", etc.
    
    AllocatedBuffer(uint64_t addr, size_t sz, const std::string& p) 
        : address(addr), size(sz), purpose(p) {}
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

static std::string resolve_elf_path_from_args(int argc, char **argv);

bool check_file_permissions(const std::string& filepath);

bool check_qemu_process();

// Helper: load flat binary file and pad to page boundary
static bool load_flat_binary_file(const std::string& binary_path, std::vector<uint8_t>& flat_binary);

// Helper forward declarations
static void clear_all_mailbox_slots(ThunderbirdHostRuntime &runtime);

// Helper: create malloc batch for multiple allocations and send it
static bool create_and_send_malloc_batch(ThunderbirdHostRuntime &runtime, 
                                         const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                                         std::vector<std::pair<int, message_slot_t>> &out_batch);

// Helper: wait for a response batch for the given sent batch and validate integrity
static bool wait_for_response_batch(ThunderbirdHostRuntime &runtime,
                                    const std::vector<std::pair<int, message_slot_t>> &sent_batch,
                                    std::vector<std::pair<int, message_slot_t>> &out_response_batch,
                                    std::vector<std::pair<int, message_slot_t>> &out_body_slots,
                                    int max_iterations = 50,
                                    int poll_ms = 100,
                                    const char *label = "response");

// Helper: extract malloc addresses from validated body slots and match them to purposes
static bool extract_malloc_addresses_from_body_slots(const std::vector<std::pair<int, message_slot_t>> &body_slots,
                                                     const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                                                     std::vector<AllocatedBuffer>& out_allocated_buffers);

// Helper: allocate multiple buffers in sequence
static bool allocate_buffers(ThunderbirdHostRuntime &runtime, 
                            const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                            std::vector<AllocatedBuffer>& out_allocated_buffers);

// Helper: find allocated buffer by purpose
static AllocatedBuffer* find_buffer_by_purpose(std::vector<AllocatedBuffer>& buffers, const std::string& purpose);

// Helper: generate random float vector
static std::vector<float> generate_random_vector(size_t size, float min_val = -10.0f, float max_val = 10.0f);

// Helper: write float vector to device memory
static bool write_vector_to_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, const std::vector<float>& data, const std::string& name);

// Helper: read float vector from device memory
static bool read_vector_from_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, std::vector<float>& data, const std::string& name);

// Helper: verify vector transmission by reading back and comparing
static bool verify_vector_transmission(ThunderbirdHostRuntime &runtime, 
                                       uint64_t allocated_address, 
                                       const std::vector<float>& original_data, 
                                       const std::string& name);

// Helper: write the flat binary to device memory
static bool write_flatbin_to_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address);

// Helper: write arguments to device memory
static bool write_args_to_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, const args_t* args_data);

// Helper: create and send kernel launch batch (launch only, no memory free)
static bool create_and_send_launch_batch(ThunderbirdHostRuntime &runtime, 
                                         uint64_t entry_address, 
                                         uint64_t args_address, 
                                         std::vector<std::pair<int, message_slot_t>> &out_launch_batch);

// Helper: create and send memory free batch for all allocated buffers
static bool create_and_send_free_batch(ThunderbirdHostRuntime &runtime,
                                       const std::vector<AllocatedBuffer>& allocated_buffers,
                                       std::vector<std::pair<int, message_slot_t>> &out_free_batch);

// Helper: read results from device memory and verify SAXPY computation
static bool read_and_verify_results(ThunderbirdHostRuntime &runtime, 
                                    uint64_t out_address,
                                    const std::vector<float>& x_data,
                                    const std::vector<float>& y_data,
                                    float scalar_a);

// Clear the slots listed in response_batch
static void clear_response_slots(ThunderbirdHostRuntime &runtime, const std::vector<std::pair<int, message_slot_t>> &response_batch);

bool test_thunderbird_runtime_construction() {
    // Use the shared memory file initialized by external entity
    std::string shared_mem_file = "/dev/shm/ivshmem";
    
    // Check file permissions
    if (!check_file_permissions(shared_mem_file)) {
        return false;
    }
    
    // Check QEMU process
    if (!check_qemu_process()) {
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

    // Resolve ELF path
    std::string elf_path = resolve_elf_path_from_args(0, nullptr);
    std::cout << "Using ELF file: " << elf_path << std::endl;

    // Load flat binary from file (assume binary file has same name as ELF but with .bin extension)
    std::string binary_path = elf_path;
    if (binary_path.size() > 6 && binary_path.substr(binary_path.size() - 6) == ".llext") {
        binary_path = binary_path.substr(0, binary_path.size() - 6) + ".bin";
    } else {
        binary_path = binary_path + ".bin";
    }
    
    if (!load_flat_binary_file(binary_path, flatbin_vec)) {
        std::cerr << "Error: Failed to load flat binary file" << std::endl;
        return false;
    }
    
    // Use ElfParser::determineBinaryEntryOffset to find entry point in binary
    ElfInfo info;
    std::cout << "Calling ElfParser::determineBinaryEntryOffset..." << std::endl;
    int parse_result = ElfParser::determineBinaryEntryOffset(elf_path, flatbin_vec, "execute", info);
    if (parse_result != 0) {
        std::cerr << "Error: Failed to determine binary entry offset (return code: " << parse_result << ")" << std::endl;
        return false;
    }
    std::cout << "✓ Entry offset determination successful" << std::endl;

    std::cout << "Flat binary size: " << flatbin_vec.size() << " bytes" << std::endl;
    std::cout << "Entry offset: 0x" << std::hex << info.entry_binary_offset << std::dec << std::endl;

    try {
        // Phase 0: ensure mailboxes are clear
        clear_all_mailbox_slots(*g_runtime);

        // Phase 1: allocate multiple buffers for SAXPY computation (N=256 elements)
        std::vector<std::pair<size_t, std::string>> buffer_specs = {
            {flatbin_vec.size(), "binary"},  // Use actual loaded binary size
            {sizeof(args_t), "args"},
            {VECTOR_BUFFER_SIZE, "x"},       // SAXPY input vector x (N elements)
            {VECTOR_BUFFER_SIZE, "y"},       // SAXPY input vector y (N elements)
            {VECTOR_BUFFER_SIZE, "out"}      // SAXPY output vector (N elements)
        };
        
        std::vector<AllocatedBuffer> allocated_buffers;
        if (!allocate_buffers(*g_runtime, buffer_specs, allocated_buffers)) {
            std::cerr << "Error: Failed to allocate required buffers" << std::endl;
            return false;
        }

        // Phase 2: Find specific buffers and transfer binary to binary buffer
        AllocatedBuffer* binary_buffer = find_buffer_by_purpose(allocated_buffers, "binary");
        AllocatedBuffer* args_buffer = find_buffer_by_purpose(allocated_buffers, "args");
        AllocatedBuffer* x_buffer = find_buffer_by_purpose(allocated_buffers, "x");
        AllocatedBuffer* y_buffer = find_buffer_by_purpose(allocated_buffers, "y");
        AllocatedBuffer* out_buffer = find_buffer_by_purpose(allocated_buffers, "out");
        
        if (!binary_buffer || !args_buffer || !x_buffer || !y_buffer || !out_buffer) {
            std::cerr << "Error: Failed to find required buffers" << std::endl;
            return false;
        }

        std::cout << "✓ Binary buffer allocated at: 0x" << std::hex << binary_buffer->address << std::dec << " (" << binary_buffer->size << " bytes)" << std::endl;
        std::cout << "✓ Args buffer allocated at: 0x" << std::hex << args_buffer->address << std::dec << " (" << args_buffer->size << " bytes)" << std::endl;
        std::cout << "✓ SAXPY X vector buffer allocated at: 0x" << std::hex << x_buffer->address << std::dec << " (" << x_buffer->size << " bytes, " << N << " floats)" << std::endl;
        std::cout << "✓ SAXPY Y vector buffer allocated at: 0x" << std::hex << y_buffer->address << std::dec << " (" << y_buffer->size << " bytes, " << N << " floats)" << std::endl;
        std::cout << "✓ SAXPY Output buffer allocated at: 0x" << std::hex << out_buffer->address << std::dec << " (" << out_buffer->size << " bytes, " << N << " floats)" << std::endl;

        // Transfer binary to binary buffer
        if (!write_flatbin_to_device(*g_runtime, binary_buffer->address)) {
            return false;
        }

        // Phase 3: prepare and transfer arguments to args buffer
        std::cout << "\nPreparing SAXPY kernel arguments..." << std::endl;
        
        // Generate random scalar value for SAXPY operation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> scalar_dist(-5.0f, 5.0f);
        float scalar_a = scalar_dist(gen);
        
        args_t kernel_args = {
            .x = reinterpret_cast<const float*>(x_buffer->address),
            .y = reinterpret_cast<const float*>(y_buffer->address),
            .out = reinterpret_cast<float*>(out_buffer->address),
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
        
        if (!write_args_to_device(*g_runtime, args_buffer->address, &kernel_args)) {
            return false;
        }

        // Phase 4: generate and transfer vector data
        std::cout << "\nGenerating random vector data for SAXPY problem (N=" << N << ")..." << std::endl;
        std::vector<float> x_data = generate_random_vector(N, -10.0f, 10.0f);
        std::vector<float> y_data = generate_random_vector(N, -5.0f, 5.0f);
        std::vector<float> out_data(N, 1.0f); // Initialize output vector with 1
        
        if (!write_vector_to_device(*g_runtime, x_buffer->address, x_data, "X")) {
            return false;
        }
        
        if (!write_vector_to_device(*g_runtime, y_buffer->address, y_data, "Y")) {
            return false;
        }

        if (!write_vector_to_device(*g_runtime, out_buffer->address, out_data, "Output")) {
            return false;
        }

        // Verify that x and y vectors were transmitted correctly by reading them back
        std::cout << "\nVerifying vector data transmission..." << std::endl;
        
        if (!verify_vector_transmission(*g_runtime, x_buffer->address, x_data, "X") ||
            !verify_vector_transmission(*g_runtime, y_buffer->address, y_data, "Y")) {
            return false;
        }
        
        // Phase 5: execute kernel (launch only, don't free memory yet)
        uint64_t entry_address = binary_buffer->address + info.entry_binary_offset;
        std::cout << "\nLaunching kernel..." << std::endl;
        std::cout << "Kernel entry address: 0x" << std::hex << entry_address << std::dec 
                  << " (base: 0x" << std::hex << binary_buffer->address << std::dec 
                  << " + offset: 0x" << std::hex << info.entry_binary_offset << std::dec << ")" << std::endl;
        
        // Create and send launch batch
        std::vector<std::pair<int, message_slot_t>> launch_batch;
        if (!create_and_send_launch_batch(*g_runtime, entry_address, args_buffer->address, launch_batch)) {
            std::cerr << "Error: Failed to create/send launch batch" << std::endl;
            return false;
        }
        
        // Wait for launch response
        std::vector<std::pair<int, message_slot_t>> launch_response_batch;
        std::vector<std::pair<int, message_slot_t>> launch_body_slots;
        if (!wait_for_response_batch(*g_runtime, launch_batch, launch_response_batch, launch_body_slots, 
                                    100, 100, "launch response")) {
            std::cerr << "Error: Launch batch failed or timed out" << std::endl;
            return false;
        }
        
        // Extract launch response
        launch_rsp_t launch_rsp;
        bool found_launch = false;
        for (const auto& [slot_index, slot] : launch_body_slots) {
            if (slot.msg_id == MSG_RSP_LAUNCH) {
                if (!MessageUtils::extractPayload(&slot, &launch_rsp)) {
                    std::cerr << "Error: Failed to extract launch response payload" << std::endl;
                    return false;
                }
                found_launch = true;
                break;
            }
        }
        
        if (!found_launch || launch_rsp.status != ERR_OK) {
            std::cerr << "Error: Kernel launch failed with status: " 
                      << MessageUtils::getErrorCodeString(launch_rsp.status) << std::endl;
            return false;
        }
        
        std::cout << "✓ Kernel launched successfully" << std::endl;
        clear_response_slots(*g_runtime, launch_response_batch);
        
        // Phase 6: Read and verify results while memory is still allocated
        std::cout << "\nReading and verifying computation results..." << std::endl;
        if (!read_and_verify_results(*g_runtime, out_buffer->address, x_data, y_data, scalar_a)) {
            std::cerr << "Error: Result verification failed" << std::endl;
            return false;
        }
        
        // Phase 7: Now free all allocated memory
        std::cout << "\nFreeing allocated memory..." << std::endl;
        std::vector<std::pair<int, message_slot_t>> free_batch;
        if (!create_and_send_free_batch(*g_runtime, allocated_buffers, free_batch)) {
            std::cerr << "Error: Failed to create/send free batch" << std::endl;
            return false;
        }
        
        // Wait for free responses
        std::vector<std::pair<int, message_slot_t>> free_response_batch;
        std::vector<std::pair<int, message_slot_t>> free_body_slots;
        if (!wait_for_response_batch(*g_runtime, free_batch, free_response_batch, free_body_slots,
                                    50, 100, "free response")) {
            std::cerr << "Error: Free batch failed or timed out" << std::endl;
            return false;
        }
        
        // Verify all free operations succeeded
        int successful_frees = 0;
        for (const auto& [slot_index, slot] : free_body_slots) {
            if (slot.msg_id == MSG_RSP_FREE) {
                free_rsp_t free_rsp;
                if (MessageUtils::extractPayload(&slot, &free_rsp) && free_rsp.status == ERR_OK) {
                    successful_frees++;
                }
            }
        }
        
        if (successful_frees != static_cast<int>(allocated_buffers.size())) {
            std::cerr << "Warning: Not all memory was freed successfully (" 
                      << successful_frees << "/" << allocated_buffers.size() << ")" << std::endl;
        } else {
            std::cout << "✓ All " << successful_frees << " memory buffers freed successfully" << std::endl;
        }
        
        clear_response_slots(*g_runtime, free_response_batch);

    } catch (const std::exception& e) {
        std::cerr << "Exception during runtime functionality test: " << e.what() << std::endl;
        return false;
    }

    std::cout << "✓ Runtime functionality test completed successfully" << std::endl;
    return true;
}

int main(int argc, char **argv) {
    std::cout << "Starting test_flat_binary..." << std::endl;

    // Determine ELF path from CLI or fallback
    std::string chosen_elf = resolve_elf_path_from_args(argc, argv);
    std::cout << "Using ELF file: " << chosen_elf << std::endl;
    
    // Test ELF file accessibility before anything else
    std::ifstream test_file(chosen_elf);
    if (!test_file) {
        std::cerr << "Error: Cannot access ELF file: " << chosen_elf << std::endl;
        return 1;
    }
    test_file.close();
    std::cout << "✓ ELF file exists and is accessible" << std::endl;

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

static std::string resolve_elf_path_from_args(int argc, char **argv) {
    if (argc > 1 && argv && argv[1]) {
        std::string argpath = argv[1];
        if (!argpath.empty()) {
            return argpath;
        }
    }
    return elf_path;
}

bool check_file_permissions(const std::string& filepath) {
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        std::cerr << "Error: Shared memory file " << filepath << " does not exist" << std::endl;
        return false;
    }
    
    // Check read and write permissions
    if (access(filepath.c_str(), R_OK | W_OK) != 0) {
        std::cerr << "Error: Current user does not have read/write permissions for " << filepath << std::endl;
        return false;
    }
    
    std::cout << "✓ Shared memory file exists and has proper permissions" << std::endl;
    return true;
}

bool check_qemu_process() {
    // Use pgrep to check for QEMU processes owned by current user (partial match)
    int result = system("pgrep -u $(id -u) -f qemu > /dev/null 2>&1");
    if (result != 0) {
        std::cerr << "Error: No QEMU process found running for current user" << std::endl;
        std::cerr << "Please start QEMU with ivshmem support before running this test" << std::endl;
        return false;
    }
    
    std::cout << "✓ QEMU process found running for current user" << std::endl;
    return true;
}

// Helper: load flat binary file and pad to page boundary
static bool load_flat_binary_file(const std::string& binary_path, std::vector<uint8_t>& flat_binary) {
    std::cout << "Loading flat binary file: " << binary_path << std::endl;
    
    // Load binary file into memory
    std::ifstream binary_file(binary_path, std::ios::binary | std::ios::ate);
    if (!binary_file) {
        std::cerr << "Error: Failed to open flat binary file " << binary_path << std::endl;
        return false;
    }
    
    std::streamsize binary_size = binary_file.tellg();
    if (binary_size <= 0) {
        std::cerr << "Error: Flat binary file " << binary_path << " is empty" << std::endl;
        return false;
    }
    
    binary_file.seekg(0, std::ios::beg);
    flat_binary.resize(static_cast<size_t>(binary_size));
    if (!binary_file.read(reinterpret_cast<char*>(flat_binary.data()), binary_size)) {
        std::cerr << "Error: Failed to read flat binary file " << binary_path << std::endl;
        return false;
    }
    
    // Pad to page boundary (4096 bytes)
    size_t unpadded_size = flat_binary.size();
    size_t padded_size = ((unpadded_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    
    if (padded_size > unpadded_size) {
        flat_binary.resize(padded_size, 0); // Pad with zeros
        std::cout << "✓ Binary padded from " << unpadded_size << " to " << padded_size << " bytes (page aligned)" << std::endl;
    }
    
    std::cout << "✓ Flat binary loaded successfully: " << flat_binary.size() << " bytes" << std::endl;
    return true;
}

// Helper: clear all D2H mailbox slots
static void clear_all_mailbox_slots(ThunderbirdHostRuntime &runtime) {
    auto writer = runtime.getWriter();
    std::cout << "Clearing all mailbox slots for clean test start..." << std::endl;
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; ++i) {
        MailboxUtils::clearD2HSlot(*writer, i);
    }
}

// Helper: create malloc batch for multiple allocations and send it
static bool create_and_send_malloc_batch(ThunderbirdHostRuntime &runtime, 
                                         const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                                         std::vector<std::pair<int, message_slot_t>> &out_batch) {
    std::cout << "Creating malloc batch for " << buffer_specs.size() << " allocations..." << std::endl;
    
    // Create batch body with one malloc command per buffer spec
    std::vector<message_slot_t> batch_body(buffer_specs.size());
    
    for (size_t i = 0; i < buffer_specs.size(); ++i) {
        const auto& [size, purpose] = buffer_specs[i];
        std::cout << "  - " << purpose << ": " << size << " bytes" << std::endl;
        
        if (!MessageUtils::createMallocCmd(&batch_body[i], static_cast<uint32_t>(size))) {
            std::cerr << "Error: Failed to create malloc command for " << purpose << std::endl;
            return false;
        }
    }

    std::cout << "\n=== MALLOC BATCH BODY MESSAGES ===" << std::endl;
    for (size_t i = 0; i < batch_body.size(); ++i) {
        const auto& [size, purpose] = buffer_specs[i];
        std::cout << "\n--- " << purpose << " Malloc Command ---" << std::endl;
        MessageUtils::printMessageSlot(&batch_body[i]);
    }
    std::cout << "============================\n" << std::endl;

    if (!create_command_batch(batch_body, 0, out_batch)) {
        std::cerr << "Error: Failed to create malloc batch" << std::endl;
        return false;
    }

    std::cout << "\n=== COMPLETE MALLOC BATCH ===" << std::endl;
    std::cout << "Batch contains " << out_batch.size() << " slots:" << std::endl;
    for (const auto& [slot_index, slot] : out_batch) {
        std::cout << "\n--- Slot " << slot_index << " ---" << std::endl;
        MessageUtils::printMessageSlot(&slot);
    }
    std::cout << "============================\n" << std::endl;

    auto writer = runtime.getWriter();
    std::cout << "Clearing slots before sending malloc batch..." << std::endl;
    for (const auto& [slot_index, _] : out_batch) {
        MailboxUtils::clearD2HSlot(*writer, slot_index);
    }

    std::cout << "Sending malloc batch to device..." << std::endl;
    for (const auto& [slot_index, slot] : out_batch) {
        std::cout << "Writing slot " << slot_index << " with message ID " 
                  << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
        if (!MailboxUtils::writeH2DMessage(*writer, slot_index, &slot)) {
            std::cerr << "Error: Failed to write malloc batch slot " << slot_index << std::endl;
            return false;
        }
    }

    return true;
}

static bool wait_for_response_batch(ThunderbirdHostRuntime &runtime,
                                    const std::vector<std::pair<int, message_slot_t>> &sent_batch,
                                    std::vector<std::pair<int, message_slot_t>> &out_response_batch,
                                    std::vector<std::pair<int, message_slot_t>> &out_body_slots,
                                    int max_iterations,
                                    int poll_ms,
                                    const char *label) {
    auto reader = runtime.getReader();
    out_response_batch.clear();
    out_body_slots.clear();

    for (int iter = 0; iter < max_iterations; ++iter) {
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));

        out_response_batch.clear();
        for (const auto& [slot_index, _] : sent_batch) {
            message_slot_t slot;
            if (MailboxUtils::readD2HMessage(*reader, slot_index, &slot)) {
                if (slot.msg_id != MSG_INVALID) {
                    out_response_batch.push_back({slot_index, slot});
                    std::cout << "Peeked " << label << " in slot " << slot_index 
                              << " with message ID " << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
                }
            }
        }

        int begin_slot, end_slot;
        bool is_cmd, is_rsp;
        if (confirm_batch_integrity(out_response_batch, begin_slot, end_slot, is_cmd, is_rsp, out_body_slots)) {
            if (is_rsp && !is_cmd) {
                std::cout << "✓ Valid " << label << " batch received!" << std::endl;
                std::cout << "\n=== " << label << " BATCH ===" << std::endl;
                for (const auto& [slot_index, slot] : out_response_batch) {
                    std::cout << "\n--- Response Slot " << slot_index << " ---" << std::endl;
                    MessageUtils::printMessageSlot(&slot);
                }
                std::cout << "============================\n" << std::endl;
                return true;
            }
        }

        if (iter % 10 == 0) {
            std::cout << "Timeout " << iter << ": Still waiting for " << label << "..." << std::endl;
        }
    }

    return false;
}

static bool extract_malloc_addresses_from_body_slots(const std::vector<std::pair<int, message_slot_t>> &body_slots,
                                                     const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                                                     std::vector<AllocatedBuffer>& out_allocated_buffers) {
    out_allocated_buffers.clear();
    
    // Count malloc responses
    std::vector<malloc_rsp_t> malloc_responses;
    for (const auto& [slot_index, slot] : body_slots) {
        if (slot.msg_id == MSG_RSP_MALLOC) {
            malloc_rsp_t malloc_rsp;
            if (!MessageUtils::extractPayload(&slot, &malloc_rsp)) {
                std::cerr << "Error: Failed to extract malloc response payload" << std::endl;
                return false;
            }
            malloc_responses.push_back(malloc_rsp);
        }
    }
    
    // Verify we have the expected number of malloc responses
    if (malloc_responses.size() != buffer_specs.size()) {
        std::cerr << "Error: Expected " << buffer_specs.size() << " malloc responses, got " << malloc_responses.size() << std::endl;
        return false;
    }
    
    // Match malloc responses to buffer specs (assume same order)
    for (size_t i = 0; i < buffer_specs.size(); ++i) {
        const auto& [expected_size, purpose] = buffer_specs[i];
        const auto& malloc_rsp = malloc_responses[i];
        
        if (malloc_rsp.status != ERR_OK) {
            std::cerr << "Error: Malloc failed for " << purpose << " with status: "
                      << MessageUtils::getErrorCodeString(malloc_rsp.status) << std::endl;
            return false;
        }
        
        // Create allocated buffer entry
        out_allocated_buffers.emplace_back(malloc_rsp.address, expected_size, purpose);
        
        std::cout << "✓ Malloc successful for " << purpose << ", allocated address: 0x" 
                  << std::hex << malloc_rsp.address << std::dec << " (" << expected_size << " bytes)" << std::endl;
    }
    
    return true;
}

// New helper function to allocate multiple buffers in a single batch
static bool allocate_buffers(ThunderbirdHostRuntime &runtime, 
                            const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                            std::vector<AllocatedBuffer>& out_allocated_buffers) {
    out_allocated_buffers.clear();
    
    std::cout << "\nAllocating " << buffer_specs.size() << " buffers in batch:" << std::endl;
    for (const auto& [size, purpose] : buffer_specs) {
        std::cout << "  - " << purpose << ": " << size << " bytes" << std::endl;
    }
    
    // Create and send malloc batch for all allocations
    std::vector<std::pair<int, message_slot_t>> malloc_batch;
    if (!create_and_send_malloc_batch(runtime, buffer_specs, malloc_batch)) {
        std::cerr << "Error: Failed during malloc batch send" << std::endl;
        return false;
    }

    // Wait for and validate malloc response batch
    std::vector<std::pair<int, message_slot_t>> malloc_response_batch;
    std::vector<std::pair<int, message_slot_t>> malloc_body_slots;
    if (!wait_for_response_batch(runtime, malloc_batch, malloc_response_batch, malloc_body_slots, 50, 100, "malloc response")) {
        std::cerr << "Error: Malloc request batch timed out or invalid" << std::endl;
        return false;
    }

    // Extract allocated addresses and match them to purposes
    if (!extract_malloc_addresses_from_body_slots(malloc_body_slots, buffer_specs, out_allocated_buffers)) {
        std::cerr << "Error: Failed to extract malloc addresses from response batch" << std::endl;
        return false;
    }

    // Clear malloc response batch slots
    clear_response_slots(runtime, malloc_response_batch);
    
    std::cout << "\n✓ Successfully allocated " << out_allocated_buffers.size() << " buffers:" << std::endl;
    for (const auto& buffer : out_allocated_buffers) {
        std::cout << "  - " << buffer.purpose << ": 0x" << std::hex << buffer.address 
                  << std::dec << " (" << buffer.size << " bytes)" << std::endl;
    }
    
    return true;
}

// New helper function to find buffer by purpose
static AllocatedBuffer* find_buffer_by_purpose(std::vector<AllocatedBuffer>& buffers, const std::string& purpose) {
    for (auto& buffer : buffers) {
        if (buffer.purpose == purpose) {
            return &buffer;
        }
    }
    return nullptr;
}

static std::vector<float> generate_random_vector(size_t size, float min_val, float max_val) {
    std::vector<float> data(size);
    
    // Use a fixed seed for reproducible results (or use random_device for true randomness)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min_val, max_val);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(gen);
    }
    
    std::cout << "Generated " << size << " random floats in range [" 
              << min_val << ", " << max_val << "]" << std::endl;
    std::cout << "First few values: ";
    for (size_t i = 0; i < std::min(size_t(5), size); ++i) {
        std::cout << data[i] << " ";
    }
    std::cout << "..." << std::endl;
    
    return data;
}

static bool write_vector_to_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, const std::vector<float>& data, const std::string& name) {
    auto writer = runtime.getWriter();
    std::cout << "Writing " << name << " vector to allocated memory..." << std::endl;
    
    size_t byte_size = data.size() * sizeof(float);
    int64_t written = writer->transfer(allocated_address - IVSHMEM_BASE_ADDRESS, 
                                     reinterpret_cast<const uint8_t*>(data.data()), 
                                     byte_size);
    if (written != static_cast<int64_t>(byte_size)) {
        std::cerr << "Error: Failed to write " << name << " vector to device memory (written=" << written 
                  << ", expected=" << byte_size << ")" << std::endl;
        return false;
    }
    
    std::cout << "✓ Successfully wrote " << data.size() << " floats (" << byte_size 
              << " bytes) for " << name << " vector to device memory" << std::endl;
    return true;
}

static bool read_vector_from_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, std::vector<float>& data, const std::string& name) {
    auto reader = runtime.getReader();
    std::cout << "Reading " << name << " vector from device memory for verification..." << std::endl;
    
    size_t byte_size = data.size() * sizeof(float);
    int64_t bytes_read = reader->transfer(allocated_address - IVSHMEM_BASE_ADDRESS,
                                         reinterpret_cast<uint8_t*>(data.data()),
                                         byte_size);
    
    if (bytes_read != static_cast<int64_t>(byte_size)) {
        std::cerr << "Error: Failed to read " << name << " vector from device memory (read=" 
                  << bytes_read << ", expected=" << byte_size << ")" << std::endl;
        return false;
    }
    
    std::cout << "✓ Successfully read " << data.size() << " floats (" << byte_size 
              << " bytes) for " << name << " vector from device memory" << std::endl;
    return true;
}

static bool write_flatbin_to_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address) {
    auto writer = runtime.getWriter();
    std::cout << "Writing flat binary to allocated memory..." << std::endl;
    int64_t written = writer->transfer(allocated_address - IVSHMEM_BASE_ADDRESS, flatbin_vec.data(), flatbin_vec.size());
    if (written != static_cast<int64_t>(flatbin_vec.size())) {
        std::cerr << "Error: Failed to write flat binary to device memory (written=" << written << ")" << std::endl;
        return false;
    }
    std::cout << "✓ Successfully wrote " << flatbin_vec.size() << " bytes to device memory" << std::endl;
    return true;
}

static bool write_args_to_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, const args_t* args_data) {
    auto writer = runtime.getWriter();
    std::cout << "Writing arguments to allocated memory..." << std::endl;
    
    if (!args_data) {
        std::cerr << "Error: args_data pointer is null" << std::endl;
        return false;
    }
    
    int64_t written = writer->transfer(allocated_address - IVSHMEM_BASE_ADDRESS, 
                                     reinterpret_cast<const uint8_t*>(args_data), 
                                     sizeof(args_t));
    if (written != static_cast<int64_t>(sizeof(args_t))) {
        std::cerr << "Error: Failed to write args to device memory (written=" << written 
                  << ", expected=" << sizeof(args_t) << ")" << std::endl;
        return false;
    }
    
    std::cout << "✓ Successfully wrote " << sizeof(args_t) << " bytes of arguments to device memory" << std::endl;
    return true;
}


// Clear the slots listed in response_batch
static void clear_response_slots(ThunderbirdHostRuntime &runtime, const std::vector<std::pair<int, message_slot_t>> &response_batch) {
    auto writer = runtime.getWriter();
    for (const auto& [clear_slot_idx, _] : response_batch) {
        MailboxUtils::clearD2HSlot(*writer, clear_slot_idx);
    }
}

// NEW FUNCTIONS FOR SEPARATED EXECUTION

// Helper: create and send kernel launch batch (launch only, no memory free)
static bool create_and_send_launch_batch(ThunderbirdHostRuntime &runtime, 
                                         uint64_t entry_address, 
                                         uint64_t args_address, 
                                         std::vector<std::pair<int, message_slot_t>> &out_launch_batch) {
    std::cout << "Creating kernel launch batch..." << std::endl;
    std::cout << "Entry address: 0x" << std::hex << entry_address << std::dec << std::endl;
    std::cout << "Args address: 0x" << std::hex << args_address << std::dec << std::endl;
    
    // Create batch with only launch command
    std::vector<message_slot_t> launch_batch_body(1);

    if (!MessageUtils::createLaunchCmd(&launch_batch_body[0], entry_address, 
                                      NUM_BLOCKS_X, NUM_BLOCKS_Y, NUM_BLOCKS_Z, 
                                      NUM_THREADS_PER_BLOCKDIM, NUM_THREADS_PER_BLOCKDIM, 
                                      NUM_THREADS_PER_BLOCKDIM, args_address)) {
        std::cerr << "Error: Failed to create launch command" << std::endl;
        return false;
    }

    if (!create_command_batch(launch_batch_body, 0, out_launch_batch)) {
        std::cerr << "Error: Failed to create launch batch" << std::endl;
        return false;
    }

    std::cout << "\n=== LAUNCH BATCH ===" << std::endl;
    std::cout << "Batch contains " << out_launch_batch.size() << " slots:" << std::endl;
    for (const auto& [slot_index, slot] : out_launch_batch) {
        std::cout << "\n--- Slot " << slot_index << " ---" << std::endl;
        MessageUtils::printMessageSlot(&slot);
    }
    std::cout << "==================\n" << std::endl;

    // Send the launch batch
    auto writer = runtime.getWriter();
    std::cout << "Clearing slots before sending launch batch..." << std::endl;
    for (const auto& [slot_index, _] : out_launch_batch) {
        MailboxUtils::clearD2HSlot(*writer, slot_index);
    }

    std::cout << "Sending launch batch to device..." << std::endl;
    for (const auto& [slot_index, slot] : out_launch_batch) {
        std::cout << "Writing slot " << slot_index << " with message ID " 
                  << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
        if (!MailboxUtils::writeH2DMessage(*writer, slot_index, &slot)) {
            std::cerr << "Error: Failed to write launch batch slot " << slot_index << std::endl;
            return false;
        }
    }

    return true;
}

// Helper: create and send memory free batch for all allocated buffers
static bool create_and_send_free_batch(ThunderbirdHostRuntime &runtime,
                                       const std::vector<AllocatedBuffer>& allocated_buffers,
                                       std::vector<std::pair<int, message_slot_t>> &out_free_batch) {
    std::cout << "Creating memory free batch for " << allocated_buffers.size() << " buffers..." << std::endl;
    
    // Create batch with free commands for all buffers
    std::vector<message_slot_t> free_batch_body(allocated_buffers.size());
    
    for (size_t i = 0; i < allocated_buffers.size(); ++i) {
        const auto& buffer = allocated_buffers[i];
        std::cout << "  - Freeing " << buffer.purpose << " at 0x" << std::hex << buffer.address << std::dec << std::endl;
        
        if (!MessageUtils::createFreeCmd(&free_batch_body[i], buffer.address)) {
            std::cerr << "Error: Failed to create free command for " << buffer.purpose << std::endl;
            return false;
        }
    }

    if (!create_command_batch(free_batch_body, 0, out_free_batch)) {
        std::cerr << "Error: Failed to create free batch" << std::endl;
        return false;
    }

    std::cout << "\n=== FREE BATCH ===" << std::endl;
    std::cout << "Batch contains " << out_free_batch.size() << " slots:" << std::endl;
    for (const auto& [slot_index, slot] : out_free_batch) {
        std::cout << "\n--- Slot " << slot_index << " ---" << std::endl;
        MessageUtils::printMessageSlot(&slot);
    }
    std::cout << "================\n" << std::endl;

    // Send the free batch
    auto writer = runtime.getWriter();
    std::cout << "Clearing slots before sending free batch..." << std::endl;
    for (const auto& [slot_index, _] : out_free_batch) {
        MailboxUtils::clearD2HSlot(*writer, slot_index);
    }

    std::cout << "Sending free batch to device..." << std::endl;
    for (const auto& [slot_index, slot] : out_free_batch) {
        std::cout << "Writing slot " << slot_index << " with message ID " 
                  << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
        if (!MailboxUtils::writeH2DMessage(*writer, slot_index, &slot)) {
            std::cerr << "Error: Failed to write free batch slot " << slot_index << std::endl;
            return false;
        }
    }

    return true;
}

// Helper: read results from device memory and verify SAXPY computation
static bool read_and_verify_results(ThunderbirdHostRuntime &runtime, 
                                    uint64_t out_address,
                                    const std::vector<float>& x_data,
                                    const std::vector<float>& y_data,
                                    float scalar_a) {
    auto reader = runtime.getReader();
    std::cout << "Reading computation results from device memory..." << std::endl;
    
    // Read output vector from device
    std::vector<float> device_results(N);
    size_t byte_size = device_results.size() * sizeof(float);
    
    int64_t bytes_read = reader->transfer(out_address - IVSHMEM_BASE_ADDRESS,
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

// Helper: verify vector transmission by reading back and comparing
static bool verify_vector_transmission(ThunderbirdHostRuntime &runtime, 
                                       uint64_t allocated_address, 
                                       const std::vector<float>& original_data, 
                                       const std::string& name) {
    std::vector<float> readback_data(original_data.size());
    
    if (!read_vector_from_device(runtime, allocated_address, readback_data, name)) {
        return false;
    }
    
    const float tolerance = 1e-6f;
    for (size_t i = 0; i < original_data.size(); ++i) {
        if (std::abs(readback_data[i] - original_data[i]) > tolerance) {
            std::cerr << name << " vector mismatch at index " << i << ": wrote=" << original_data[i] 
                      << ", read=" << readback_data[i] << std::endl;
            return false;
        }
    }
    
    std::cout << "✓ " << name << " vector data transmission verification successful" << std::endl;
    return true;
}
