/**
 * @file mailbox_operations.c
 * @brief Strategy-based mailbox operations implementation
 * @details Implements clean, focused API with external orchestration for batch handling.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.3
 * @date 2025-08-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "mailbox_operations.h"
#include "mailbox_region.h"
#include "mailbox_strategies.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <string.h>

LOG_MODULE_REGISTER(mailbox_ops, CONFIG_LOG_DEFAULT_LEVEL);

// External orchestration interface - implemented by batch controller
extern receive_strategy_t batch_controller_get_receive_strategy(void);
extern send_strategy_t batch_controller_get_send_strategy(void);


// Helper functions for timeout handling and mailbox operations

/**
 * @brief Convert k_timeout_t to milliseconds for easier handling
 * @param timeout Zephyr timeout value
 * @return Timeout in milliseconds (INT64_MAX for K_FOREVER, 0 for K_NO_WAIT)
 */
static int64_t timeout_to_ms(k_timeout_t timeout)
{
    if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {
        return INT64_MAX;
    }
    if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
        return 0;
    }
    return timeout.ticks / CONFIG_SYS_CLOCK_TICKS_PER_SEC * 1000;
}

/**
 * @brief Check if timeout has expired
 * @param start_time Start time in milliseconds
 * @param timeout_ms Timeout in milliseconds
 * @return true if timeout expired, false otherwise
 */
static bool is_timeout_expired(int64_t start_time, int64_t timeout_ms)
{
    if (timeout_ms == INT64_MAX) {
        return false; // K_FOREVER never expires
    }
    return (k_uptime_get() - start_time) >= timeout_ms;
}

// /**
//  * @brief Generic mailbox send operation with timeout and strategy
//  * @param mbox Target mailbox
//  * @param msg Message to send
//  * @param timeout Timeout value
//  * @param operation_name Name for logging (e.g., "send to host")
//  * @param strategy Send strategy to use
//  * @param slot_index_out Optional pointer to store the slot index where message was placed
//  * @return true on success, false on failure/timeout
//  */
// static bool mailbox_send_with_timeout(volatile mailbox_t* mbox, const message_slot_t* msg, 
//                                      k_timeout_t timeout, const char* operation_name,
//                                      send_strategy_t strategy, uint32_t* slot_index_out)
// {
//     if (!msg) {
//         LOG_ERR("%s: Invalid parameters", operation_name);
//         return false;
//     }

//     int64_t start_time = k_uptime_get();
//     int64_t timeout_ms = timeout_to_ms(timeout);

//     while (true) {
//         uint32_t slot_index;
        
//         // Use strategy to determine which slot to write to
//         if (strategy.send_to_slot_fn(mbox, &strategy, &slot_index)) {
//             // Copy message to slot
//             memcpy((void*)&mbox->slots[slot_index], msg, sizeof(message_slot_t));
//             // Mark slot as occupied, slots are to be freed by another step TBD but associated with observing the END of a command batch.
//             mbox->slots_occupied[slot_index] = 1;
//             LOG_DBG("Message %s at slot %u", operation_name, slot_index);
            
//             // Store slot index if caller wants it
//             if (slot_index_out) {
//                 *slot_index_out = slot_index;
//             }
            
//             return true;
//         }

//         // No free slots available using this strategy
//         if (is_timeout_expired(start_time, timeout_ms)) {
//             LOG_WRN("%s: Timeout - no suitable slots", operation_name);
//             return false;
//         }

//         if (timeout_ms == 0) {
//             return false;
//         }

//         k_sleep(K_MSEC(10000));
//     }
// }

/**
 * @brief Generic mailbox receive operation with timeout and strategy
 * @param mbox Source mailbox
 * @param msg Buffer for received message
 * @param timeout Timeout value
 * @param operation_name Name for logging (e.g., "receive from host")
 * @param strategy Receive strategy to use
 * @param slot_index_out Optional pointer to store the slot index where message was received from
 * @return true on success, false on failure/timeout
 */
static bool mailbox_receive_with_timeout(volatile mailbox_t* mbox, message_slot_t* msg, 
                                        k_timeout_t timeout, const char* operation_name,
                                        receive_strategy_t strategy, uint32_t* slot_index_out)
{
    if (!msg) {
        LOG_ERR("%s: Invalid parameters", operation_name);
        return false;
    }

    int64_t start_time = k_uptime_get();
    int64_t timeout_ms = timeout_to_ms(timeout);

    while (true) {
        uint32_t slot_index;
        
        // Use strategy to determine which slot to read
        if (strategy.find_slot_fn(mbox, &strategy, &slot_index)) {
            // Copy message from slot
            memcpy(msg, (void*)&mbox->slots[slot_index], sizeof(message_slot_t));
            // The sender, not us, must maintain the slot_occupied record, we just read.
            LOG_DBG("Message %s from slot %u", operation_name, slot_index);
            
            // Store slot index if caller wants it
            if (slot_index_out) {
                *slot_index_out = slot_index;
            }
            
            return true;
        }

        // No suitable messages using this strategy
        if (is_timeout_expired(start_time, timeout_ms)) {
            LOG_WRN("%s: Timeout - no suitable messages", operation_name);
            return false;
        }

        if (timeout_ms == 0) {
            return false;
        }

        k_sleep(K_MSEC(10000));
    }
}

