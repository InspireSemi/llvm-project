/**
 * @file mailbox_operations.h
 * @brief Mailbox operations for device-perspective communication
 * @details Provides clean API for mailbox communication from device perspective.
 * 
 * Device perspective operations (default):
 * - mailbox_send() sends to host (via D2H mailbox)  
 * - mailbox_receive() receives from host (via H2D mailbox)
 * 
 * Test operations (host emulation):
 * - mailbox_send_to_device() sends to device (via H2D mailbox)
 * - mailbox_receive_from_device() receives from device (via D2H mailbox)
 * 
 * All operations work with message_slot_t structures containing:
 * - msg_id: Message type identifier
 * - length: Data payload length (max 64 bytes)
 * - data[]: Actual message payload
 * - checksum: Message integrity check
 * 
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.3
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include "mailbox.h"
#include "message_slot.h"
#include "message_id.h"
#include "error_codes.h"
#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Device perspective operations (default)

/**
 * @brief Receive a message from host (device perspective)
 * @param msg Buffer to store the received message slot
 * @param timeout Timeout for the operation (K_NO_WAIT, K_FOREVER, or K_MSEC(ms))
 * @param slot_index_out Optional pointer to store the slot index where message was received from (can be NULL)
 * @return true if message was received successfully, false otherwise
 * @note Receives from Host-to-Device (H2D) mailbox
 */
bool mailbox_receive(message_slot_t* msg, k_timeout_t timeout, uint32_t* slot_index_out);

/**
 * @brief Check if a mailbox has space for a new message
 * @param mbox Pointer to the mailbox
 * @return true if there's space, false if full or invalid
 */
bool mailbox_has_space(const volatile mailbox_t* mbox);

/**
 * @brief Check if a mailbox has any messages available
 * @param mbox Pointer to the mailbox
 * @return true if there are messages, false if empty or invalid
 */
bool mailbox_has_messages(const volatile mailbox_t* mbox);

/**
 * @brief Get the number of messages currently in the mailbox
 * @param mbox Pointer to the mailbox
 * @return Number of messages in the mailbox, 0 if empty or invalid
 */
uint32_t mailbox_message_count(const volatile mailbox_t* mbox);

/**
 * @brief Reset a mailbox to empty state
 * @param mbox Pointer to the mailbox to reset
 */
void mailbox_reset(volatile mailbox_t* mbox);

/**
 * @brief Get a pointer to a specific message slot in a mailbox
 * @param mbox Pointer to the mailbox
 * @param slot_index Index of the slot to retrieve (0 to MAILBOX_MAX_MESSAGES-1)
 * @return Pointer to the message slot, or NULL if invalid index or mailbox
 */
volatile message_slot_t* mailbox_get_slot(volatile mailbox_t* mbox, uint32_t slot_index);

/**
 * @brief Copy the H2D mailbox contents to the caller's mailbox
 * @param dest_mbox Pointer to destination mailbox to copy into
 * @return true if copy was successful, false if either mailbox is invalid
 * @note This copies the entire H2D mailbox structure including header and all slots
 */
bool mailbox_copy_h2d(volatile mailbox_t* dest_mbox);

/**
 * @brief Send message to specific slot (batch response only)
 */
error_code_t mailbox_operations_send_message_to_slot(message_id_t msg_id,
                                                     const void* data,
                                                     uint32_t length,
                                                     uint8_t slot_index);

/**
 * @brief Invalidate H2D slot
 */
error_code_t mailbox_operations_invalidate_h2d_slot(uint8_t slot_index);
