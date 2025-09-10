#ifndef THUNDERBIRD_MAILBOX_UTILS_HPP
#define THUNDERBIRD_MAILBOX_UTILS_HPP
#pragma once

// Functions for verifying things like identifying a batch of response messages, testing their completeness, and checking if they are the response to a command batch.
// functions for building a batch of commands and sending them to the mailbox.
// functions to watch the D2H mailbox for incoming messages and process them as batches.
// can ask for mailbox slot occupancy to be cleared if it has been confirmed a batch has been fully sent, received, processed and responded to.

#include "MailboxUtils.hpp"
#include "MessageUtils.hpp"
#include "message_slot.h"
#include "message_id.h"
#include "message_payloads.h"
#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace {
    // Helper function to extract slot information from batch payload
    bool extract_batch_slots(const message_slot_t& slot, std::set<uint8_t>& slots_set, uint32_t& slot_count) {
        if (slot.msg_id == MSG_CMD_BEGIN_BATCH || slot.msg_id == MSG_RSP_BEGIN_BATCH) {
            if (slot.length >= sizeof(batch_begin_cmd_t)) {
                const batch_begin_cmd_t* payload = reinterpret_cast<const batch_begin_cmd_t*>(slot.data);
                slot_count = payload->slot_count;
                for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
                    slots_set.insert(payload->batch_slots[i]);
                }
                return true;
            }
        } else if (slot.msg_id == MSG_CMD_END_BATCH || slot.msg_id == MSG_RSP_END_BATCH) {
            if (slot.length >= sizeof(batch_end_cmd_t)) {
                const batch_end_cmd_t* payload = reinterpret_cast<const batch_end_cmd_t*>(slot.data);
                slot_count = payload->slot_count;
                for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
                    slots_set.insert(payload->batch_slots[i]);
                }
                return true;
            }
        }
        return false;
    }

    // Helper function to validate batch consistency
    bool validate_batch_consistency(bool found_cmd_begin, bool found_cmd_end,
                                   bool found_rsp_begin, bool found_rsp_end,
                                   bool& is_cmd, bool& is_rsp) {
        if (found_cmd_begin && found_cmd_end && !found_rsp_begin && !found_rsp_end) {
            is_cmd = true;
            return true;
        } else if (found_rsp_begin && found_rsp_end && !found_cmd_begin && !found_cmd_end) {
            is_rsp = true;
            return true;
        }
        return false;
    }

    // Helper function to verify slot sets match
    bool verify_slot_sets_match(const std::set<uint8_t>& begin_slots,
                                const std::set<uint8_t>& end_slots,
                                uint32_t begin_count, uint32_t end_count) {
        return (begin_count == end_count) && (begin_slots == end_slots);
    }

    // Helper function to validate actual slots against advertised slots
    bool validate_actual_slots(const std::vector<std::pair<int, message_slot_t>>& slots,
                              const std::set<uint8_t>& advertised_slots,
                              uint32_t advertised_count) {
        // Check slot count
        if (advertised_count != slots.size()) {
            return false;
        }

        // Check that all actual slots are in advertised set
        std::set<uint8_t> actual_slots;
        for (const auto& [index, slot] : slots) {
            actual_slots.insert(static_cast<uint8_t>(index));
        }

        return advertised_slots == actual_slots;
    }
}

// Public API functions for batch creation and validation

