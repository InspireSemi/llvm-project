/**
 * @file message_utils.c
 * @brief Message formation and handling implementation
 * @details Provides device-side functions for creating and formatting messages
 *          according to the messaging protocol. These functions populate
 *          message_slot_t structures with proper message types and payloads
 *          for transmission over the communication interface.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.2
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "ivshmem_abi.h"    // For MSG_TYPE_*, message_slot_t, etc.
#include <stddef.h>  // For size_t
#include <string.h>  // For memcpy
#include <zephyr/kernel.h>  // Device-specific Zephyr includes

/**
 * @brief Forms a ping message in the provided message slot
 * @param slot Pointer to message slot to populate
 * @return 0 on success, negative error code on failure
 * @retval -EINVAL if slot is NULL
 */
int form_ping_message(message_slot_t *slot) {
    if (!slot) return -EINVAL;
    
    slot->msg_id = MSG_PING;
    slot->length = 0;
    // ... rest of ping message setup
    
    return 0;
}

/**
 * @brief Forms a data message with the specified payload
 * @param slot Pointer to message slot to populate
 * @param data Pointer to data to copy into message payload
 * @param len Length of data in bytes
 * @return 0 on success, negative error code on failure
 * @retval -EINVAL if slot or data is NULL, or len exceeds payload capacity
 */
int form_data_message(message_slot_t *slot, const void *data, size_t len) {
    if (!slot || !data || len > MESSAGE_SLOT_DATA_SIZE) {
        return -EINVAL;
    }
    
    slot->msg_id = MSG_DATA;
    slot->length = len;
    memcpy(slot->data, data, len);
    
    return 0;
}