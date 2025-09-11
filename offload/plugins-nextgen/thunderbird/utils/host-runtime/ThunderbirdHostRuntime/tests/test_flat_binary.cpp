/**
 * @file test_thunderbird_runtime.cpp
 * @author Generated for Thunderbird Host Runtime Tests
 * @brief Simple initialization test for ThunderbirdHostRuntime
 * @details Tests basic construction and initialization of the ThunderbirdHostRuntime class
 * @version 0.1
 * @date 2025-08-03
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "ThunderbirdRuntime.hpp"
#include "DataTransferEngineFactory.hpp"
#include "DataTransferBackend.hpp"
#include "MailboxUtils.hpp"
#include "BatchUtils.hpp"
#include "MessageUtils.hpp"
#include <iostream>
#include <fstream>
#include "ivshmem_config.h"
#include <vector>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <chrono>

constexpr uint64_t IVSHMEM_BASE_ADDRESS = 0x82000000000000ull;

static std::vector<uint8_t> flatbin_vec;

// Forward declarations (implementations moved below main)
static bool load_flat_binary_file(const std::string &path);

static const std::string flat_binary_path = "/home/mbrothers/tiny_code.bin";

static std::string resolve_flat_binary_path_from_args(int argc, char **argv);

bool check_file_permissions(const std::string& filepath);

bool check_qemu_process();

// Global test state
std::unique_ptr<ThunderbirdHostRuntime> g_runtime;

// Helper forward declarations
static void clear_all_mailbox_slots(ThunderbirdHostRuntime &runtime);

// Helper: create malloc command and send batch
static bool create_and_send_malloc_batch(ThunderbirdHostRuntime &runtime, size_t size, std::vector<std::pair<int, message_slot_t>> &out_batch);

// Helper: wait for a response batch for the given sent batch and validate integrity
static bool wait_for_response_batch(ThunderbirdHostRuntime &runtime,
                                    const std::vector<std::pair<int, message_slot_t>> &sent_batch,
                                    std::vector<std::pair<int, message_slot_t>> &out_response_batch,
                                    std::vector<std::pair<int, message_slot_t>> &out_body_slots,
                                    int max_iterations = 50,
                                    int poll_ms = 100,
                                    const char *label = "response");

// Helper: extract malloc address from validated body slots
static bool extract_malloc_address_from_body_slots(const std::vector<std::pair<int, message_slot_t>> &body_slots, uint64_t &out_address);

// Helper: write the flat binary to device memory
static bool write_flatbin_to_device(ThunderbirdHostRuntime &runtime, uint64_t allocated_address);

// Helper: create execution batch (launch + free), send it and wait for response; fills launch_rsp and free_rsp
// New helper: create execution batch (launch + free) and send it. Returns the created exec_batch on success.
static bool create_and_send_exec_batch(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, std::vector<std::pair<int, message_slot_t>> &out_exec_batch);

static bool wait_for_exec_response(ThunderbirdHostRuntime &runtime,
                                   const std::vector<std::pair<int, message_slot_t>> &exec_batch,
                                   std::vector<std::pair<int, message_slot_t>> &out_response_batch,
                                   std::vector<std::pair<int, message_slot_t>> &out_body_slots,
                                   int max_iterations = 100,
                                   int poll_ms = 100);

// Extract launch and free responses from validated body slots
static bool extract_exec_responses(const std::vector<std::pair<int, message_slot_t>> &body_slots,
                                  launch_rsp_t &out_launch_rsp,
                                  free_rsp_t &out_free_rsp);

// Clear the slots listed in response_batch
static void clear_response_slots(ThunderbirdHostRuntime &runtime, const std::vector<std::pair<int, message_slot_t>> &response_batch);

// NOTE: wrapper removed — use create_and_send_exec_batch() followed by wait_and_extract_exec_responses()

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

    if (flatbin_vec.empty()) {
        std::cerr << "Error: flat binary not loaded (flatbin_vec is empty)" << std::endl;
        return false;
    }

    try {
        // Phase 0: ensure mailboxes are clear
        clear_all_mailbox_slots(*g_runtime);

        // Phase 1: allocate remote buffer
        std::vector<std::pair<int, message_slot_t>> malloc_batch;
        if (!create_and_send_malloc_batch(*g_runtime, flatbin_vec.size(), malloc_batch)) {
            std::cerr << "Error: Failed during malloc batch send" << std::endl;
            return false;
        }

        // Phase 2: wait for and validate malloc response
        std::vector<std::pair<int, message_slot_t>> malloc_response_batch;
        std::vector<std::pair<int, message_slot_t>> malloc_body_slots;
        if (!wait_for_response_batch(*g_runtime, malloc_batch, malloc_response_batch, malloc_body_slots, 50, 100, "malloc response")) {
            std::cerr << "Error: Malloc request timed out or invalid" << std::endl;
            return false;
        }

        // Phase 3: extract allocated address
        uint64_t allocated_address = 0;
        if (!extract_malloc_address_from_body_slots(malloc_body_slots, allocated_address)) {
            std::cerr << "Error: No malloc response found in validated batch" << std::endl;
            return false;
        }

        std::cout << "✓ Malloc successful, allocated address: 0x" << std::hex << allocated_address << std::dec << std::endl;

        // Clear malloc response batch slots
        auto writer = g_runtime->getWriter();
        for (const auto& [clear_slot_idx, _] : malloc_response_batch) {
            MailboxUtils::clearD2HSlot(*writer, clear_slot_idx);
        }

        // Phase 4: transfer binary
        if (!write_flatbin_to_device(*g_runtime, allocated_address)) {
            return false;
        }

        // Phase 5: execute and free remote buffer
        launch_rsp_t launch_rsp;
        free_rsp_t free_rsp;
        std::vector<std::pair<int, message_slot_t>> exec_batch;
        if (!create_and_send_exec_batch(*g_runtime, allocated_address, exec_batch)) {
            std::cerr << "Error: Failed to create/send execution batch" << std::endl;
            return false;
        }
        std::vector<std::pair<int, message_slot_t>> exec_response_batch;
        std::vector<std::pair<int, message_slot_t>> exec_body_slots;
        if (!wait_for_exec_response(*g_runtime, exec_batch, exec_response_batch, exec_body_slots)) {
            std::cerr << "Error: Execution batch failed or timed out" << std::endl;
            return false;
        }

        if (!extract_exec_responses(exec_body_slots, launch_rsp, free_rsp)) {
            std::cerr << "Error: Failed to extract exec responses" << std::endl;
            return false;
        }

        // Clear execution response batch slots
        clear_response_slots(*g_runtime, exec_response_batch);

        // Process the responses
        if (launch_rsp.status == ERR_OK) {
            std::cout << "✓ Kernel launched successfully" << std::endl;
        } else {
            std::cerr << "Warning: Launch failed with status: " 
                      << MessageUtils::getErrorCodeString(launch_rsp.status) << std::endl;
        }

        if (free_rsp.status == ERR_OK) {
            std::cout << "✓ Memory freed successfully" << std::endl;
        } else {
            std::cerr << "Warning: Free failed with status: " 
                      << MessageUtils::getErrorCodeString(free_rsp.status) << std::endl;
        }

        if (launch_rsp.status != ERR_OK || free_rsp.status != ERR_OK) {
            std::cerr << "Error: Not all execution responses were successful" << std::endl;
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
    std::cout << "Starting test_flat_binary..." << std::endl;

    // Determine flat binary path from CLI or fallback
    std::string chosen_flatbin = resolve_flat_binary_path_from_args(argc, argv);
    std::cout << "Using flat binary file: " << chosen_flatbin << std::endl;

    // Load the flat binary file
    if (!load_flat_binary_file(chosen_flatbin)) {
        std::cerr << "Error: Failed to load flat binary from " << chosen_flatbin << std::endl;
        return 1;
    }
    std::cout << "Loaded flat binary: " << flatbin_vec.size() << " bytes" << std::endl;

    // Construct runtime
    if (!test_thunderbird_runtime_construction()) {
        std::cerr << "Error: Failed to construct ThunderbirdHostRuntime" << std::endl;
        return 2;
    }

    // Run the functionality test
    if (!test_runtime_functionality()) {
        std::cerr << "Error: Runtime functionality test failed" << std::endl;
        return 3;
    }

    std::cout << "All tests passed" << std::endl;
    return 0;
}

// Implementations moved here to keep main near the top of the file

static bool load_flat_binary_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::cerr << "Error: failed to open " << path << std::endl;
        return false;
    }
    std::streamsize sz = f.tellg();
    if (sz <= 0) {
        std::cerr << "Error: file " << path << " is empty or tellg() failed" << std::endl;
        return false;
    }
    f.seekg(0, std::ios::beg);
    flatbin_vec.resize(static_cast<size_t>(sz));
    if (!f.read(reinterpret_cast<char*>(flatbin_vec.data()), sz)) {
        std::cerr << "Error: failed to read " << path << std::endl;
        return false;
    }
    // size is available via flatbin_vec.size() when needed
    return true;
}

static std::string resolve_flat_binary_path_from_args(int argc, char **argv) {
    if (argc > 1 && argv && argv[1]) {
        std::string argpath = argv[1];
        if (!argpath.empty()) {
            return argpath;
        }
    }
    return flat_binary_path;
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

// Helper: clear all D2H mailbox slots
static void clear_all_mailbox_slots(ThunderbirdHostRuntime &runtime) {
    auto writer = runtime.getWriter();
    std::cout << "Clearing all mailbox slots for clean test start..." << std::endl;
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; ++i) {
        MailboxUtils::clearD2HSlot(*writer, i);
    }
}

// Helper: create malloc command and send batch
static bool create_and_send_malloc_batch(ThunderbirdHostRuntime &runtime, size_t size, std::vector<std::pair<int, message_slot_t>> &out_batch) {
    std::cout << "Creating malloc request for " << size << " bytes..." << std::endl;
    std::vector<message_slot_t> batch_body(1);
    if (!MessageUtils::createMallocCmd(&batch_body[0], static_cast<uint32_t>(size))) {
        std::cerr << "Error: Failed to create malloc command" << std::endl;
        return false;
    }

    std::cout << "\n=== MALLOC COMMAND MESSAGE ===" << std::endl;
    MessageUtils::printMessageSlot(&batch_body[0]);
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

static bool extract_malloc_address_from_body_slots(const std::vector<std::pair<int, message_slot_t>> &body_slots, uint64_t &out_address) {
    for (const auto& [slot_index, slot] : body_slots) {
        if (slot.msg_id == MSG_RSP_MALLOC) {
            malloc_rsp_t malloc_rsp;
            if (!MessageUtils::extractPayload(&slot, &malloc_rsp)) {
                std::cerr << "Error: Failed to extract malloc response payload" << std::endl;
                return false;
            }

            if (malloc_rsp.status != ERR_OK) {
                std::cerr << "Error: Malloc failed with status: "
                          << MessageUtils::getErrorCodeString(malloc_rsp.status) << std::endl;
                return false;
            }

            out_address = malloc_rsp.address;
            return true;
        }
    }
    return false;
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

static bool create_and_send_exec_batch(ThunderbirdHostRuntime &runtime, uint64_t allocated_address, std::vector<std::pair<int, message_slot_t>> &out_exec_batch) {
    std::cout << "Creating kernel launch and free batch..." << std::endl;
    std::vector<message_slot_t> exec_batch_body(2);
    if (!MessageUtils::createLaunchCmd(&exec_batch_body[0], allocated_address, 1, 1, 1, 2, 2, 2)) {
        std::cerr << "Error: Failed to create launch command" << std::endl;
        return false;
    }
    if (!MessageUtils::createFreeCmd(&exec_batch_body[1], allocated_address)) {
        std::cerr << "Error: Failed to create free command" << std::endl;
        return false;
    }

    std::cout << "\n=== EXECUTION BATCH BODY MESSAGES ===" << std::endl;
    for (size_t i = 0; i < exec_batch_body.size(); ++i) {
        std::cout << "\n--- Body Message " << i << " ---" << std::endl;
        MessageUtils::printMessageSlot(&exec_batch_body[i]);
    }
    std::cout << "============================\n" << std::endl;

    if (!create_command_batch(exec_batch_body, 10, out_exec_batch)) {
        std::cerr << "Error: Failed to create execution batch" << std::endl;
        return false;
    }

    std::cout << "\n=== COMPLETE EXECUTION BATCH ===" << std::endl;
    std::cout << "Batch contains " << out_exec_batch.size() << " slots:" << std::endl;
    for (const auto& [slot_index, slot] : out_exec_batch) {
        std::cout << "\n--- Slot " << slot_index << " ---" << std::endl;
        MessageUtils::printMessageSlot(&slot);
    }
    std::cout << "============================\n" << std::endl;

    auto writer = runtime.getWriter();
    std::cout << "Clearing slots before sending execution batch..." << std::endl;
    for (const auto& [slot_index, _] : out_exec_batch) {
        MailboxUtils::clearD2HSlot(*writer, slot_index);
    }

    std::cout << "Sending execution batch to device..." << std::endl;
    for (const auto& [slot_index, slot] : out_exec_batch) {
        std::cout << "Writing slot " << slot_index << " with message ID " 
                  << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
        if (!MailboxUtils::writeH2DMessage(*writer, slot_index, &slot)) {
            std::cerr << "Error: Failed to write execution batch slot " << slot_index << std::endl;
            return false;
        }
    }

    return true;
}

// Wait for execution response batch and return the validated body slots.
static bool wait_for_exec_response(ThunderbirdHostRuntime &runtime,
                                   const std::vector<std::pair<int, message_slot_t>> &exec_batch,
                                   std::vector<std::pair<int, message_slot_t>> &out_response_batch,
                                   std::vector<std::pair<int, message_slot_t>> &out_body_slots,
                                   int max_iterations,
                                   int poll_ms) {
    return wait_for_response_batch(runtime, exec_batch, out_response_batch, out_body_slots, max_iterations, poll_ms, "exec response");
}

static bool extract_exec_responses(const std::vector<std::pair<int, message_slot_t>> &body_slots,
                                  launch_rsp_t &out_launch_rsp,
                                  free_rsp_t &out_free_rsp) {
    bool found_launch_response = false, found_free_response = false;
    for (const auto& [slot_index, slot] : body_slots) {
        if (slot.msg_id == MSG_RSP_LAUNCH) {
            if (!MessageUtils::extractPayload(&slot, &out_launch_rsp)) {
                std::cerr << "Error: Failed to extract launch response payload" << std::endl;
                return false;
            }
            found_launch_response = true;
        } else if (slot.msg_id == MSG_RSP_FREE) {
            if (!MessageUtils::extractPayload(&slot, &out_free_rsp)) {
                std::cerr << "Error: Failed to extract free response payload" << std::endl;
                return false;
            }
            found_free_response = true;
        }
    }

    if (!found_launch_response || !found_free_response) {
        std::cerr << "Error: Missing expected responses in validated batch (launch: " 
                  << (found_launch_response ? "YES" : "NO") << ", free: " 
                  << (found_free_response ? "YES" : "NO") << ")" << std::endl;
        return false;
    }
    return true;
}

// Clear the slots listed in response_batch
static void clear_response_slots(ThunderbirdHostRuntime &runtime, const std::vector<std::pair<int, message_slot_t>> &response_batch) {
    auto writer = runtime.getWriter();
    for (const auto& [clear_slot_idx, _] : response_batch) {
        MailboxUtils::clearD2HSlot(*writer, clear_slot_idx);
    }
}
