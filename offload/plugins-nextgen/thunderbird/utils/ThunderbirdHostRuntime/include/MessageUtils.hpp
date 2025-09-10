#ifndef THUNDERBIRD_MESSAGE_UTILS_HPP
#define THUNDERBIRD_MESSAGE_UTILS_HPP
#pragma once

// include message payload definitions and types they depend on
// these functions are for serializing and deserializing message payloads
#include "error_codes.h"
#include "ivshmem_config.h"
#include "message_id.h"
#include "message_slot.h"
#include "device_types.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <vector>

namespace MessageUtils {

/**
 * @brief Get human-readable string for message ID
 */
inline const char* getMessageIdString(message_id_t msg_id) {
    switch (msg_id) {
        case MSG_INVALID: return "MSG_INVALID";
        case MSG_PING: return "MSG_PING";
        case MSG_PONG: return "MSG_PONG";
        case MSG_DATA: return "MSG_DATA";
        case MSG_CTRL: return "MSG_CTRL";
        case MSG_CMD_MALLOC: return "MSG_CMD_MALLOC";
        case MSG_CMD_FREE: return "MSG_CMD_FREE";
        case MSG_CMD_LAUNCH: return "MSG_CMD_LAUNCH";
        case MSG_CMD_QUERY_DEVICE: return "MSG_CMD_QUERY_DEVICE";
        case MSG_CMD_BEGIN_BATCH: return "MSG_CMD_BEGIN_BATCH";
        case MSG_CMD_END_BATCH: return "MSG_CMD_END_BATCH";
        case MSG_CMD_TRANSFER_BEGIN: return "MSG_CMD_TRANSFER_BEGIN";
        case MSG_CMD_TRANSFER_FINISHED: return "MSG_CMD_TRANSFER_FINISHED";
        case MSG_RSP_MALLOC: return "MSG_RSP_MALLOC";
        case MSG_RSP_FREE: return "MSG_RSP_FREE";
        case MSG_RSP_LAUNCH: return "MSG_RSP_LAUNCH";
        case MSG_RSP_QUERY_DEVICE: return "MSG_RSP_QUERY_DEVICE";
        case MSG_RSP_BEGIN_BATCH: return "MSG_RSP_BEGIN_BATCH";
        case MSG_RSP_END_BATCH: return "MSG_RSP_END_BATCH";
        case MSG_RSP_TRANSFER_BEGIN: return "MSG_RSP_TRANSFER_BEGIN";
        case MSG_RSP_TRANSFER_FINISHED: return "MSG_RSP_TRANSFER_FINISHED";
        case MSG_INTERNAL_INVALIDATE_SLOTS: return "MSG_INTERNAL_INVALIDATE_SLOTS";
        default:
            if (msg_id >= MSG_VENDOR_BASE) {
                return "MSG_VENDOR_*";
            }
            return "MSG_UNKNOWN";
    }
}

/**
 * @brief Get human-readable string for error code
 */
inline const char* getErrorCodeString(error_code_t error) {
    switch (error) {
        case ERR_OK: return "ERR_OK";
        case ERR_INVALID_PARAM: return "ERR_INVALID_PARAM";
        case ERR_NO_MEMORY: return "ERR_NO_MEMORY";
        case ERR_TIMEOUT: return "ERR_TIMEOUT";
        case ERR_BUSY: return "ERR_BUSY";
        case ERR_NOT_FOUND: return "ERR_NOT_FOUND";
        case ERR_PERMISSION: return "ERR_PERMISSION";
        case ERR_IO: return "ERR_IO";
        case ERR_BEGIN_RECEIVED_BUT_CURRENT_BATCH_STILL_PROCESSING:
            return "ERR_BEGIN_RECEIVED_BUT_CURRENT_BATCH_STILL_PROCESSING";
        case ERR_END_RECEIVED_BUT_NO_BATCH_TO_END:
            return "ERR_END_RECEIVED_BUT_NO_BATCH_TO_END";
        case ERR_INVALID_BATCH_STATE: return "ERR_INVALID_BATCH_STATE";
        case ERR_END_CMD_MISMATCHES_BEGIN: return "ERR_END_CMD_MISMATCHES_BEGIN";
        case ERR_BODY_SLOT_INDEX_OUT_OF_RANGE: return "ERR_BODY_SLOT_INDEX_OUT_OF_RANGE";
        case ERR_BODY_SLOT_ALREADY_RECEIVED: return "ERR_BODY_SLOT_ALREADY_RECEIVED";
        case ERR_UNKNOWN: return "ERR_UNKNOWN";
        default: return "ERR_UNRECOGNIZED";
    }
}

/**
 * @brief Print malloc command payload
 */
inline void printMallocCmd(const malloc_cmd_t* payload) {
    std::cout << "  Size: " << payload->size << " bytes\n";
    std::cout << "  Alignment: " << payload->alignment << " bytes\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print malloc response payload
 */
inline void printMallocRsp(const malloc_rsp_t* payload) {
    std::cout << "  Status: " << getErrorCodeString(payload->status) << " (" << payload->status << ")\n";
    std::cout << "  Address: 0x" << std::hex << payload->address << std::dec << "\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print free command payload
 */
inline void printFreeCmd(const free_cmd_t* payload) {
    std::cout << "  Address: 0x" << std::hex << payload->address << std::dec << "\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print free response payload
 */
inline void printFreeRsp(const free_rsp_t* payload) {
    std::cout << "  Status: " << getErrorCodeString(payload->status) << " (" << payload->status << ")\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print launch command payload
 */
inline void printLaunchCmd(const launch_cmd_t* payload) {
    std::cout << "  Kernel Address: 0x" << std::hex << payload->kernel_address << std::dec << "\n";
    std::cout << "  Grid: (" << payload->grid_x << ", " << payload->grid_y << ", " << payload->grid_z << ")\n";
    std::cout << "  Block: (" << payload->block_x << ", " << payload->block_y << ", " << payload->block_z << ")\n";
    std::cout << "  Shared Memory Size: " << payload->shared_mem_size << " bytes\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print launch response payload
 */
inline void printLaunchRsp(const launch_rsp_t* payload) {
    std::cout << "  Status: " << getErrorCodeString(payload->status) << " (" << payload->status << ")\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print ping payload
 */
inline void printPing(const ping_t* payload) {
    std::cout << "  Timestamp: " << payload->timestamp << "\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print query device command payload
 */
inline void printQueryDeviceCmd(const query_device_cmd_t* payload) {
    std::cout << "  Query Flags: 0x" << std::hex << payload->query_flags << std::dec << "\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print query device response payload
 */
inline void printQueryDeviceRsp(const query_device_rsp_t* payload) {
    std::cout << "  Status: " << getErrorCodeString(payload->status) << " (" << payload->status << ")\n";
    std::cout << "  Device Info:\n";
    std::cout << "    Compute Capability: " << payload->device_info.compute_capability_major
              << "." << payload->device_info.compute_capability_minor << "\n";
    std::cout << "    Max Threads Per Block: " << payload->device_info.max_threads_per_block << "\n";
    std::cout << "    Max Blocks Per Multiprocessor: " << payload->device_info.max_blocks_per_multiprocessor << "\n";
    std::cout << "    Total Global Memory: " << payload->device_info.total_global_memory << " bytes\n";
    std::cout << "    Shared Memory Per Block: " << payload->device_info.shared_memory_per_block << " bytes\n";
    std::cout << "    Registers Per Block: " << payload->device_info.registers_per_block << "\n";
    std::cout << "    Warp Size: " << payload->device_info.warp_size << "\n";
}

/**
 * @brief Print batch begin command payload
 */
inline void printBatchBeginCmd(const batch_begin_cmd_t* payload) {
    std::cout << "  Slot Count: " << payload->slot_count << "\n";
    std::cout << "  Batch Slots: [";
    for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << static_cast<int>(payload->batch_slots[i]);
    }
    std::cout << "]\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print batch end command payload
 */
inline void printBatchEndCmd(const batch_end_cmd_t* payload) {
    std::cout << "  Slot Count: " << payload->slot_count << "\n";
    std::cout << "  Batch Slots: [";
    for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << static_cast<int>(payload->batch_slots[i]);
    }
    std::cout << "]\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print batch begin response payload
 */
inline void printBatchBeginRsp(const batch_begin_rsp_t* payload) {
    std::cout << "  Slot Count: " << payload->slot_count << "\n";
    std::cout << "  Batch Slots: [";
    for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << static_cast<int>(payload->batch_slots[i]);
    }
    std::cout << "]\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print batch end response payload
 */
inline void printBatchEndRsp(const batch_end_rsp_t* payload) {
    std::cout << "  Slot Count: " << payload->slot_count << "\n";
    std::cout << "  Batch Slots: [";
    for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << static_cast<int>(payload->batch_slots[i]);
    }
    std::cout << "]\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print internal invalidate slots command payload
 */
inline void printInternalInvalidateSlots(const internal_invalidate_slots_cmd_t* payload) {
    std::cout << "  Slot Count: " << payload->slot_count << "\n";
    std::cout << "  Slots: [";
    for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << static_cast<int>(payload->slots[i]);
    }
    std::cout << "]\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print memory status response payload
 */
inline void printMemoryStatusRsp(const memory_status_rsp_t* payload) {
    std::cout << "  Status: " << getErrorCodeString(payload->status) << " (" << payload->status << ")\n";
    std::cout << "  Total Memory: " << payload->total_memory << " bytes\n";
    std::cout << "  Free Memory: " << payload->free_memory << " bytes\n";
    std::cout << "  Reserved: 0x" << std::hex << payload->reserved << std::dec << "\n";
}

/**
 * @brief Print raw data payload when no specific formatter exists
 */
inline void printRawData(const uint8_t* data, uint32_t length) {
    const uint32_t max_display = std::min(length, static_cast<uint32_t>(32));
    std::cout << "  Raw Data (" << length << " bytes): ";

    for (uint32_t i = 0; i < max_display; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
        if (i < max_display - 1) std::cout << " ";
    }

    if (length > max_display) {
        std::cout << " ...";
    }
    std::cout << std::dec << "\n";
}

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

inline MessageCategory classifyMessage(message_id_t msg_id) {
    if (msg_id >= MSG_CMD_MALLOC && msg_id <= MSG_CMD_TRANSFER_FINISHED) {
        return MessageCategory::COMMAND;
    }
    if (msg_id >= MSG_RSP_MALLOC && msg_id <= MSG_RSP_TRANSFER_FINISHED) {
        return MessageCategory::RESPONSE;
    }
    if (msg_id == MSG_PING || msg_id == MSG_PONG || msg_id == MSG_DATA || msg_id == MSG_CTRL) {
        return MessageCategory::CONTROL;
    }
    if (msg_id == MSG_INTERNAL_INVALIDATE_SLOTS) {
        return MessageCategory::INTERNAL;
    }
    if (msg_id >= MSG_VENDOR_BASE) {
        return MessageCategory::VENDOR;
    }
    return MessageCategory::UNKNOWN;
}

/**
 * @brief Extract and print message payload based on message ID
 */
inline void printMessagePayload(const message_slot_t* slot) {
    if (!slot) {
        std::cout << "  ERROR: null message slot\n";
        return;
    }

    // Validate length doesn't exceed available data
    if (slot->length > MESSAGE_SLOT_DATA_SIZE) {
        std::cout << "  ERROR: payload length (" << slot->length
                  << ") exceeds maximum (" << MESSAGE_SLOT_DATA_SIZE << ")\n";
        return;
    }

    const uint8_t* payload_data = slot->data;

    switch (slot->msg_id) {
        case MSG_CMD_MALLOC:
            if (slot->length >= sizeof(malloc_cmd_t)) {
                printMallocCmd(reinterpret_cast<const malloc_cmd_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for malloc_cmd_t\n";
            }
            break;

        case MSG_RSP_MALLOC:
            if (slot->length >= sizeof(malloc_rsp_t)) {
                printMallocRsp(reinterpret_cast<const malloc_rsp_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for malloc_rsp_t\n";
            }
            break;

        case MSG_CMD_FREE:
            if (slot->length >= sizeof(free_cmd_t)) {
                printFreeCmd(reinterpret_cast<const free_cmd_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for free_cmd_t\n";
            }
            break;

        case MSG_RSP_FREE:
            if (slot->length >= sizeof(free_rsp_t)) {
                printFreeRsp(reinterpret_cast<const free_rsp_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for free_rsp_t\n";
            }
            break;

        case MSG_CMD_LAUNCH:
            if (slot->length >= sizeof(launch_cmd_t)) {
                printLaunchCmd(reinterpret_cast<const launch_cmd_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for launch_cmd_t\n";
            }
            break;

        case MSG_RSP_LAUNCH:
            if (slot->length >= sizeof(launch_rsp_t)) {
                printLaunchRsp(reinterpret_cast<const launch_rsp_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for launch_rsp_t\n";
            }
            break;

        case MSG_PING:
        case MSG_PONG:
            if (slot->length >= sizeof(ping_t)) {
                printPing(reinterpret_cast<const ping_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for ping_t\n";
            }
            break;

        case MSG_CMD_QUERY_DEVICE:
            if (slot->length >= sizeof(query_device_cmd_t)) {
                printQueryDeviceCmd(reinterpret_cast<const query_device_cmd_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for query_device_cmd_t\n";
            }
            break;

        case MSG_RSP_QUERY_DEVICE:
            if (slot->length >= sizeof(query_device_rsp_t)) {
                printQueryDeviceRsp(reinterpret_cast<const query_device_rsp_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for query_device_rsp_t\n";
            }
            break;

        case MSG_CMD_BEGIN_BATCH:
            if (slot->length >= sizeof(batch_begin_cmd_t)) {
                printBatchBeginCmd(reinterpret_cast<const batch_begin_cmd_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for batch_begin_cmd_t\n";
            }
            break;

        case MSG_CMD_END_BATCH:
            if (slot->length >= sizeof(batch_end_cmd_t)) {
                printBatchEndCmd(reinterpret_cast<const batch_end_cmd_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for batch_end_cmd_t\n";
            }
            break;

        case MSG_RSP_BEGIN_BATCH:
            if (slot->length >= sizeof(batch_begin_rsp_t)) {
                printBatchBeginRsp(reinterpret_cast<const batch_begin_rsp_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for batch_begin_rsp_t\n";
            }
            break;

        case MSG_RSP_END_BATCH:
            if (slot->length >= sizeof(batch_end_rsp_t)) {
                printBatchEndRsp(reinterpret_cast<const batch_end_rsp_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for batch_end_rsp_t\n";
            }
            break;

        case MSG_INTERNAL_INVALIDATE_SLOTS:
            if (slot->length >= sizeof(internal_invalidate_slots_cmd_t)) {
                printInternalInvalidateSlots(reinterpret_cast<const internal_invalidate_slots_cmd_t*>(payload_data));
            } else {
                std::cout << "  ERROR: insufficient data for internal_invalidate_slots_cmd_t\n";
            }
            break;

        case MSG_CMD_TRANSFER_BEGIN:
        case MSG_CMD_TRANSFER_FINISHED:
        case MSG_RSP_TRANSFER_BEGIN:
        case MSG_RSP_TRANSFER_FINISHED:
        case MSG_DATA:
        case MSG_CTRL:
        default:
            // For unhandled message types, print raw data
            printRawData(payload_data, slot->length);
            break;
    }
}

/**
 * @brief Main function to classify and pretty-print a message slot
 */
inline void printMessageSlot(const message_slot_t* slot) {
    if (!slot) {
        std::cout << "ERROR: null message slot\n";
        return;
    }

    MessageCategory category = classifyMessage(slot->msg_id);
    const char* category_str = "";

    switch (category) {
        case MessageCategory::COMMAND: category_str = "COMMAND"; break;
        case MessageCategory::RESPONSE: category_str = "RESPONSE"; break;
        case MessageCategory::CONTROL: category_str = "CONTROL"; break;
        case MessageCategory::INTERNAL: category_str = "INTERNAL"; break;
        case MessageCategory::VENDOR: category_str = "VENDOR"; break;
        case MessageCategory::UNKNOWN: category_str = "UNKNOWN"; break;
    }

    std::cout << "=== MESSAGE SLOT ===\n";
    std::cout << "Message ID: " << getMessageIdString(slot->msg_id)
              << " (" << slot->msg_id << ")\n";
    std::cout << "Category: " << category_str << "\n";
    std::cout << "Length: " << slot->length << " bytes\n";
    std::cout << "Checksum: 0x" << std::hex << slot->checksum << std::dec << "\n";
    std::cout << "Payload:\n";

    printMessagePayload(slot);

    std::cout << "==================\n";
}

/**
 * @brief Initialize a message slot with basic fields
 */
inline void initMessageSlot(message_slot_t* slot, message_id_t msg_id, uint32_t payload_size) {
    if (!slot) return;

    std::memset(slot, 0, sizeof(message_slot_t));
    slot->msg_id = msg_id;
    slot->length = payload_size;
}

/**
 * @brief Create malloc command message
 */
inline bool createMallocCmd(message_slot_t* slot, uint32_t size, uint32_t alignment = 8) {
    if (!slot || size == 0) return false;

    malloc_cmd_t payload = {};
    payload.size = size;
    payload.alignment = alignment;
    payload.reserved = 0;

    initMessageSlot(slot, MSG_CMD_MALLOC, sizeof(malloc_cmd_t));
    std::memcpy(slot->data, &payload, sizeof(malloc_cmd_t));

    return true;
}

/**
 * @brief Create free command message
 */
inline bool createFreeCmd(message_slot_t* slot, uint64_t address) {
    if (!slot || address == 0) return false;

    free_cmd_t payload = {};
    payload.address = address;
    payload.reserved = 0;

    initMessageSlot(slot, MSG_CMD_FREE, sizeof(free_cmd_t));
    std::memcpy(slot->data, &payload, sizeof(free_cmd_t));

    return true;
}

/**
 * @brief Create launch command message
 */
inline bool createLaunchCmd(message_slot_t* slot,
                           uint64_t kernel_address,
                           uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                           uint32_t block_x, uint32_t block_y, uint32_t block_z,
                           uint32_t shared_mem_size = 0) {
    if (!slot || kernel_address == 0) return false;

    launch_cmd_t payload = {};
    payload.kernel_address = kernel_address;
    payload.grid_x = grid_x;
    payload.grid_y = grid_y;
    payload.grid_z = grid_z;
    payload.block_x = block_x;
    payload.block_y = block_y;
    payload.block_z = block_z;
    payload.shared_mem_size = shared_mem_size;
    payload.reserved = 0;

    initMessageSlot(slot, MSG_CMD_LAUNCH, sizeof(launch_cmd_t));
    std::memcpy(slot->data, &payload, sizeof(launch_cmd_t));

    return true;
}

/**
 * @brief Create query device command message
 */
inline bool createQueryDeviceCmd(message_slot_t* slot, uint32_t query_flags = 0xFFFFFFFF) {
    if (!slot) return false;

    query_device_cmd_t payload = {};
    payload.query_flags = query_flags;
    payload.reserved = 0;

    initMessageSlot(slot, MSG_CMD_QUERY_DEVICE, sizeof(query_device_cmd_t));
    std::memcpy(slot->data, &payload, sizeof(query_device_cmd_t));

    return true;
}

/**
 * @brief Create batch begin command message
 */
inline bool createBatchBeginCmd(message_slot_t* slot, const std::vector<uint8_t>& batch_slots) {
    if (!slot || batch_slots.empty() || batch_slots.size() > MAX_BATCH_SLOTS) return false;

    batch_begin_cmd_t payload = {};
    payload.slot_count = static_cast<uint32_t>(batch_slots.size());
    std::memcpy(payload.batch_slots, batch_slots.data(), batch_slots.size());
    payload.reserved = 0;

    initMessageSlot(slot, MSG_CMD_BEGIN_BATCH, sizeof(batch_begin_cmd_t));
    std::memcpy(slot->data, &payload, sizeof(batch_begin_cmd_t));

    return true;
}

/**
 * @brief Create batch end command message
 */
inline bool createBatchEndCmd(message_slot_t* slot, const std::vector<uint8_t>& batch_slots) {
    if (!slot || batch_slots.empty() || batch_slots.size() > MAX_BATCH_SLOTS) return false;

    batch_end_cmd_t payload = {};
    payload.slot_count = static_cast<uint32_t>(batch_slots.size());
    std::memcpy(payload.batch_slots, batch_slots.data(), batch_slots.size());
    payload.reserved = 0;

    initMessageSlot(slot, MSG_CMD_END_BATCH, sizeof(batch_end_cmd_t));
    std::memcpy(slot->data, &payload, sizeof(batch_end_cmd_t));

    return true;
}

/**
 * @brief Create batch begin response message
 */
inline bool createBatchBeginRsp(message_slot_t* slot, const std::vector<uint8_t>& batch_slots) {
    if (!slot || batch_slots.empty() || batch_slots.size() > MAX_BATCH_SLOTS) return false;

    batch_begin_rsp_t payload = {};
    payload.slot_count = static_cast<uint32_t>(batch_slots.size());
    std::memcpy(payload.batch_slots, batch_slots.data(), batch_slots.size());
    payload.reserved = 0;

    initMessageSlot(slot, MSG_RSP_BEGIN_BATCH, sizeof(batch_begin_rsp_t));
    std::memcpy(slot->data, &payload, sizeof(batch_begin_rsp_t));

    return true;
}

/**
 * @brief Create batch end response message
 */
inline bool createBatchEndRsp(message_slot_t* slot, const std::vector<uint8_t>& batch_slots) {
    if (!slot || batch_slots.empty() || batch_slots.size() > MAX_BATCH_SLOTS) return false;

    batch_end_rsp_t payload = {};
    payload.slot_count = static_cast<uint32_t>(batch_slots.size());
    std::memcpy(payload.batch_slots, batch_slots.data(), batch_slots.size());
    payload.reserved = 0;

    initMessageSlot(slot, MSG_RSP_END_BATCH, sizeof(batch_end_rsp_t));
    std::memcpy(slot->data, &payload, sizeof(batch_end_rsp_t));

    return true;
}

/**
 * @brief Create ping message
 */
inline bool createPing(message_slot_t* slot, uint32_t timestamp = 0) {
    if (!slot) return false;

    ping_t payload = {};
    payload.timestamp = timestamp;
    payload.reserved = 0;

    initMessageSlot(slot, MSG_PING, sizeof(ping_t));
    std::memcpy(slot->data, &payload, sizeof(ping_t));

    return true;
}

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
#endif
