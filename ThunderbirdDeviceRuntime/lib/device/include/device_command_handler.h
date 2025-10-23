#pragma once

#include "message_payloads.h"
#include "device_types.h"

/**
 * @brief Handle device query command - retrieve device information
 * @param cmd Query command parameters
 * @param result Output result structure with status and device info
 * @return ERR_OK on success, error code on failure
 */
error_code_t handle_query_device_command(const query_device_cmd_t* cmd, query_result_t* result);
