/**
 * @file device_handler.h
 * @brief Device command handler interface
 * @details Processes commands sent to the device and generates appropriate responses.
 *          Uses generic interfaces for memory and device operations.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include "message_slot.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the device handler
 * @return 0 on success, negative error code on failure
 */
int device_handler_init(void);

/**
 * @brief Process a message and generate a response
 * @param slot Incoming message slot
 * @param response Response message slot to fill
 * @return true if message was handled successfully, false otherwise
 */
bool device_handle_message(const message_slot_t* slot, message_slot_t* response);

#ifdef __cplusplus
}
#endif
