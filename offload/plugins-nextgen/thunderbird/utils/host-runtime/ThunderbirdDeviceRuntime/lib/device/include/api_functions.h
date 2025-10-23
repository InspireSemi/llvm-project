#pragma once

#include <pthread.h>
#include "api_table.h"

/**
 * @brief Populate an API table with all available function pointers
 * 
 * This function initializes an api_table_t structure with all system functions
 * that are exposed to kernel code. Should be called before passing the API
 * table to any kernel module.
 * 
 * @param table Pointer to the API table to populate
 */
void api_populate_table(api_table_t *table);

/**
 * @brief Thread-safe formatted message output for worker threads
 * 
 * This function is exposed to kernel code via the API table and provides
 * immediate log output with thread identification. Messages are printed
 * in real-time as they are produced, avoiding buffer limitations.
 * Supports printf-style formatting so kernel code doesn't need snprintf.
 * 
 * @param tid Thread ID
 * @param format Printf-style format string
 * @param ... Variable arguments for formatting
 */
void api_log_message(int tid, const char* format, ...);
