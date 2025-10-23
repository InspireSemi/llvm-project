/**
 * @file message_router.h
 * @brief Message router interface
 * @details Routes and processes messages between different components and generates appropriate responses.
 *          Uses generic interfaces for memory and device operations.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.2
 * @date 2025-08-16
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include "message_slot.h"
#include "error_codes.h"
#include <stdbool.h>

/**
 * @brief Handle incoming message - simplified signature
 * @param slot Message to process
 * @param slot_message_received_at Slot index where message was received (or target slot for responses)
 * @return true if message was handled successfully, false otherwise
 */
bool message_router_handle(const message_slot_t* slot, uint8_t slot_message_received_at);

/**
 * @brief Generate response for a command message (business logic processing)
 * @param input Input command message to process
 * @param output Output response message (caller provides storage)
 * @return ERR_OK on success, error code on failure
 */
error_code_t message_router_generate_response(const message_slot_t* input, message_slot_t* output);
