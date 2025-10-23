#pragma once

#include "BufferUtils.hpp"
#include "BatchUtils.tpp"
#include "MailboxUtils.hpp"
#include <iostream>

namespace BufferUtils {

// Forward declarations of helper functions
static bool extract_malloc_addresses_from_body_slots(const std::vector<std::pair<int, message_slot_t>> &body_slots,
                                                     const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                                                     std::unordered_map<std::string, AllocatedBuffer>& out_allocated_buffers_map);

template<typename Writer>
static void clear_response_slots(Writer* writer, const std::vector<std::pair<int, message_slot_t>> &response_batch);

template<typename Writer, typename Reader>
bool allocate_buffers(Writer* writer, Reader* reader,
                     const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                     std::unordered_map<std::string, AllocatedBuffer>& out_allocated_buffers_map) {
    out_allocated_buffers_map.clear();

    std::cout << "\nAllocating " << buffer_specs.size() << " buffers in batch:" << std::endl;
    for (const auto& [size, purpose] : buffer_specs) {
        std::cout << "  - " << purpose << ": " << size << " bytes" << std::endl;
    }

    // Create and send malloc batch for all allocations
    std::vector<std::pair<int, message_slot_t>> malloc_batch;
    if (!BatchUtils::createAndSendMallocBatch(*writer, buffer_specs, malloc_batch)) {
        std::cerr << "Error: Failed during malloc batch send" << std::endl;
        return false;
    }

    // Wait for and validate malloc response batch
    std::vector<std::pair<int, message_slot_t>> malloc_response_batch;
    std::vector<std::pair<int, message_slot_t>> malloc_body_slots;
    if (!BatchUtils::waitForResponseBatch(*reader, malloc_batch, malloc_response_batch, malloc_body_slots, 50, 100, "malloc response")) {
        std::cerr << "Error: Malloc request batch timed out or invalid" << std::endl;
        return false;
    }

    // Extract allocated addresses and match them to purposes
    if (!extract_malloc_addresses_from_body_slots(malloc_body_slots, buffer_specs, out_allocated_buffers_map)) {
        std::cerr << "Error: Failed to extract malloc addresses from response batch" << std::endl;
        return false;
    }

    // Clear malloc response batch slots
    clear_response_slots(writer, malloc_response_batch); // FIXME TODO do we want this arbitrary clear here?

    std::cout << "\n✓ Successfully allocated " << out_allocated_buffers_map.size() << " buffers:" << std::endl;
    for (const auto& [purpose, buffer] : out_allocated_buffers_map) {
        std::cout << "  - " << purpose << ": 0x" << std::hex << buffer.address
                  << std::dec << " (" << buffer.size << " bytes)" << std::endl;
    }

    return true;
}

template<typename Writer, typename Reader>
bool free_buffers(Writer* writer, Reader* reader,
                 const std::unordered_map<std::string, AllocatedBuffer>& allocated_buffers_map) {
    if (allocated_buffers_map.empty()) {
        std::cout << "No buffers to free" << std::endl;
        return true;
    }

    std::cout << "\nFreeing " << allocated_buffers_map.size() << " buffers in batch:" << std::endl;
    for (const auto& [purpose, buffer] : allocated_buffers_map) {
        std::cout << "  - " << purpose << ": 0x" << std::hex << buffer.address << std::dec << std::endl;
    }

    // Create free commands for all buffers
    std::vector<message_slot_t> free_batch_body(allocated_buffers_map.size());
    size_t i = 0;
    for (const auto& [purpose, buffer] : allocated_buffers_map) {
        if (!MessageUtils::createFreeCmd(&free_batch_body[i], buffer.address)) {
            std::cerr << "Error: Failed to create free command for " << purpose << std::endl;
            return false;
        }
        ++i;
    }

    // Create and send free batch
    std::vector<std::pair<int, message_slot_t>> free_batch;
    if (!BatchUtils::create_command_batch(free_batch_body, 0, free_batch)) {
        std::cerr << "Error: Failed to create free batch" << std::endl;
        return false;
    }

    if (!BatchUtils::sendBatch(*writer, free_batch)) {
        std::cerr << "Error: Failed to send free batch" << std::endl;
        return false;
    }

    // Wait for free response batch
    std::vector<std::pair<int, message_slot_t>> free_response_batch;
    std::vector<std::pair<int, message_slot_t>> free_body_slots;
    if (!BatchUtils::waitForResponseBatch(*reader, free_batch, free_response_batch, free_body_slots, 50, 100, "free response")) {
        std::cerr << "Error: Free batch failed or timed out" << std::endl;
        return false;
    }

    // Verify all free operations succeeded
    std::vector<std::pair<int, free_rsp_t>> free_responses;
    if (!BatchUtils::extractPayloadsFromBatch(free_body_slots, MSG_RSP_FREE, free_responses)) {
        std::cerr << "Error: Failed to extract free response payloads" << std::endl;
        return false;
    }

    // Verify we have the expected number of free responses
    if (free_responses.size() != allocated_buffers_map.size()) {
        std::cerr << "Error: Expected " << allocated_buffers_map.size() << " free responses, got " << free_responses.size() << std::endl;
        return false;
    }

    int successful_frees = 0;
    for (const auto& [slot_index, free_rsp] : free_responses) {
        if (free_rsp.status == ERR_OK) {
            successful_frees++;
        } else {
            std::cerr << "Warning: Free failed in slot " << slot_index
                      << " with status: " << MessageUtils::getErrorCodeString(free_rsp.status) << std::endl;
        }
    }

    // Clear response slots
    clear_response_slots(writer, free_response_batch);

    if (successful_frees != static_cast<int>(allocated_buffers_map.size())) {
        std::cerr << "Warning: Not all memory was freed successfully ("
                  << successful_frees << "/" << allocated_buffers_map.size() << ")" << std::endl;
        return false;
    } else {
        std::cout << "✓ All " << successful_frees << " memory buffers freed successfully" << std::endl;
        return true;
    }
}

// Helper implementations
static bool extract_malloc_addresses_from_body_slots(const std::vector<std::pair<int, message_slot_t>> &body_slots,
                                                     const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                                                     std::unordered_map<std::string, AllocatedBuffer>& out_allocated_buffers_map) {
    // Extract all malloc responses with their slot indices
    std::vector<std::pair<int, malloc_rsp_t>> malloc_responses;
    if (!BatchUtils::extractPayloadsFromBatch(body_slots, MSG_RSP_MALLOC, malloc_responses)) {
        std::cerr << "Error: Failed to extract malloc response payloads" << std::endl;
        return false;
    }

    // Verify we have the expected number of malloc responses
    if (malloc_responses.size() != buffer_specs.size()) {
        std::cerr << "Error: Expected " << buffer_specs.size() << " malloc responses, got " << malloc_responses.size() << std::endl;
        return false;
    }

    // Match malloc responses to buffer specs (assume same order)
    for (size_t i = 0; i < buffer_specs.size(); ++i) {
        const auto& [expected_size, purpose] = buffer_specs[i];
        const auto& [slot_index, malloc_rsp] = malloc_responses[i];

        if (malloc_rsp.status != ERR_OK) {
            std::cerr << "Error: Malloc failed for " << purpose << " with status: "
                      << MessageUtils::getErrorCodeString(malloc_rsp.status) << std::endl;
            return false;
        }

        // Create allocated buffer entry and add to map
        out_allocated_buffers_map.emplace(purpose, AllocatedBuffer(malloc_rsp.address, expected_size, purpose));

        std::cout << "✓ Malloc successful for " << purpose << " (slot " << slot_index
                  << "), allocated address: 0x" << std::hex << malloc_rsp.address << std::dec
                  << " (" << expected_size << " bytes)" << std::endl;
    }

    return true;
}

template<typename Writer>
static void clear_response_slots(Writer* writer, const std::vector<std::pair<int, message_slot_t>> &response_batch) {
    for (const auto& [clear_slot_idx, _] : response_batch) {
        MailboxUtils::clearD2HSlot(*writer, clear_slot_idx);
    }
}

} // namespace BufferUtils