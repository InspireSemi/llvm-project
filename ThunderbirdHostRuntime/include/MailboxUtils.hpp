#pragma once

// Includes definitions for message slots, the mailbox type and some offsets.
#include "ivshmem_config.h"
#include "ivshmem_abi.h"
#include "message_slot.h"
#include "mailbox.h"
#include "message_id.h"
#include "error_codes.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace MailboxUtils {

/**
 * @brief Write a complete message to H2D mailbox slot
 */
template<typename Writer>
inline bool writeH2DMessage(Writer& writer, uint32_t slot_index, const message_slot_t* slot) {
    if (slot_index >= MAILBOX_SLOT_COUNT || !slot) {
        return false;
    }
    
    // Calculate slot offset and write entire slot in one operation
    uint32_t slot_offset = MBOX_H2D_SLOT_OFFSET(slot_index);
    if (writer.transfer(slot_offset, slot, sizeof(message_slot_t)) != sizeof(message_slot_t)) {
        return false;
    }
    
    // Mark slot as occupied
    uint8_t occupied = 1;
    return writer.transfer(MBOX_H2D_SLOTS_OCCUPIED_OFFSET + slot_index, &occupied, sizeof(uint8_t)) == sizeof(uint8_t);
}

/**
 * @brief Read a complete message from D2H mailbox slot
 */
template<typename Reader>
inline bool readD2HMessage(Reader& reader, uint32_t slot_index, message_slot_t* slot) {
    if (slot_index >= MAILBOX_SLOT_COUNT || !slot) {
        return false;
    }
    
    // Check if slot is occupied
    uint8_t occupied;
    if (reader.transfer(MBOX_D2H_SLOTS_OCCUPIED_OFFSET + slot_index, &occupied, sizeof(uint8_t)) != sizeof(uint8_t) || !occupied) {
        return false;
    }
    
    // Read entire slot in one operation
    uint32_t slot_offset = MBOX_D2H_SLOT_OFFSET(slot_index);
    return reader.transfer(slot_offset, slot, sizeof(message_slot_t)) == sizeof(message_slot_t);
}

/**
 * @brief Clear an H2D slot (mark as unoccupied) - host clears after sending
 */
template<typename Writer>
inline bool clearH2DSlot(Writer& writer, uint32_t slot_index) {
    if (slot_index >= MAILBOX_SLOT_COUNT) {
        return false;
    }
    
    uint8_t occupied = 0;
    return writer.transfer(MBOX_H2D_SLOTS_OCCUPIED_OFFSET + slot_index, &occupied, sizeof(uint8_t)) == sizeof(uint8_t);
}

/**
 * @brief Clear a D2H slot (mark as unoccupied) - host clears after reading
 */
template<typename Writer>
inline bool clearD2HSlot(Writer& writer, uint32_t slot_index) {
    if (slot_index >= MAILBOX_SLOT_COUNT) {
        return false;
    }
    
    uint8_t occupied = 0;
    return writer.transfer(MBOX_D2H_SLOTS_OCCUPIED_OFFSET + slot_index, &occupied, sizeof(uint8_t)) == sizeof(uint8_t);
}

/**
 * @brief Bulk read multiple slots from D2H mailbox
 */
template<typename Reader>
inline bool readD2HSlots(Reader& reader, uint32_t start_slot, uint32_t count, message_slot_t* slots, std::vector<uint8_t>* occupied_map = nullptr) {
    if (!slots || start_slot + count > MAILBOX_SLOT_COUNT) {
        return false;
    }
    
    // Read occupied flags for all slots first
    std::vector<uint8_t> occupied_flags(count);
    if (reader.transfer(MBOX_D2H_SLOTS_OCCUPIED_OFFSET + start_slot, occupied_flags.data(), count) != count) {
        return false;
    }
    
    // Copy occupied flags to output map if requested
    if (occupied_map) {
        occupied_map->resize(count);
        std::memcpy(occupied_map->data(), occupied_flags.data(), count);
    }
    
    // Read all slots in one operation if they're contiguous
    uint32_t start_offset = MBOX_D2H_SLOT_OFFSET(start_slot);
    uint32_t total_size = count * sizeof(message_slot_t);
    
    if (reader.transfer(start_offset, slots, total_size) != total_size) {
        return false;
    }
    
    // Mark unoccupied slots as invalid
    for (uint32_t i = 0; i < count; ++i) {
        if (!occupied_flags[i]) {
            slots[i].msg_id = MSG_INVALID;
            slots[i].length = 0;
        }
    }
    
    return true;
}

/**
 * @brief Check which D2H slots are occupied without reading the full messages
 */
template<typename Reader>
inline bool getD2HOccupiedSlots(Reader& reader, std::vector<uint8_t>& occupied_slots) {
    occupied_slots.resize(MAILBOX_SLOT_COUNT);
    return reader.transfer(MBOX_D2H_SLOTS_OCCUPIED_OFFSET, occupied_slots.data(), MAILBOX_SLOT_COUNT) == MAILBOX_SLOT_COUNT;
}

/**
 * @brief Clear a slot using an occupied map and update the map
 */
template<typename Writer>
inline bool clearSlotAndUpdateMap(Writer& writer, std::vector<uint8_t>& occupied_map, uint32_t slot_index, bool is_d2h = true) {
    if (slot_index >= MAILBOX_SLOT_COUNT || slot_index >= occupied_map.size()) {
        return false;
    }
    
    // Clear the slot in shared memory
    uint8_t occupied = 0;
    uint32_t offset = is_d2h ? (MBOX_D2H_SLOTS_OCCUPIED_OFFSET + slot_index) 
                             : (MBOX_H2D_SLOTS_OCCUPIED_OFFSET + slot_index);
    
    if (writer.transfer(offset, &occupied, sizeof(uint8_t)) != sizeof(uint8_t)) {
        return false;
    }
    
    // Update local map
    occupied_map[slot_index] = 0;
    return true;
}

/**
 * @brief Clear multiple slots using an occupied map and update the map
 */
template<typename Writer>
inline bool clearSlotsAndUpdateMap(Writer& writer, std::vector<uint8_t>& occupied_map, const std::vector<uint32_t>& slot_indices, bool is_d2h = true) {
    bool all_success = true;
    
    for (uint32_t slot_index : slot_indices) {
        if (!clearSlotAndUpdateMap(writer, occupied_map, slot_index, is_d2h)) {
            all_success = false;
        }
    }
    
    return all_success;
}

/**
 * @brief Get list of occupied slot indices from occupied map
 */
inline std::vector<uint32_t> getOccupiedSlotIndices(const std::vector<uint8_t>& occupied_map) {
    std::vector<uint32_t> occupied_indices;
    occupied_indices.reserve(occupied_map.size());
    
    for (uint32_t i = 0; i < occupied_map.size(); ++i) {
        if (occupied_map[i]) {
            occupied_indices.push_back(i);
        }
    }
    
    return occupied_indices;
}

/**
 * @brief Check if a specific slot is marked as occupied in the map
 */
inline bool isSlotOccupied(const std::vector<uint8_t>& occupied_map, uint32_t slot_index) {
    return slot_index < occupied_map.size() && occupied_map[slot_index];
}

} // namespace MailboxUtils
