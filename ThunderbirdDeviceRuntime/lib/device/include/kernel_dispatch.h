#pragma once

#include <stddef.h>
#include "error_codes.h"

/**
 * @brief Dispatch a kernel function with variable arguments
 * 
 * This function provides a generic mechanism to call kernel functions with
 * a variable number of pointer-sized arguments. It uses a jump table to
 * efficiently dispatch to the correct function signature.
 * 
 * @param kernel_fn Pointer to kernel function (will be cast to appropriate signature)
 * @param args Array of void* arguments to pass to the kernel
 * @param num_args Number of arguments (0-8 supported, expandable to 16+)
 * @return ERR_OK on success, ERR_INVALID_PARAM if num_args exceeds maximum supported
 */
error_code_t dispatch_kernel(void (*kernel_fn)(void*), void **args, size_t num_args);

/**
 * @brief Get the maximum number of arguments supported by the dispatcher
 * @return Maximum argument count
 */
size_t dispatch_kernel_max_args(void);
