#include "MailboxUtils.hpp"

namespace MailboxUtils {

/**
 * @brief Get list of occupied slot indices from occupied map
 */
std::vector<uint32_t> getOccupiedSlotIndices(const std::vector<uint8_t>& occupied_map) {
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
bool isSlotOccupied(const std::vector<uint8_t>& occupied_map, uint32_t slot_index) {
    return slot_index < occupied_map.size() && occupied_map[slot_index];
}

} // namespace MailboxUtils