/**
 * @brief Check if mailbox has space for new messages
 */
bool mailbox_has_space(const volatile mailbox_t* mbox)
{
    if (!mbox) {
        return false;
    }
    
    uint32_t occupied_count = 0;
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        occupied_count += mbox->slots_occupied[i];
    }
    return (occupied_count < MAILBOX_SLOT_COUNT);
}

/**
 * @brief Check if mailbox has messages available
 */
bool mailbox_has_messages(const volatile mailbox_t* mbox)
{
    if (!mbox) {
        return false;
    }
    
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        if (mbox->slots_occupied[i] > 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Get current message count in mailbox
 */
uint32_t mailbox_message_count(const volatile mailbox_t* mbox)
{
    if (!mbox) {
        return 0;
    }
    
    uint32_t occupied_count = 0;
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        occupied_count += mbox->slots_occupied[i];
    }
    return occupied_count;
}

/**
 * @brief Reset mailbox to empty state
 */
void mailbox_reset(volatile mailbox_t* mbox)
{
    if (mbox != NULL) {
        memset((void*)mbox->slots_occupied, 0, sizeof(mbox->slots_occupied));
        memset((void*)mbox->slots, 0, sizeof(mbox->slots));
    }
}

/**
 * @brief Get a pointer to a specific message slot in a mailbox
 */
volatile message_slot_t* mailbox_get_slot(volatile mailbox_t* mbox, uint32_t slot_index)
{
    if (!mbox) {
        LOG_ERR("Invalid mailbox pointer");
        return NULL;
    }
    
    if (slot_index >= MAILBOX_SLOT_COUNT) {
        LOG_ERR("Slot index %u exceeds maximum %u", slot_index, MAILBOX_SLOT_COUNT - 1);
        return NULL;
    }
    
    return &mbox->slots[slot_index];
}

/**
 * @brief Copy the H2D mailbox contents to the caller's mailbox
 */
bool mailbox_copy_h2d(volatile mailbox_t* dest_mbox)
{
    if (!dest_mbox) {
        LOG_ERR("Invalid destination mailbox pointer");
        return false;
    }
    
    volatile mailbox_t* h2d_mailbox = mailbox_region_h2d();
    if (!h2d_mailbox) {
        LOG_ERR("Host-to-device mailbox not available");
        return false;
    }
    
    // Copy the entire mailbox structure
    memcpy((void*)dest_mbox, (void*)h2d_mailbox, sizeof(mailbox_t));
    
    LOG_DBG("Copied H2D mailbox contents to destination");
    return true;
}

/**
 * @brief Validate message parameters
 * @param msg Message to validate
 * @return true if valid, false otherwise
 */
static bool is_message_valid(const message_slot_t* msg)
{
    if (!msg) {
        return false;
    }
    
    // Check message length
    if (msg->length > MESSAGE_SLOT_DATA_SIZE) {
        LOG_ERR("Message length %u exceeds maximum %u", msg->length, MESSAGE_SLOT_DATA_SIZE);
        return false;
    }
    
    // Check message ID is reasonable
    if (msg->msg_id == MSG_INVALID) {
        LOG_ERR("Invalid message ID: MSG_INVALID");
        return false;
    }
    
    // Check message ID is within known ranges
    if (msg->msg_id > MSG_RSP_TRANSFER_FINISHED && msg->msg_id < MSG_VENDOR_BASE) {
        LOG_ERR("Unknown message ID: %u", msg->msg_id);
        return false;
    }
    
    return true;
}

// Device-perspective mailbox operations (strategy-driven)

// bool mailbox_send(const message_slot_t* msg, k_timeout_t timeout, uint32_t* slot_index_out)
// {
//     if (!is_message_valid(msg)) {
//         LOG_ERR("Invalid message parameters for send");
//         return false;
//     }
    
//     volatile mailbox_t* d2h_mailbox = mailbox_region_d2h();
//     if (!d2h_mailbox) {
//         LOG_ERR("Device-to-host mailbox not available");
//         return false;
//     }
    
//     // Get strategy from external batch controller
//     send_strategy_t strategy = batch_controller_get_send_strategy();
    
//     return mailbox_send_with_timeout(d2h_mailbox, msg, timeout, "sent to host", strategy, slot_index_out);
// }

bool mailbox_receive(message_slot_t* msg, k_timeout_t timeout, uint32_t* slot_index_out)
{
    LOG_DBG("Attempting to receive message with timeout %lld ms", timeout_to_ms(timeout));

    if (!msg) {
        LOG_ERR("Invalid message buffer for receive");
        return false;
    }
    
    volatile mailbox_t* h2d_mailbox = mailbox_region_h2d();
    if (!h2d_mailbox) {
        LOG_ERR("Host-to-device mailbox not available");
        return false;
    }
    
    // Get strategy from external batch controller
    receive_strategy_t strategy = batch_controller_get_receive_strategy();

    LOG_DBG("Got receive strategy: find_slot_fn=%p", strategy.find_slot_fn);
    
    bool result = mailbox_receive_with_timeout(h2d_mailbox, msg, timeout, "received from host", strategy, slot_index_out);
    
    // Validate received message
    if (result && !is_message_valid(msg)) {
        LOG_ERR("Received invalid message - corrupted data?");
        return false;
    }

    if (result) {
        LOG_DBG("Successfully received message: ID=%u, length=%u", msg->msg_id, msg->length);
    } else {
        LOG_WRN("Failed to receive message within timeout");
    }
        
    return result;
}

error_code_t mailbox_operations_send_message_to_slot(message_id_t msg_id,
                                                     const void* data,
                                                     uint32_t length,
                                                     uint8_t slot_index)
{
    if (!data && length > 0) {
        return ERR_INVALID_PARAM;
    }
    
    if (length > MESSAGE_SLOT_DATA_SIZE) {
        LOG_ERR("Message length %u exceeds maximum %u", length, MESSAGE_SLOT_DATA_SIZE);
        return ERR_INVALID_PARAM;
    }
    
    if (slot_index >= MAILBOX_SLOT_COUNT) {
        LOG_ERR("Slot index %u exceeds maximum %u", slot_index, MAILBOX_SLOT_COUNT - 1);
        return ERR_INVALID_PARAM;
    }
    
    volatile mailbox_t* d2h_mailbox = mailbox_region_d2h();
    if (!d2h_mailbox) {
        LOG_ERR("Device-to-host mailbox not available");
        return ERR_IO;
    }
    
    // Create message slot
    message_slot_t msg_slot = {0};
    msg_slot.msg_id = msg_id;
    msg_slot.length = length;
    if (length > 0 && data) {
        memcpy(msg_slot.data, data, length);
    }
    
    // Get send strategy and send to specific slot
    send_strategy_t strategy = batch_controller_get_send_strategy();
    
    if (!strategy.send_to_slot_fn) {
        LOG_ERR("No send strategy function provided");
        return ERR_IO;
    }
    
    // Validate the slot can be used
    if (!strategy.send_to_slot_fn(d2h_mailbox, &strategy, slot_index)) {
        LOG_ERR("Strategy rejected slot %u for sending", slot_index);
        return ERR_IO;
    }
    
    // Write the message to the specified slot
    d2h_mailbox->slots[slot_index].msg_id = msg_slot.msg_id;
    d2h_mailbox->slots[slot_index].length = msg_slot.length;
    if (msg_slot.length > 0) {
        memcpy((void*)d2h_mailbox->slots[slot_index].data, msg_slot.data, msg_slot.length);
    }
    d2h_mailbox->slots_occupied[slot_index] = 1;
    
    LOG_DBG("Message %u sent to slot %u", msg_slot.msg_id, slot_index);
    return ERR_OK;
}

error_code_t mailbox_operations_invalidate_h2d_slot(uint8_t slot_index)
{
    if (slot_index >= MAILBOX_SLOT_COUNT) {
        LOG_ERR("Slot index %u exceeds maximum %u", slot_index, MAILBOX_SLOT_COUNT - 1);
        return ERR_INVALID_PARAM;
    }
    
    volatile mailbox_t* h2d_mailbox = mailbox_region_h2d();
    if (!h2d_mailbox) {
        LOG_ERR("Host-to-device mailbox not available");
        return ERR_IO;
    }
    
    // Clear the slot
    h2d_mailbox->slots_occupied[slot_index] = 0;
    memset((void*)&h2d_mailbox->slots[slot_index], 0, sizeof(message_slot_t));
    
    LOG_DBG("H2D slot %u invalidated", slot_index);
    return ERR_OK;
}

