#pragma once

#include "message_payloads.h"
#include "error_codes.h"

/**
 * @brief Result structure for malloc command execution
 */
typedef struct {
    error_code_t status;
    uint64_t address;
} malloc_result_t;

/**
 * @brief Handle malloc command - allocate memory with specified size and alignment
 * @param cmd Malloc command parameters
 * @param result Output result structure with status and allocated address
 * @return ERR_OK on success, error code on failure
 */
error_code_t handle_malloc_command(const malloc_cmd_t* cmd, malloc_result_t* result);

/**
 * @brief Handle free command - deallocate previously allocated memory
 * @param cmd Free command parameters containing address to free
 * @return ERR_OK on success, error code on failure
 */
error_code_t handle_free_command(const free_cmd_t* cmd);
