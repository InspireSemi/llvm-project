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
#include <vector>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <chrono>

constexpr uint64_t IVSHMEM_BASE_ADDRESS = 0x82000000000000ull;

// Preloaded llext extension in llext elf format.
static uint8_t llext_buffer[] = {
#include "hello_world_ext.inc"
};

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

// Global test state
std::unique_ptr<ThunderbirdHostRuntime> g_runtime;

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
    
    // Clear all mailbox slots at the start to ensure clean state
    auto writer = g_runtime->getWriter();
    std::cout << "Clearing all mailbox slots for clean test start..." << std::endl;
    for (int i = 0; i < 32; ++i) {  // Assuming 32 slots max
        MailboxUtils::clearD2HSlot(*writer, i);
    }
    
    try {
        // Step 1: Create and send malloc command batch
        size_t llext_size = sizeof(llext_buffer);
        std::cout << "Creating malloc request for " << llext_size << " bytes..." << std::endl;
        
        std::vector<message_slot_t> batch_body(1);
        if (!MessageUtils::createMallocCmd(&batch_body[0], static_cast<uint32_t>(llext_size))) {
            std::cerr << "Error: Failed to create malloc command" << std::endl;
            return false;
        }
        
        std::cout << "\n=== MALLOC COMMAND MESSAGE ===" << std::endl;
        MessageUtils::printMessageSlot(&batch_body[0]);
        std::cout << "============================\n" << std::endl;
        
        std::vector<std::pair<int, message_slot_t>> malloc_batch;
        if (!create_command_batch(batch_body, 0, malloc_batch)) {
            std::cerr << "Error: Failed to create malloc batch" << std::endl;
            return false;
        }
        
        std::cout << "\n=== COMPLETE MALLOC BATCH ===" << std::endl;
        std::cout << "Batch contains " << malloc_batch.size() << " slots:" << std::endl;
        for (const auto& [slot_index, slot] : malloc_batch) {
            std::cout << "\n--- Slot " << slot_index << " ---" << std::endl;
            MessageUtils::printMessageSlot(&slot);
        }
        std::cout << "============================\n" << std::endl;
        
        auto writer = g_runtime->getWriter();
        auto reader = g_runtime->getReader();
        
        // Clear any existing responses in the slots we're about to use
        std::cout << "Clearing slots before sending malloc batch..." << std::endl;
        for (const auto& [slot_index, _] : malloc_batch) {
            MailboxUtils::clearD2HSlot(*writer, slot_index);
        }
        
        std::cout << "Sending malloc batch to device..." << std::endl;
        for (const auto& [slot_index, slot] : malloc_batch) {
            std::cout << "Writing slot " << slot_index << " with message ID " 
                     << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
            if (!MailboxUtils::writeH2DMessage(*writer, slot_index, &slot)) {
                std::cerr << "Error: Failed to write malloc batch slot " << slot_index << std::endl;
                return false;
            }
        }
        
        std::cout << "Malloc batch sent, waiting for response..." << std::endl;
        
        // Step 2: Wait for and validate malloc response batch
        std::vector<std::pair<int, message_slot_t>> malloc_response_batch;
        std::vector<std::pair<int, message_slot_t>> malloc_body_slots;
        bool malloc_response_received = false;
        
        for (int timeout = 0; timeout < 50 && !malloc_response_received; ++timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            malloc_response_batch.clear();
            for (const auto& [slot_index, _] : malloc_batch) {
                message_slot_t slot;
                if (MailboxUtils::readD2HMessage(*reader, slot_index, &slot)) {
                    if (slot.msg_id != MSG_INVALID) {
                        malloc_response_batch.push_back({slot_index, slot});
                        std::cout << "Peeked response in slot " << slot_index 
                                 << " with message ID " << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
                    }
                }
            }
            
            int begin_slot, end_slot;
            bool is_cmd, is_rsp;
            
            if (confirm_batch_integrity(malloc_response_batch, begin_slot, end_slot, is_cmd, is_rsp, malloc_body_slots)) {
                if (is_rsp && !is_cmd) {
                    std::cout << "✓ Valid malloc response batch received!" << std::endl;
                    
                    std::cout << "\n=== MALLOC RESPONSE BATCH ===" << std::endl;
                    for (const auto& [slot_index, slot] : malloc_response_batch) {
                        std::cout << "\n--- Response Slot " << slot_index << " ---" << std::endl;
                        MessageUtils::printMessageSlot(&slot);
                    }
                    std::cout << "============================\n" << std::endl;
                    
                    malloc_response_received = true;
                    break;
                }
            }
            
            if (timeout % 10 == 0) {
                std::cout << "Timeout " << timeout << ": Still waiting for malloc response..." << std::endl;
            }
        }
        
        if (!malloc_response_received) {
            std::cerr << "Error: Malloc request timed out" << std::endl;
            return false;
        }
        
        // Step 3: Extract malloc response data (use already validated body_slots)
        uint64_t allocated_address = 0;
        bool found_malloc_response = false;
        
        for (const auto& [slot_index, slot] : malloc_body_slots) {
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
                
                allocated_address = malloc_rsp.address;
                found_malloc_response = true;
                break;
            }
        }
        
        if (!found_malloc_response) {
            std::cerr << "Error: No malloc response found in validated batch" << std::endl;
            return false;
        }
        
        std::cout << "✓ Malloc successful, allocated address: 0x" << std::hex << allocated_address << std::dec << std::endl;
        
        // Clear malloc response batch slots
        for (const auto& [clear_slot_idx, _] : malloc_response_batch) {
            MailboxUtils::clearD2HSlot(*writer, clear_slot_idx);
        }
        
        // Step 4: Write the llext buffer to allocated memory
        std::cout << "Writing llext buffer to allocated memory..." << std::endl;
        if (writer->transfer(allocated_address - IVSHMEM_BASE_ADDRESS, llext_buffer, llext_size) != llext_size) {
            std::cerr << "Error: Failed to write llext buffer to device memory" << std::endl;
            return false;
        }
        std::cout << "✓ Successfully wrote " << llext_size << " bytes to device memory" << std::endl;
        
        // Step 5: Create and send execution batch (launch + free)
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
        
        std::vector<std::pair<int, message_slot_t>> exec_batch;
        if (!create_command_batch(exec_batch_body, 10, exec_batch)) {
            std::cerr << "Error: Failed to create execution batch" << std::endl;
            return false;
        }
        
        std::cout << "\n=== COMPLETE EXECUTION BATCH ===" << std::endl;
        std::cout << "Batch contains " << exec_batch.size() << " slots:" << std::endl;
        for (const auto& [slot_index, slot] : exec_batch) {
            std::cout << "\n--- Slot " << slot_index << " ---" << std::endl;
            MessageUtils::printMessageSlot(&slot);
        }
        std::cout << "============================\n" << std::endl;
        
        // Clear any existing responses in the slots we're about to use
        std::cout << "Clearing slots before sending execution batch..." << std::endl;
        for (const auto& [slot_index, _] : exec_batch) {
            MailboxUtils::clearD2HSlot(*writer, slot_index);
        }
        
        std::cout << "Sending execution batch to device..." << std::endl;
        for (const auto& [slot_index, slot] : exec_batch) {
            std::cout << "Writing slot " << slot_index << " with message ID " 
                     << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
            if (!MailboxUtils::writeH2DMessage(*writer, slot_index, &slot)) {
                std::cerr << "Error: Failed to write execution batch slot " << slot_index << std::endl;
                return false;
            }
        }
        
        std::cout << "Execution batch sent, waiting for response..." << std::endl;
        
        // Step 6: Wait for and validate execution response batch
        std::vector<std::pair<int, message_slot_t>> exec_response_batch;
        std::vector<std::pair<int, message_slot_t>> exec_body_slots;
        bool exec_response_received = false;
        
        for (int timeout = 0; timeout < 50 && !exec_response_received; ++timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            exec_response_batch.clear();
            for (const auto& [slot_index, _] : exec_batch) {
                message_slot_t slot;
                if (MailboxUtils::readD2HMessage(*reader, slot_index, &slot)) {
                    if (slot.msg_id != MSG_INVALID) {
                        exec_response_batch.push_back({slot_index, slot});
                        std::cout << "Peeked response in slot " << slot_index 
                                 << " with message ID " << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
                    }
                }
            }
            
            int begin_slot, end_slot;
            bool is_cmd, is_rsp;
            
            if (confirm_batch_integrity(exec_response_batch, begin_slot, end_slot, is_cmd, is_rsp, exec_body_slots)) {
                if (is_rsp && !is_cmd) {
                    std::cout << "✓ Valid execution response batch received!" << std::endl;
                    
                    std::cout << "\n=== EXECUTION RESPONSE BATCH ===" << std::endl;
                    for (const auto& [slot_index, slot] : exec_response_batch) {
                        std::cout << "\n--- Response Slot " << slot_index << " ---" << std::endl;
                        MessageUtils::printMessageSlot(&slot);
                    }
                    std::cout << "============================\n" << std::endl;
                    
                    exec_response_received = true;
                    break;
                }
            }
            
            if (timeout % 10 == 0) {
                std::cout << "Timeout " << timeout << ": Still waiting for execution response..." << std::endl;
            }
        }
        
        if (!exec_response_received) {
            std::cerr << "Error: Execution batch response timed out" << std::endl;
            return false;
        }
        
        // Step 7: Extract execution response data (use already validated body_slots)
        bool found_launch_response = false, found_free_response = false;
        launch_rsp_t launch_rsp;
        free_rsp_t free_rsp;
        
        for (const auto& [slot_index, slot] : exec_body_slots) {
            if (slot.msg_id == MSG_RSP_LAUNCH) {
                if (!MessageUtils::extractPayload(&slot, &launch_rsp)) {
                    std::cerr << "Error: Failed to extract launch response payload" << std::endl;
                    return false;
                }
                found_launch_response = true;
            } else if (slot.msg_id == MSG_RSP_FREE) {
                if (!MessageUtils::extractPayload(&slot, &free_rsp)) {
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
        
        // Clear execution response batch slots
        for (const auto& [clear_slot_idx, _] : exec_response_batch) {
            MailboxUtils::clearD2HSlot(*writer, clear_slot_idx);
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

int main() {
    std::cout << "ThunderbirdHostRuntime Tests" << std::endl;
    std::cout << "============================" << std::endl;
    
    // Run tests in sequence, exit early on failure
    std::cout << "\n1. Testing ThunderbirdHostRuntime construction..." << std::endl;
    if (!test_thunderbird_runtime_construction()) {
        std::cout << "Construction test failed ✗" << std::endl;
        return 1;
    }
    std::cout << "Construction test passed ✓" << std::endl;
    
    std::cout << "\n2. Testing runtime functionality..." << std::endl;
    if (!test_runtime_functionality()) {
        std::cout << "Runtime functionality test failed ✗" << std::endl;
        return 1;
    }
    std::cout << "Runtime functionality test passed ✓" << std::endl;
    
    std::cout << "\nAll tests passed ✓" << std::endl;
    return 0;
}
