#pragma once

#include "message_payloads.h"
#include "device_types.h"

/**
 * @brief Handle kernel launch command - execute kernel with specified parameters
 * @param cmd Launch command parameters
 * @param result Output result structure with status and launch info
 * @return ERR_OK on success, error code on failure
 */
error_code_t handle_launch_command(const launch_cmd_t* cmd, launch_result_t* result);
