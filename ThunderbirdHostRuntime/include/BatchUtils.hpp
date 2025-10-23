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

// Public API function declarations for batch creation and validation

namespace BatchUtils {

// Function to get, given a vector of pairs of slot indices and message slots, whether there is 1 begin command and 1 end command and if they agree on what the slot indices should be, and those slot indices
// are nominally in the vector.
bool confirm_batch_integrity(const std::vector<std::pair<int, message_slot_t>>& slots, 
                           int& begin_slot, int& end_slot, bool& is_cmd, bool& is_rsp,
                           std::vector<std::pair<int, message_slot_t>>& body_slots);

/**
 * @brief Create a command batch with begin/end slots and validate it
 * @param body_messages Vector of message slots for the batch body
 * @param start_slot_index Starting slot index for the batch
 * @param batch_slots Output vector containing complete batch (begin + body + end)
 * @return True if batch was created successfully and passes validation
 */
bool create_command_batch(const std::vector<message_slot_t>& body_messages,
                         uint8_t start_slot_index,
                         std::vector<std::pair<int, message_slot_t>>& batch_slots);

/**
 * @brief Create a response batch with begin/end slots and validate it
 * @param body_messages Vector of message slots for the batch body
 * @param start_slot_index Starting slot index for the batch
 * @param batch_slots Output vector containing complete batch (begin + body + end)
 * @return True if batch was created successfully and passes validation
 */
bool create_response_batch(const std::vector<message_slot_t>& body_messages,
                          uint8_t start_slot_index,
                          std::vector<std::pair<int, message_slot_t>>& batch_slots);

// Test orchestration function for confirm_batch_integrity
bool run_batch_integrity_tests();

/**
 * @brief Helper class for building and managing message batches
 */
class Batch {
public:
    /**
     * @brief Type of batch to create
     */
    enum class Type {
        COMMAND,
        RESPONSE
    };

private:
    std::vector<std::pair<int, message_slot_t>> finalized_batch_;
    int begin_slot_ = 0;
    int end_slot_ = 0;
    int max_end_slot_ = 0;
    std::vector<message_slot_t> messages_;  // body messages
    Type type_ = Type::COMMAND;

public:
    /**
     * @brief Constructor for Batch helper
     * @param begin_slot Starting slot index (default 0)
     * @param end_slot Ending slot index (default 0)
     * @param max_end_slot Maximum allowed end slot (default 0)
     * @param type Type of batch (COMMAND or RESPONSE, default COMMAND)
     */
    Batch(int begin_slot = 0, int end_slot = 0, int max_end_slot = 0, Type type = Type::COMMAND);

    /**
     * @brief Append a body message to the batch
     * @param message The message slot to append
     */
    void append_body_message(const message_slot_t& message);

    /**
     * @brief Get intrusive access to the body messages vector
     * @return Reference to the body messages vector
     */
    std::vector<message_slot_t>& body_messages();

    /**
     * @brief Finalize the batch by validating bounds and creating the complete batch
     * @return True if batch fits within bounds and is valid
     */
    bool finalize();

    /**
     * @brief Get the finalized batch
     * @return Const reference to the finalized batch slots
     */
    const std::vector<std::pair<int, message_slot_t>>& finalized_batch() const;

    /**
     * @brief Get the begin slot
     * @return Begin slot index
     */
    int begin_slot() const;

    /**
     * @brief Get the end slot
     * @return End slot index
     */
    int end_slot() const;

    /**
     * @brief Get the max end slot
     * @return Maximum allowed end slot
     */
    int max_end_slot() const;
};

/**
 * @brief Send a batch of messages to the device
 * @tparam Writer Writer type for H2D communication
 * @param writer Writer instance
 * @param batch The batch to send (vector of {slot_index, message} pairs)
 * @param clear_d2h_slots If true, clear corresponding D2H slots before sending (default false)
 * @return True if all messages were sent successfully
 */
template<typename Writer>
bool sendBatch(Writer& writer, const std::vector<std::pair<int, message_slot_t>>& batch, bool clear_d2h_slots = false);

/**
 * @brief Extract all payloads of a specific type from validated batch body slots
 * @param body_slots The body slots from a validated batch (output of confirm_batch_integrity)
 * @param expected_msg_id The message ID to filter by
 * @param out_payloads Vector to store {slot_index, payload} pairs
 * @return True if all extractions succeeded
 */
template<typename T>
bool extractPayloadsFromBatch(const std::vector<std::pair<int, message_slot_t>>& body_slots,
                             message_id_t expected_msg_id,
                             std::vector<std::pair<int, T>>& out_payloads) {
    out_payloads.clear();
    for (const auto& [slot_index, slot] : body_slots) {
        if (slot.msg_id == expected_msg_id) {
            T payload;
            if (MessageUtils::extractPayload(&slot, &payload)) {
                out_payloads.emplace_back(slot_index, std::move(payload));
            } else {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Extract first valid response payload from batch with status validation
 * @param body_slots The body slots from a validated batch
 * @param expected_msg_id Response message ID to find
 * @param out_slot_index The slot index where the response was found
 * @param out_payload The extracted payload
 * @param expected_status Expected status code (default ERR_OK)
 * @return True if payload found and status matches
 */
template<typename T>
bool extractValidResponseFromBatch(const std::vector<std::pair<int, message_slot_t>>& body_slots,
                                  message_id_t expected_msg_id,
                                  int& out_slot_index,
                                  T& out_payload,
                                  error_code_t expected_status = ERR_OK) {
    for (const auto& [slot_index, slot] : body_slots) {
        if (slot.msg_id == expected_msg_id) {
            if (MessageUtils::extractPayload(&slot, &out_payload)) {
                // Check status for response types that have it
                if constexpr (std::is_same_v<T, malloc_rsp_t> ||
                             std::is_same_v<T, free_rsp_t> ||
                             std::is_same_v<T, launch_rsp_t>) {
                    if (out_payload.status == expected_status) {
                        out_slot_index = slot_index;
                        return true;
                    }
                } else {
                    // For types without status field, just return success
                    out_slot_index = slot_index;
                    return true;
                }
            }
        }
    }
    return false;
}

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
                              std::vector<std::pair<int, message_slot_t>>& out_batch);

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
                         int max_iterations = 50,
                         int poll_ms = 100,
                         const char* label = "response");

} // namespace BatchUtils

// Include template implementations
#include "BatchUtils.tpp"
