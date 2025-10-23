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

//TODO FIXME we dont do a good job of maintaining the state of which slots are occupied or not. We should have a better system for this.

namespace MailboxUtils {

/**
 * @brief Write a complete message to H2D mailbox slot
 */
template<typename Writer>
bool writeH2DMessage(Writer& writer, uint32_t slot_index, const message_slot_t* slot);

/**
 * @brief Read a complete message from D2H mailbox slot
 */
template<typename Reader>
bool readD2HMessage(Reader& reader, uint32_t slot_index, message_slot_t* slot);

/**
 * @brief Clear an H2D slot (mark as unoccupied) - host clears after sending
 */
template<typename Writer>
bool clearH2DSlot(Writer& writer, uint32_t slot_index);

/**
 * @brief Clear a D2H slot (mark as unoccupied) - host clears after reading
 */
template<typename Writer>
bool clearD2HSlot(Writer& writer, uint32_t slot_index);

/**
 * @brief Bulk read multiple slots from D2H mailbox
 */
template<typename Reader>
bool readD2HSlots(Reader& reader, uint32_t start_slot, uint32_t count, message_slot_t* slots, std::vector<uint8_t>* occupied_map = nullptr);

/**
 * @brief Check which D2H slots are occupied without reading the full messages
 */
template<typename Reader>
bool getD2HOccupiedSlots(Reader& reader, std::vector<uint8_t>& occupied_slots);

/**
 * @brief Clear a slot using an occupied map and update the map
 */
template<typename Writer>
bool clearSlotAndUpdateMap(Writer& writer, std::vector<uint8_t>& occupied_map, uint32_t slot_index, bool is_d2h = true);

/**
 * @brief Clear multiple slots using an occupied map and update the map
 */
template<typename Writer>
bool clearSlotsAndUpdateMap(Writer& writer, std::vector<uint8_t>& occupied_map, const std::vector<uint32_t>& slot_indices, bool is_d2h = true);

/**
 * @brief Get list of occupied slot indices from occupied map
 */
std::vector<uint32_t> getOccupiedSlotIndices(const std::vector<uint8_t>& occupied_map);

/**
 * @brief Check if a specific slot is marked as occupied in the map
 */
bool isSlotOccupied(const std::vector<uint8_t>& occupied_map, uint32_t slot_index);

/**
 * @brief Clear all D2H mailbox slots
 */
template<typename Writer>
void clearAllD2HSlots(Writer& writer);

/**
 * @brief Clear D2H mailbox slots from a response batch
 */
template<typename Writer>
void clearResponseSlots(Writer& writer, const std::vector<std::pair<int, message_slot_t>>& response_batch);

#include "MailboxUtils.tpp"

} // namespace MailboxUtils
