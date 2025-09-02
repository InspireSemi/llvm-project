#pragma once

#include "message_payloads.h"
#include "memory_command_handler.h"
#include "error_codes.h"

/**
 * @brief Build response for malloc command
 * @param result Result from malloc command execution
 * @param response Output response structure to populate
 * @return ERR_OK on success, error code on failure
 */
error_code_t build_malloc_response(const malloc_result_t* result, malloc_rsp_t* response);

/**
 * @brief Build response for free command
 * @param free_status Status result from free command execution
 * @param response Output response structure to populate
 * @return ERR_OK on success, error code on failure
 */
error_code_t build_free_response(error_code_t free_status, free_rsp_t* response);
