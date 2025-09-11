#pragma once

#include "batch_state.h"
#include "message_slot.h"
#include "error_codes.h"
#include "mailbox_strategies.h"

// Initialization
error_code_t batch_controller_init(void);

/**
 * @brief Handle begin command using state machine
 * @param cmd Begin command payload
 * @param slot_index Slot where BEGIN message was received
 * @param message_slot Message slot containing the begin command
 * @return ERR_OK on success, error code on failure
 */
error_code_t batch_controller_handle_begin(const batch_begin_cmd_t* cmd, uint8_t slot_index, const message_slot_t* message_slot);

/**
 * @brief Handle end command using state machine
 * @param cmd End command payload
 * @param slot_index Slot where END message was received
 * @param message_slot Message slot containing the end command
 * @return ERR_OK on success, error code on failure
 */
error_code_t batch_controller_handle_end(const batch_end_cmd_t* cmd, uint8_t slot_index, const message_slot_t* message_slot);

/**
 * @brief Handle body message using state machine
 * @param slot Message slot containing body message
 * @param slot_index Slot index where message was received
 * @return ERR_OK on success, error code on failure
 */
error_code_t batch_controller_handle_body_message(const message_slot_t* slot, uint8_t slot_index);

/**
 * @brief Complete the current batch processing
 * @return ERR_OK on success, error code on failure
 */
error_code_t batch_controller_complete_batch(void);

/**
 * @brief Check if batch is active using state machine
 * @return true if batch is active, false otherwise
 */
bool batch_controller_is_batch_active(void);

/**
 * @brief Peek at current batch state (read-only)
 * @return Pointer to current batch state machine
 */
const batch_state_t* batch_controller_peek_batch_state(void);

/**
 * @brief Reset the batch controller state to inactive
 */
error_code_t batch_controller_reset_state(void);

/**
 * @brief Get the current send strategy based on batch state  
 * @return send_strategy_t configured for current batch mode
 */
send_strategy_t batch_controller_get_send_strategy(void);

/**
 * @brief Get the current receive strategy based on batch state
 * @return receive_strategy_t configured for current batch mode
 */
receive_strategy_t batch_controller_get_receive_strategy(void);