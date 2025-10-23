/**
 * @file BatchUtils.tpp
 * @brief Template implementations for BatchUtils that depend on runtime components
 */

#pragma once

#include "BatchUtils.hpp"
#include "MailboxUtils.hpp"
#include "MessageUtils.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace BatchUtils {

/**
 * @brief Create and send a malloc batch for multiple buffer allocations
 * @tparam Writer Writer type for H2D communication
 * @param writer Writer instance
 * @param buffer_specs Vector of {size, purpose} pairs for allocations
 * @param out_batch The created batch slots
 * @return True on success
 */
template<typename Writer>
bool createAndSendMallocBatch(Writer& writer,
                              const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                              std::vector<std::pair<int, message_slot_t>>& out_batch) {
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

    if (!BatchUtils::create_command_batch(batch_body, 0, out_batch)) {
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

    // Send the batch
    if (!BatchUtils::sendBatch(writer, out_batch, true)) {
        std::cerr << "Error: Failed to send malloc batch" << std::endl;
        return false;
    }

    return true;
}

/**
 * @brief Wait for a response batch and validate it
 * @tparam Reader Reader type for D2H communication
 * @param reader Reader instance
 * @param sent_batch The batch that was sent
 * @param out_response_batch Received response batch
 * @param out_body_slots Extracted body slots from valid batch
 * @param max_iterations Maximum polling iterations
 * @param poll_ms Polling interval in milliseconds
 * @param label Descriptive label for logging
 * @return True if valid response batch received
 */
template<typename Reader>
bool waitForResponseBatch(Reader& reader,
                         const std::vector<std::pair<int, message_slot_t>>& sent_batch,
                         std::vector<std::pair<int, message_slot_t>>& out_response_batch,
                         std::vector<std::pair<int, message_slot_t>>& out_body_slots,
                         int max_iterations,
                         int poll_ms,
                         const char* label) {
    out_response_batch.clear();
    out_body_slots.clear();

    for (int iter = 0; iter < max_iterations; ++iter) {
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));

        out_response_batch.clear();
        for (const auto& [slot_index, _] : sent_batch) {
            message_slot_t slot;
            if (MailboxUtils::readD2HMessage(reader, slot_index, &slot)) {
                if (slot.msg_id != MSG_INVALID) {
                    out_response_batch.push_back({slot_index, slot});
                    std::cout << "Peeked " << label << " in slot " << slot_index
                              << " with message ID " << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
                }
            }
        }

        int begin_slot, end_slot;
        bool is_cmd, is_rsp;
        if (BatchUtils::confirm_batch_integrity(out_response_batch, begin_slot, end_slot, is_cmd, is_rsp, out_body_slots)) {
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

/**
 * @brief Send a batch of messages to the device
 * @tparam Writer Writer type for H2D communication
 * @param writer Writer instance
 * @param batch The batch to send (vector of {slot_index, message} pairs)
 * @param clear_d2h_slots If true, clear corresponding D2H slots before sending (default false)
 * @return True if all messages were sent successfully
 */
template<typename Writer>
bool sendBatch(Writer& writer, const std::vector<std::pair<int, message_slot_t>>& batch, bool clear_d2h_slots) {
    // Optionally clear D2H slots first
    if (clear_d2h_slots) {
        std::cout << "Clearing D2H slots before sending batch..." << std::endl;
        for (const auto& [slot_index, _] : batch) {
            MailboxUtils::clearD2HSlot(writer, slot_index);
        }
    }

    // Send all messages in the batch
    for (const auto& [slot_index, slot] : batch) {
        std::cout << "Writing slot " << slot_index << " with message ID "
                  << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
        if (!MailboxUtils::writeH2DMessage(writer, slot_index, &slot)) {
            std::cerr << "Error: Failed to write message to slot " << slot_index << std::endl;
            return false;
        }
    }
    return true;
}

} // namespace BatchUtils