// Function to get, given a vector of pairs of slot indices and message slots, whether there is 1 begin command and 1 end command and if they agree on what the slot indices should be, and those slot indices
// are nominally in the vector.
bool confirm_batch_integrity(const std::vector<std::pair<int, message_slot_t>>& slots,
                           int& begin_slot, int& end_slot, bool& is_cmd, bool& is_rsp,
                           std::vector<std::pair<int, message_slot_t>>& body_slots) {
    begin_slot = -1;
    end_slot = -1;
    is_cmd = false;
    is_rsp = false;
    body_slots.clear();

    bool found_cmd_begin = false;
    bool found_rsp_begin = false;
    bool found_cmd_end = false;
    bool found_rsp_end = false;

    std::set<uint8_t> begin_advertised_slots;
    std::set<uint8_t> end_advertised_slots;
    uint32_t begin_slot_count = 0;
    uint32_t end_slot_count = 0;

    for (const auto& [index, slot] : slots) {
        if (slot.msg_id == MSG_CMD_BEGIN_BATCH) {
            if (begin_slot != -1) return false; // More than one begin command found
            begin_slot = index;
            found_cmd_begin = true;
            if (!extract_batch_slots(slot, begin_advertised_slots, begin_slot_count)) {
                return false;
            }

        } else if (slot.msg_id == MSG_RSP_BEGIN_BATCH) {
            if (begin_slot != -1) return false; // More than one begin command found
            begin_slot = index;
            found_rsp_begin = true;
            if (!extract_batch_slots(slot, begin_advertised_slots, begin_slot_count)) {
                return false;
            }

        } else if (slot.msg_id == MSG_CMD_END_BATCH) {
            if (end_slot != -1) return false; // More than one end command found
            end_slot = index;
            found_cmd_end = true;
            if (!extract_batch_slots(slot, end_advertised_slots, end_slot_count)) {
                return false;
            }

        } else if (slot.msg_id == MSG_RSP_END_BATCH) {
            if (end_slot != -1) return false; // More than one end command found
            end_slot = index;
            found_rsp_end = true;
            if (!extract_batch_slots(slot, end_advertised_slots, end_slot_count)) {
                return false;
            }
        }
    }

    // Check if begin and end slots are found and properly ordered
    if (begin_slot == -1 || end_slot == -1 || begin_slot >= end_slot) {
        return false;
    }

    // Validate batch consistency (cmd vs rsp)
    if (!validate_batch_consistency(found_cmd_begin, found_cmd_end,
                                   found_rsp_begin, found_rsp_end, is_cmd, is_rsp)) {
        return false;
    }

    // Verify begin and end slot sets match
    if (!verify_slot_sets_match(begin_advertised_slots, end_advertised_slots,
                               begin_slot_count, end_slot_count)) {
        return false;
    }

    // Validate actual slots against advertised slots
    if (!validate_actual_slots(slots, begin_advertised_slots, begin_slot_count)) {
        return false;
    }

    // Collect body slots (excluding begin and end) in index order
    for (const auto& [index, slot] : slots) {
        if (index != begin_slot && index != end_slot) {
            body_slots.push_back({index, slot});
        }
    }

    // Sort body slots by index to ensure proper order
    std::sort(body_slots.begin(), body_slots.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    return true;
}

/**
 * @brief Create a command batch with begin/end slots and validate it
 * @param body_messages Vector of message slots for the batch body
 * @param start_slot_index Starting slot index for the batch
 * @param batch_slots Output vector containing complete batch (begin + body + end)
 * @return True if batch was created successfully and passes validation
 */
bool create_command_batch(const std::vector<message_slot_t>& body_messages,
                         uint8_t start_slot_index,
                         std::vector<std::pair<int, message_slot_t>>& batch_slots) {
    batch_slots.clear();

    if (body_messages.empty() || start_slot_index + body_messages.size() + 1 >= 256) {
        return false; // Invalid parameters or would overflow uint8_t
    }

    // Create slot index vector
    std::vector<uint8_t> slot_indices;
    for (size_t i = 0; i < body_messages.size() + 2; ++i) { // +2 for begin and end
        slot_indices.push_back(start_slot_index + static_cast<uint8_t>(i));
    }

    // Create begin slot using MessageUtils
    message_slot_t begin_slot;
    if (!MessageUtils::createBatchBeginCmd(&begin_slot, slot_indices)) {
        return false;
    }
    batch_slots.push_back({start_slot_index, begin_slot});

    // Add body slots
    for (size_t i = 0; i < body_messages.size(); ++i) {
        batch_slots.push_back({start_slot_index + 1 + static_cast<int>(i), body_messages[i]});
    }

    // Create end slot using MessageUtils
    message_slot_t end_slot;
    if (!MessageUtils::createBatchEndCmd(&end_slot, slot_indices)) {
        return false;
    }
    batch_slots.push_back({start_slot_index + 1 + static_cast<int>(body_messages.size()), end_slot});

    // Validate the created batch
    int begin_slot_idx, end_slot_idx;
    bool is_cmd, is_rsp;
    std::vector<std::pair<int, message_slot_t>> validated_body;

    return confirm_batch_integrity(batch_slots, begin_slot_idx, end_slot_idx, is_cmd, is_rsp, validated_body);
}

/**
 * @brief Create a response batch with begin/end slots and validate it
 * @param body_messages Vector of message slots for the batch body
 * @param start_slot_index Starting slot index for the batch
 * @param batch_slots Output vector containing complete batch (begin + body + end)
 * @return True if batch was created successfully and passes validation
 */
bool create_response_batch(const std::vector<message_slot_t>& body_messages,
                          uint8_t start_slot_index,
                          std::vector<std::pair<int, message_slot_t>>& batch_slots) {
    batch_slots.clear();

    if (body_messages.empty() || start_slot_index + body_messages.size() + 1 >= 256) {
        return false; // Invalid parameters or would overflow uint8_t
    }

    // Create slot index vector
    std::vector<uint8_t> slot_indices;
    for (size_t i = 0; i < body_messages.size() + 2; ++i) { // +2 for begin and end
        slot_indices.push_back(start_slot_index + static_cast<uint8_t>(i));
    }

    // Create begin slot using MessageUtils
    message_slot_t begin_slot;
    if (!MessageUtils::createBatchBeginRsp(&begin_slot, slot_indices)) {
        return false;
    }
    batch_slots.push_back({start_slot_index, begin_slot});

    // Add body slots
    for (size_t i = 0; i < body_messages.size(); ++i) {
        batch_slots.push_back({start_slot_index + 1 + static_cast<int>(i), body_messages[i]});
    }

    // Create end slot using MessageUtils
    message_slot_t end_slot;
    if (!MessageUtils::createBatchEndRsp(&end_slot, slot_indices)) {
        return false;
    }
    batch_slots.push_back({start_slot_index + 1 + static_cast<int>(body_messages.size()), end_slot});

    // Validate the created batch
    int begin_slot_idx, end_slot_idx;
    bool is_cmd, is_rsp;
    std::vector<std::pair<int, message_slot_t>> validated_body;

    return confirm_batch_integrity(batch_slots, begin_slot_idx, end_slot_idx, is_cmd, is_rsp, validated_body);
}

// Test orchestration function for confirm_batch_integrity
bool run_batch_integrity_tests() {
    std::cout << "Running BatchUtils tests..." << std::endl;
    int tests_passed = 0;
    int total_tests = 0;

    // Test 1: Valid command batch using public API
    {
        total_tests++;
        std::cout << "Testing command batch creation..." << std::endl;

        std::vector<message_slot_t> body_messages(3);
        MessageUtils::createMallocCmd(&body_messages[0], 1024);
        MessageUtils::createFreeCmd(&body_messages[1], 0x12345678);
        MessageUtils::createQueryDeviceCmd(&body_messages[2]);

        std::vector<std::pair<int, message_slot_t>> batch_slots;
        bool result = create_command_batch(body_messages, 10, batch_slots);

        if (result && batch_slots.size() == 5) { // begin + 3 body + end
            std::cout << "✓ Command batch creation test passed" << std::endl;
            tests_passed++;
        } else {
            std::cout << "✗ Command batch creation test failed" << std::endl;
        }
    }

    // Test 2: Valid response batch using public API
    {
        total_tests++;
        std::cout << "Testing response batch creation..." << std::endl;

        std::vector<message_slot_t> body_messages(1);
        MessageUtils::createPing(&body_messages[0], 12345);

        std::vector<std::pair<int, message_slot_t>> batch_slots;
        bool result = create_response_batch(body_messages, 20, batch_slots);

        if (result && batch_slots.size() == 3) { // begin + 1 body + end
            std::cout << "✓ Response batch creation test passed" << std::endl;
            tests_passed++;
        } else {
            std::cout << "✗ Response batch creation test failed" << std::endl;
        }
    }

    // Test 3: Empty body messages should fail
    {
        total_tests++;
        std::cout << "Testing empty batch creation..." << std::endl;

        std::vector<message_slot_t> body_messages;
        std::vector<std::pair<int, message_slot_t>> batch_slots;
        bool result = create_command_batch(body_messages, 0, batch_slots);

        if (!result) {
            std::cout << "✓ Empty batch creation test passed" << std::endl;
            tests_passed++;
        } else {
            std::cout << "✗ Empty batch creation test failed" << std::endl;
        }
    }

    std::cout << "\nTest Results: " << tests_passed << "/" << total_tests << " tests passed" << std::endl;

    if (tests_passed == total_tests) {
        std::cout << "✅ All tests passed!" << std::endl;
        return true;
    } else {
        std::cout << "❌ Some tests failed!" << std::endl;
        return false;
    }
}

bool tbird_resp_wait(std::vector<std::pair<int, message_slot_t>> &waiting_batch, std::unique_ptr<DataTransferEngineReadBase> &rd_channel, std::vector<std::pair<int, message_slot_t>> &body_slots, std::vector<std::pair<int, message_slot_t>> &response_batch){

        // Step 2: Wait for and validate malloc response batch
        bool response_received = false;

        for (int timeout = 0; timeout < 50 && !response_received; ++timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            response_batch.clear();
            for (const auto& [slot_index, _] : waiting_batch) {
                message_slot_t slot;
                if (MailboxUtils::readD2HMessage(*rd_channel, slot_index, &slot)) {
                    if (slot.msg_id != MSG_INVALID) {
                        response_batch.push_back({slot_index, slot});
                        std::cout << "Peeked response in slot " << slot_index
                                 << " with message ID " << MessageUtils::getMessageIdString(slot.msg_id) << std::endl;
                    }
                }
            }

            int begin_slot, end_slot;
            bool is_cmd, is_rsp;

            if (confirm_batch_integrity(response_batch, begin_slot, end_slot, is_cmd, is_rsp, body_slots)) {
                if (is_rsp && !is_cmd) {
                    std::cout << "✓ Valid malloc response batch received!" << std::endl;

                    std::cout << "\n=== MALLOC RESPONSE BATCH ===" << std::endl;
                    for (const auto& [slot_index, slot] : response_batch) {
                        std::cout << "\n--- Response Slot " << slot_index << " ---" << std::endl;
                        MessageUtils::printMessageSlot(&slot);
                    }
                    std::cout << "============================\n" << std::endl;

                    response_received = true;
                    break;
                }
            }

            if (timeout % 10 == 0) {
                std::cout << "Timeout " << timeout << ": Still waiting for malloc response..." << std::endl;
            }
        }

        if (!response_received) {
            std::cerr << "Error: Malloc request timed out" << std::endl;
            return false;
        }
        return true;
}

#endif
