#pragma once

#include <stddef.h>
#include "posix_internal.h"

BUILD_ASSERT(CONFIG_POSIX_THREAD_THREADS_MAX == CONFIG_MAX_PTHREAD_MUTEX_COUNT);
// TODO: Re-enable when CONFIG_DYNAMIC_THREAD_POOL_SIZE is properly defined in Zephyr
//BUILD_ASSERT(CONFIG_DYNAMIC_THREAD_POOL_SIZE == CONFIG_POSIX_THREAD_THREADS_MAX);

/**
 * @brief API table structure for Zephyr OS system calls
 */
typedef struct __attribute__((packed)){
    int _API_TABLE_VERSION;
    int _IVSHMEM_ABI_VERSION;
    
    /* Logging functions */
    void (*log_msg)(int tid, const char *format, ...);
    
    /* String formatting functions */
    int (*snprintf)(char *str, size_t size, const char *format, ...);
    
    /* Thread management functions */
    int (*pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine)(void *), void *arg);
    int (*pthread_join)(pthread_t thread, void **retval);
    pthread_t (*pthread_self)(void);
    int (*pthread_detach)(pthread_t thread);
    void (*pthread_exit)(void *retval);
    
    /* Mutex functions */
    int (*pthread_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
    int (*pthread_mutex_destroy)(pthread_mutex_t *mutex);
    int (*pthread_mutex_lock)(pthread_mutex_t *mutex);
    int (*pthread_mutex_trylock)(pthread_mutex_t *mutex);
    int (*pthread_mutex_unlock)(pthread_mutex_t *mutex);
    
    /* Condition variable functions */
    int (*pthread_cond_init)(pthread_cond_t *cond, const pthread_condattr_t *attr);
    int (*pthread_cond_destroy)(pthread_cond_t *cond);
    int (*pthread_cond_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex);
    int (*pthread_cond_signal)(pthread_cond_t *cond);
    int (*pthread_cond_broadcast)(pthread_cond_t *cond);
} api_table_t;

