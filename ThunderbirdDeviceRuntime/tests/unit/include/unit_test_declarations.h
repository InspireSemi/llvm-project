/**
 * @file unit_test_declarations.h
 * @brief Declarations for all unit test runner functions
 */

#ifndef UNIT_TEST_DECLARATIONS_H
#define UNIT_TEST_DECLARATIONS_H

#include <stdint.h>

// Unit test runner function declarations
uint32_t run_device_memory_unit_tests(void);
uint32_t run_mailbox_unit_tests(void);
uint32_t run_mailbox_operations_unit_tests(void);
uint32_t run_mailbox_region_unit_tests(void);
uint32_t run_message_slot_unit_tests(void);
uint32_t run_message_payloads_unit_tests(void);
uint32_t run_ivshmem_shm_unit_tests(void);
uint32_t run_ivshmem_heap_unit_tests(void);

// New test function declarations
void run_new_mailbox_operations_tests(void);
void run_new_message_payloads_tests(void);
void run_new_message_slot_tests(void);
void run_batch_controller_tests(void);
void run_batch_state_tests(void);
void run_message_router_tests(void);

// Resource management and kernel processing test declarations
uint32_t run_resource_map_tests(void);
uint32_t run_lle_manager_tests(void);
uint32_t run_kernel_processor_tests(void);

#endif // UNIT_TEST_DECLARATIONS_H
