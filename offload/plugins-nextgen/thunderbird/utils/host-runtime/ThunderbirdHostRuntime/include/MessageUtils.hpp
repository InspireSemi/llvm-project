#pragma once

// include message payload definitions and types they depend on
// these functions are for serializing and deserializing message payloads
#include "error_codes.h"
#include "ivshmem_config.h"
#include "message_id.h"
#include "message_slot.h"
#include "device_types.h"
#include "message_payloads.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <vector>

namespace MessageUtils {

/**
 * @brief Get human-readable string for message ID
 */
const char* getMessageIdString(message_id_t msg_id);

/**
 * @brief Get human-readable string for error code
 */
const char* getErrorCodeString(error_code_t error);

/**
 * @brief Print malloc command payload
 */
void printMallocCmd(const malloc_cmd_t* payload);

/**
 * @brief Print malloc response payload
 */
void printMallocRsp(const malloc_rsp_t* payload);

/**
 * @brief Print free command payload
 */
void printFreeCmd(const free_cmd_t* payload);

/**
 * @brief Print free response payload
 */
void printFreeRsp(const free_rsp_t* payload);

/**
 * @brief Print launch command payload
 */
void printLaunchCmd(const launch_cmd_t* payload);

/**
 * @brief Print launch response payload
 */
void printLaunchRsp(const launch_rsp_t* payload);

/**
 * @brief Print ping payload
 */
void printPing(const ping_t* payload);

/**
 * @brief Print query device command payload
 */
void printQueryDeviceCmd(const query_device_cmd_t* payload);

/**
 * @brief Print query device response payload
 */
void printQueryDeviceRsp(const query_device_rsp_t* payload);

/**
 * @brief Print batch begin command payload
 */
void printBatchBeginCmd(const batch_begin_cmd_t* payload);

/**
 * @brief Print batch end command payload
 */
void printBatchEndCmd(const batch_end_cmd_t* payload);

/**
 * @brief Print batch begin response payload
 */
void printBatchBeginRsp(const batch_begin_rsp_t* payload);

/**
 * @brief Print batch end response payload
 */
void printBatchEndRsp(const batch_end_rsp_t* payload);

/**
 * @brief Print internal invalidate slots command payload
 */
void printInternalInvalidateSlots(const internal_invalidate_slots_cmd_t* payload);

/**
 * @brief Print memory status response payload
 */
void printMemoryStatusRsp(const memory_status_rsp_t* payload);

/**
 * @brief Print raw data payload when no specific formatter exists
 */
void printRawData(const uint8_t* data, uint32_t length);

/**
 * @brief Classify message by ID and return category
 */
enum class MessageCategory {
    COMMAND,
    RESPONSE,
    CONTROL,
    INTERNAL,
    VENDOR,
    UNKNOWN
};

MessageCategory classifyMessage(message_id_t msg_id);

/**
 * @brief Extract and print message payload based on message ID
 */
void printMessagePayload(const message_slot_t* slot);

/**
 * @brief Main function to classify and pretty-print a message slot
 */
void printMessageSlot(const message_slot_t* slot);

/**
 * @brief Initialize a message slot with basic fields
 */
void initMessageSlot(message_slot_t* slot, message_id_t msg_id, uint32_t payload_size);

/**
 * @brief Create malloc command message
 */
bool createMallocCmd(message_slot_t* slot, uint32_t size, uint32_t alignment = 8);

/**
 * @brief Create free command message
 */
bool createFreeCmd(message_slot_t* slot, uint64_t address);

/**
 * @brief Create launch command message
 */
bool createLaunchCmd(message_slot_t* slot, 
                    uint64_t kernel_address,
                    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                    uint32_t block_x, uint32_t block_y, uint32_t block_z,
                    uint64_t args_address);

/**
 * @brief Create query device command message
 */
bool createQueryDeviceCmd(message_slot_t* slot, uint32_t query_flags = 0xFFFFFFFF);

/**
 * @brief Create batch begin command message
 */
bool createBatchBeginCmd(message_slot_t* slot, const std::vector<uint8_t>& batch_slots);

/**
 * @brief Create batch end command message
 */
bool createBatchEndCmd(message_slot_t* slot, const std::vector<uint8_t>& batch_slots);

/**
 * @brief Create batch begin response message
 */
bool createBatchBeginRsp(message_slot_t* slot, const std::vector<uint8_t>& batch_slots);

/**
 * @brief Create batch end response message
 */
bool createBatchEndRsp(message_slot_t* slot, const std::vector<uint8_t>& batch_slots);

/**
 * @brief Create ping message
 */
bool createPing(message_slot_t* slot, uint32_t timestamp = 0);

/**
 * @brief Extract payload from message slot (type-safe)
 */
template<typename T>
inline bool extractPayload(const message_slot_t* slot, T* payload) {
    if (!slot || !payload || slot->length < sizeof(T)) {
        return false;
    }
    
    std::memcpy(payload, slot->data, sizeof(T));
    return true;
}

} // namespace MessageUtils