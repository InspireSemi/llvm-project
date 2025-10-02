#pragma once

#include <stddef.h>

/**
 * @brief API table structure for Zephyr OS system calls
 */
typedef struct __attribute__((packed)){
    int _API_TABLE_VERSION;
    int _IVSHMEM_ABI_VERSION;
    /* System output functions */
    void (*print_msg)(int ltid, const char *msg);
} api_table_t;

