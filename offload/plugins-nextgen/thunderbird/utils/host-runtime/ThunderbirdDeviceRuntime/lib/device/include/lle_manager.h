#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * Initialize the LLE manager
 */
int lle_manager_init(void);

/**
 * Load an LLEXT module from memory
 * @param address Base address of the ELF module in memory
 * @param size Size of the ELF module in bytes
 * @return 0 on success, negative error code on failure
 */
int lle_manager_load_module(uintptr_t address, size_t size);

/**
 * Load and get entry point for an LLE module at the given address
 * Returns function pointer to entry point or NULL if failed
 */
void* lle_manager_get_entry_point(uintptr_t address);

/**
 * Check if an address has a loaded LLE module and unload it if so
 * Called by device_memory before freeing buffers
 */
bool lle_manager_unload_if_loaded(uintptr_t address);

/**
 * Clean up the LLE manager
 */
void lle_manager_cleanup(void);