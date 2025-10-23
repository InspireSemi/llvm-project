#include "api_functions.h"
#include "ivshmem_config.h"
#include "device_specs.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

LOG_MODULE_REGISTER(api_functions, LOG_LEVEL_DBG);

// Mutex for thread-safe log output
static K_MUTEX_DEFINE(log_output_mutex);

void api_populate_table(api_table_t *table) {
    if (!table) {
        LOG_ERR("Cannot populate NULL API table");
        return;
    }
    
    // Version information
    table->_API_TABLE_VERSION = API_TABLE_VERSION;
    table->_IVSHMEM_ABI_VERSION = IVSHMEM_ABI_VERSION;
    
    // Logging functions (custom implementation)
    table->log_msg = api_log_message;
    
    // Standard library functions (direct assignment)
    table->snprintf = snprintf;
    
    // Pthread functions (direct assignment - no wrappers needed)
    table->pthread_create = pthread_create;
    table->pthread_join = pthread_join;
    table->pthread_self = pthread_self;
    table->pthread_detach = pthread_detach;
    table->pthread_exit = pthread_exit;
    
    table->pthread_mutex_init = pthread_mutex_init;
    table->pthread_mutex_destroy = pthread_mutex_destroy;
    table->pthread_mutex_lock = pthread_mutex_lock;
    table->pthread_mutex_trylock = pthread_mutex_trylock;
    table->pthread_mutex_unlock = pthread_mutex_unlock;
    
    table->pthread_cond_init = pthread_cond_init;
    table->pthread_cond_destroy = pthread_cond_destroy;
    table->pthread_cond_wait = pthread_cond_wait;
    table->pthread_cond_signal = pthread_cond_signal;
    table->pthread_cond_broadcast = pthread_cond_broadcast;
    
    LOG_DBG("API table populated successfully");
}

void api_log_message(int tid, const char* format, ...) {
    if (format == NULL) {
        return;
    }
    
    // Use mutex to ensure atomic log output and prevent interleaved messages
    k_mutex_lock(&log_output_mutex, K_FOREVER);
    
    // Format the message with variable arguments
    char buffer[256];
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Check for truncation and warn if message was too long
    if (needed >= (int)sizeof(buffer)) {
        LOG_WRN("[Worker %d] Log message truncated (%d chars needed, %zu available)", 
                tid, needed, sizeof(buffer));
    }
    
    LOG_INF("[Worker %d] %s", tid, buffer);
    k_mutex_unlock(&log_output_mutex);
}
