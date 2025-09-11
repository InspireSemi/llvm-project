#pragma once

#include "message_payloads.h"
#include "kernel_command_handler.h"
#include "device_command_handler.h"
#include "error_codes.h"

/**
 * @brief Build response for kernel launch command
 * @param result Result from launch command execution
 * @param response Output response structure to populate
 * @return ERR_OK on success, error code on failure
 */
error_code_t build_launch_response(const launch_result_t* result, launch_rsp_t* response);

/**
 * @brief Build response for device query command
 * @param result Result from query command execution
 * @param response Output response structure to populate
 * @return ERR_OK on success, error code on failure
 */
error_code_t build_query_device_response(const query_result_t* result, query_device_rsp_t* response);
