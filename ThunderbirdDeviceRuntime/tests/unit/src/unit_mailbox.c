/**
 * @file unit_mailbox.c
 * @brief Unit tests for mailbox structure sanity checks
 */

#include "unit_test_framework.h"
#include "mailbox.h"
#include "message_slot.h"
#include "message_id.h"
#include "ivshmem_abi.h"

#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>

static test_result_t test_mailbox_structure_size(void)
{
    // Test mailbox_t structure size
    // Should verify structure size is reasonable and aligned
    // Should verify structure fits expected memory layout
    size_t mailbox_size = sizeof(mailbox_t);
    size_t expected_min_size = MAILBOX_SLOT_COUNT + (sizeof(message_slot_t) * MAILBOX_SLOT_COUNT);  // slots_occupied + slots arrays
    
    TEST_ASSERT(mailbox_size >= expected_min_size, "Mailbox size too small");
    TEST_ASSERT(mailbox_size % 8 == 0, "Mailbox size not 8-byte aligned");
    TEST_ASSERT(mailbox_size < 4096, "Mailbox size unreasonably large");
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_alignment(void)
{
    // Test mailbox_t structure alignment
    // Should verify structure is properly aligned (8-byte boundary)
    // Should verify packed attribute is working correctly
    mailbox_t test_mailbox;
    uintptr_t addr = (uintptr_t)&test_mailbox;
    
    TEST_ASSERT(addr % 8 == 0, "Mailbox not aligned to 8-byte boundary");
    TEST_ASSERT(_Alignof(mailbox_t) >= 8, "Mailbox alignment requirement too small");
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_slot_count(void)
{
    // Test MAILBOX_SLOT_COUNT constant
    // Should verify slot count is reasonable (16)
    // Should verify slots array size matches slot count
    TEST_ASSERT(MAILBOX_SLOT_COUNT == 16, "MAILBOX_SLOT_COUNT should be 16");
    
    mailbox_t test_mailbox;
    size_t slots_array_size = sizeof(test_mailbox.slots);
    size_t expected_slots_size = sizeof(message_slot_t) * MAILBOX_SLOT_COUNT;
    
    TEST_ASSERT(slots_array_size == expected_slots_size, "Slots array size mismatch");
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_initialization(void)
{
    // Test mailbox structure initialization
    // Should verify zeroed mailbox has expected initial values
    // Should verify all arrays start at zero
    mailbox_t test_mailbox;
    memset(&test_mailbox, 0, sizeof(test_mailbox));
    
    // Verify slots_occupied array is initialized to zero
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "slots_occupied[%u] should be zero", i);
        TEST_ASSERT(test_mailbox.slots_occupied[i] == 0, msg);
    }
    
    // Verify slots array is initialized to zero
    for (int i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        TEST_ASSERT(test_mailbox.slots[i].msg_id == 0, "Slot message_id should be zero");
        TEST_ASSERT(test_mailbox.slots[i].length == 0, "Slot length should be zero");
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_slots_occupied_offset(void)
{
    // Test slots_occupied member offset
    // Should verify slots_occupied starts at offset 0
    // Should verify slots starts after slots_occupied
    size_t slots_occupied_offset = offsetof(mailbox_t, slots_occupied);
    size_t slots_offset = offsetof(mailbox_t, slots);
    
    TEST_ASSERT(slots_occupied_offset == 0, "slots_occupied offset should be 0");
    TEST_ASSERT(slots_offset == MAILBOX_SLOT_COUNT, "slots offset should be MAILBOX_SLOT_COUNT");
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_slots_occupied_access(void)
{
    // Test individual slots_occupied array element access
    // Should verify all elements can be read/written independently
    // Should verify array bounds are safe
    mailbox_t test_mailbox;
    memset(&test_mailbox, 0, sizeof(test_mailbox));
    
    // Test individual element access
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        test_mailbox.slots_occupied[i] = i + 1;  // Set unique values
    }
    
    // Verify each element retained its value
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "slots_occupied[%u] should be %u", i, i + 1);
        TEST_ASSERT(test_mailbox.slots_occupied[i] == (i + 1), msg);
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_slots_occupied_independence(void)
{
    // Test that slots_occupied elements are independent
    // Should verify setting one element doesn't affect others
    mailbox_t test_mailbox;
    memset(&test_mailbox, 0, sizeof(test_mailbox));
    
    // Set alternating pattern
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i += 2) {
        test_mailbox.slots_occupied[i] = 1;
    }
    
    // Verify pattern is correct
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        uint8_t expected = (i % 2 == 0) ? 1 : 0;
        char msg[64];
        snprintf(msg, sizeof(msg), "slots_occupied[%u] should be %u", i, expected);
        TEST_ASSERT(test_mailbox.slots_occupied[i] == expected, msg);
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_slots_occupied_vs_slots_isolation(void)
{
    // Test that slots_occupied and slots arrays don't interfere
    // Should verify modifying one doesn't affect the other
    mailbox_t test_mailbox;
    memset(&test_mailbox, 0, sizeof(test_mailbox));
    
    // Fill slots_occupied with pattern
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        test_mailbox.slots_occupied[i] = 0xFF;
    }
    
    // Fill slots with different pattern
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        test_mailbox.slots[i].msg_id = MSG_PING + i;
        test_mailbox.slots[i].length = i * 100;
        memset(test_mailbox.slots[i].data, 0x55, MESSAGE_SLOT_DATA_SIZE);
    }
    
    // Verify slots_occupied wasn't affected by slots modification
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        TEST_ASSERT(test_mailbox.slots_occupied[i] == 0xFF, 
                   "slots_occupied corrupted by slots modification");
    }
    
    // Verify slots wasn't affected by slots_occupied modification
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        TEST_ASSERT(test_mailbox.slots[i].msg_id == MSG_PING + i, 
                   "slots msg_id corrupted by slots_occupied modification");
        TEST_ASSERT(test_mailbox.slots[i].length == i * 100, 
                   "slots length corrupted by slots_occupied modification");
        TEST_ASSERT(test_mailbox.slots[i].data[0] == 0x55, 
                   "slots data corrupted by slots_occupied modification");
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_slots_access(void)
{
    // Test mailbox slots array access
    // Should verify all slots can be accessed
    // Should verify slot indexing works correctly
    // Should verify no buffer overrun issues
    mailbox_t test_mailbox;
    memset(&test_mailbox, 0, sizeof(test_mailbox));
    
    for (int i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        test_mailbox.slots[i].msg_id = MSG_PING + i;
        test_mailbox.slots[i].length = i * 4;
        
        TEST_ASSERT(test_mailbox.slots[i].msg_id == MSG_PING + i, 
                   "Slot message_id access failed");
        TEST_ASSERT(test_mailbox.slots[i].length == i * 4, 
                   "Slot length access failed");
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_circular_buffer_bounds(void)
{
    
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_message_slot_size(void)
{
    // Test message slot size within mailbox
    // Should verify message_slot_t size is consistent
    // Should verify slots array total size is correct
    size_t slot_size = sizeof(message_slot_t);
    size_t min_expected_size = sizeof(message_id_t) + sizeof(uint32_t) + MESSAGE_SLOT_DATA_SIZE + sizeof(uint32_t);
    size_t max_expected_size = min_expected_size + 8; // Allow for up to 8 bytes of padding due to aligned(8)
    
    TEST_ASSERT(slot_size >= min_expected_size, "Message slot size too small");
    TEST_ASSERT(slot_size <= max_expected_size, "Message slot size unreasonably large");
    TEST_ASSERT(slot_size % 8 == 0, "Message slot size not 8-byte aligned");
    
    mailbox_t test_mailbox;
    size_t total_slots_size = sizeof(test_mailbox.slots);
    size_t expected_total_size = slot_size * MAILBOX_SLOT_COUNT;
    
    TEST_ASSERT(total_slots_size == expected_total_size, "Total slots array size mismatch");
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_memory_layout(void)
{
    TEST_PASS_RESULT();
}

static test_result_t test_all_real_slot_addresses(void)
{
    // Test access to all real slot addresses in both H2D and D2H mailboxes
    uintptr_t shm_base = IVSHMEM_SHM_BASE;
    
    // Test all H2D slots
    for (uint32_t slot_idx = 0; slot_idx < MAILBOX_SLOT_COUNT; slot_idx++) {
        uintptr_t h2d_slot_addr = shm_base + MBOX_H2D_SLOT_OFFSET(slot_idx);
        message_slot_t *h2d_slot = (message_slot_t*)h2d_slot_addr;
        
        // Test slot field access
        h2d_slot->msg_id = MSG_CMD_MALLOC + slot_idx;
        h2d_slot->length = slot_idx * 4;
        
        TEST_ASSERT(h2d_slot->msg_id == MSG_CMD_MALLOC + slot_idx, 
                   "H2D slot message_id access failed");
        TEST_ASSERT(h2d_slot->length == slot_idx * 4, 
                   "H2D slot length access failed");
        
        // Test slot data array access
        h2d_slot->data[0] = 0xAA + slot_idx;
        h2d_slot->data[MESSAGE_SLOT_DATA_SIZE - 1] = 0xBB + slot_idx;
        
        TEST_ASSERT(h2d_slot->data[0] == 0xAA + slot_idx, 
                   "H2D slot data[0] access failed");
        TEST_ASSERT(h2d_slot->data[MESSAGE_SLOT_DATA_SIZE - 1] == 0xBB + slot_idx, 
                   "H2D slot data[end] access failed");
    }
    
    // Test all D2H slots
    for (uint32_t slot_idx = 0; slot_idx < MAILBOX_SLOT_COUNT; slot_idx++) {
        uintptr_t d2h_slot_addr = shm_base + MBOX_D2H_SLOT_OFFSET(slot_idx);
        message_slot_t *d2h_slot = (message_slot_t*)d2h_slot_addr;
        
        // Test slot field access
        d2h_slot->msg_id = MSG_RSP_MALLOC + slot_idx;
        d2h_slot->length = (slot_idx + 1) * 8;
        
        TEST_ASSERT(d2h_slot->msg_id == MSG_RSP_MALLOC + slot_idx, 
                   "D2H slot message_id access failed");
        TEST_ASSERT(d2h_slot->length == (slot_idx + 1) * 8, 
                   "D2H slot length access failed");
        
        // Test slot data array access
        d2h_slot->data[0] = 0xCC + slot_idx;
        d2h_slot->data[MESSAGE_SLOT_DATA_SIZE - 1] = 0xDD + slot_idx;
        
        TEST_ASSERT(d2h_slot->data[0] == 0xCC + slot_idx, 
                   "D2H slot data[0] access failed");
        TEST_ASSERT(d2h_slot->data[MESSAGE_SLOT_DATA_SIZE - 1] == 0xDD + slot_idx, 
                   "D2H slot data[end] access failed");
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_payload_alignment_in_all_real_slots(void)
{
    // Test that every payload type is properly aligned in every slot of both mailboxes
    uintptr_t shm_base = IVSHMEM_SHM_BASE;
    
    // Test alignment for all H2D slots with all payload types
    for (uint32_t slot_idx = 0; slot_idx < MAILBOX_SLOT_COUNT; slot_idx++) {
        uintptr_t h2d_slot_addr = shm_base + MBOX_H2D_SLOT_OFFSET(slot_idx);
        uintptr_t h2d_data_addr = h2d_slot_addr + offsetof(message_slot_t, data);
        
        // Test alignment for all payload types that could be sent H2D
        TEST_ASSERT((h2d_data_addr % _Alignof(malloc_cmd_t)) == 0, 
                   "H2D slot data not aligned for malloc_cmd_t");
        TEST_ASSERT((h2d_data_addr % _Alignof(free_cmd_t)) == 0, 
                   "H2D slot data not aligned for free_cmd_t");
        TEST_ASSERT((h2d_data_addr % _Alignof(launch_cmd_t)) == 0, 
                   "H2D slot data not aligned for launch_cmd_t");
        TEST_ASSERT((h2d_data_addr % _Alignof(ping_t)) == 0, 
                   "H2D slot data not aligned for ping_t");
        
        // Also test response types that might be echoed back in H2D
        TEST_ASSERT((h2d_data_addr % _Alignof(malloc_rsp_t)) == 0, 
                   "H2D slot data not aligned for malloc_rsp_t");
        TEST_ASSERT((h2d_data_addr % _Alignof(free_rsp_t)) == 0, 
                   "H2D slot data not aligned for free_rsp_t");
        TEST_ASSERT((h2d_data_addr % _Alignof(launch_rsp_t)) == 0, 
                   "H2D slot data not aligned for launch_rsp_t");
        TEST_ASSERT((h2d_data_addr % _Alignof(memory_status_rsp_t)) == 0, 
                   "H2D slot data not aligned for memory_status_rsp_t");
    }
    
    // Test alignment for all D2H slots with all payload types
    for (uint32_t slot_idx = 0; slot_idx < MAILBOX_SLOT_COUNT; slot_idx++) {
        uintptr_t d2h_slot_addr = shm_base + MBOX_D2H_SLOT_OFFSET(slot_idx);
        uintptr_t d2h_data_addr = d2h_slot_addr + offsetof(message_slot_t, data);
        
        // Test alignment for all payload types that could be sent D2H
        TEST_ASSERT((d2h_data_addr % _Alignof(malloc_rsp_t)) == 0, 
                   "D2H slot data not aligned for malloc_rsp_t");
        TEST_ASSERT((d2h_data_addr % _Alignof(free_rsp_t)) == 0, 
                   "D2H slot data not aligned for free_rsp_t");
        TEST_ASSERT((d2h_data_addr % _Alignof(launch_rsp_t)) == 0, 
                   "D2H slot data not aligned for launch_rsp_t");
        TEST_ASSERT((d2h_data_addr % _Alignof(memory_status_rsp_t)) == 0, 
                   "D2H slot data not aligned for memory_status_rsp_t");
        TEST_ASSERT((d2h_data_addr % _Alignof(ping_t)) == 0, 
                   "D2H slot data not aligned for ping_t");
        
        // Also test command types that might be echoed back in D2H
        TEST_ASSERT((d2h_data_addr % _Alignof(malloc_cmd_t)) == 0, 
                   "D2H slot data not aligned for malloc_cmd_t");
        TEST_ASSERT((d2h_data_addr % _Alignof(free_cmd_t)) == 0, 
                   "D2H slot data not aligned for free_cmd_t");
        TEST_ASSERT((d2h_data_addr % _Alignof(launch_cmd_t)) == 0, 
                   "D2H slot data not aligned for launch_cmd_t");
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_payload_access_in_all_real_slots(void)
{
    // Test that payload structures can be accessed in every real slot
    uintptr_t shm_base = IVSHMEM_SHM_BASE;
    
    // Test payload access in all H2D slots
    for (uint32_t slot_idx = 0; slot_idx < MAILBOX_SLOT_COUNT; slot_idx++) {
        uintptr_t h2d_slot_addr = shm_base + MBOX_H2D_SLOT_OFFSET(slot_idx);
        message_slot_t *h2d_slot = (message_slot_t*)h2d_slot_addr;
        
        // Test malloc_cmd_t in this slot
        malloc_cmd_t *malloc_cmd = (malloc_cmd_t*)h2d_slot->data;
        malloc_cmd->size = 1000 + slot_idx;
        malloc_cmd->alignment = 8 + slot_idx;
        TEST_ASSERT(malloc_cmd->size == 1000 + slot_idx, 
                   "malloc_cmd size access failed in H2D slot");
        TEST_ASSERT(malloc_cmd->alignment == 8 + slot_idx, 
                   "malloc_cmd alignment access failed in H2D slot");
        
        // Test launch_cmd_t in this slot (overwrite previous data)
        launch_cmd_t *launch_cmd = (launch_cmd_t*)h2d_slot->data;
        launch_cmd->kernel_address = 0x1000 + slot_idx;  // uint64_t, not void*
        launch_cmd->grid_x = 32 + slot_idx;
        launch_cmd->shared_mem_size = 2048 + slot_idx;
        TEST_ASSERT(launch_cmd->kernel_address == 0x1000 + slot_idx, 
                   "launch_cmd kernel_address failed in H2D slot");
        TEST_ASSERT(launch_cmd->grid_x == 32 + slot_idx, 
                   "launch_cmd grid_x failed in H2D slot");
        TEST_ASSERT(launch_cmd->shared_mem_size == 2048 + slot_idx, 
                   "launch_cmd shared_mem_size failed in H2D slot");
    }
    
    // Test payload access in all D2H slots
    for (uint32_t slot_idx = 0; slot_idx < MAILBOX_SLOT_COUNT; slot_idx++) {
        uintptr_t d2h_slot_addr = shm_base + MBOX_D2H_SLOT_OFFSET(slot_idx);
        message_slot_t *d2h_slot = (message_slot_t*)d2h_slot_addr;
        
        // Test malloc_rsp_t in this slot
        malloc_rsp_t *malloc_rsp = (malloc_rsp_t*)d2h_slot->data;
        malloc_rsp->status = ERR_OK + slot_idx;  // Changed from error_code
        malloc_rsp->address = 0x2000 + slot_idx * 0x100;  // uint64_t, not void*
        TEST_ASSERT(malloc_rsp->status == ERR_OK + slot_idx, 
                   "malloc_rsp status failed in D2H slot");  // Changed from error_code
        TEST_ASSERT(malloc_rsp->address == 0x2000 + slot_idx * 0x100, 
                   "malloc_rsp address failed in D2H slot");
        
        // Test memory_status_rsp_t in this slot (overwrite previous data)
        memory_status_rsp_t *mem_rsp = (memory_status_rsp_t*)d2h_slot->data;
        mem_rsp->status = ERR_NO_MEMORY + slot_idx;  // Changed from error_code
        mem_rsp->total_memory = 0x10000 + slot_idx * 0x1000;
        mem_rsp->free_memory = 0x8000 + slot_idx * 0x800;
        TEST_ASSERT(mem_rsp->status == ERR_NO_MEMORY + slot_idx, 
                   "memory_status_rsp status failed in D2H slot");  // Changed from error_code
        TEST_ASSERT(mem_rsp->total_memory == 0x10000 + slot_idx * 0x1000, 
                   "memory_status_rsp total_memory failed in D2H slot");
        TEST_ASSERT(mem_rsp->free_memory == 0x8000 + slot_idx * 0x800, 
                   "memory_status_rsp free_memory failed in D2H slot");
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_real_slot_address_calculations(void)
{
    // Test that real slot address calculations are correct and non-overlapping
    uintptr_t shm_base = IVSHMEM_SHM_BASE;
    
    // Verify H2D slots don't overlap
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT - 1; i++) {
        uintptr_t slot_i_addr = shm_base + MBOX_H2D_SLOT_OFFSET(i);
        uintptr_t slot_next_addr = shm_base + MBOX_H2D_SLOT_OFFSET(i + 1);
        
        TEST_ASSERT(slot_next_addr >= slot_i_addr + sizeof(message_slot_t), 
                   "H2D slots overlap in real memory");
        TEST_ASSERT(slot_next_addr == slot_i_addr + sizeof(message_slot_t), 
                   "H2D slots have unexpected gaps in real memory");
    }
    
    // Verify D2H slots don't overlap
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT - 1; i++) {
        uintptr_t slot_i_addr = shm_base + MBOX_D2H_SLOT_OFFSET(i);
        uintptr_t slot_next_addr = shm_base + MBOX_D2H_SLOT_OFFSET(i + 1);
        
        TEST_ASSERT(slot_next_addr >= slot_i_addr + sizeof(message_slot_t), 
                   "D2H slots overlap in real memory");
        TEST_ASSERT(slot_next_addr == slot_i_addr + sizeof(message_slot_t), 
                   "D2H slots have unexpected gaps in real memory");
    }
    
    // Verify H2D and D2H mailboxes don't overlap
    uintptr_t h2d_end = shm_base + MBOX_H2D_OFFSET + sizeof(mailbox_t);
    uintptr_t d2h_start = shm_base + MBOX_D2H_OFFSET;
    
    TEST_ASSERT(d2h_start >= h2d_end, "H2D and D2H mailboxes overlap in real memory");
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_alignment_at_real_addresses(void)
{
    // Test mailbox structure alignment at real IVSHMEM addresses
    uintptr_t shm_base = IVSHMEM_SHM_BASE;
    
    // Test H2D mailbox alignment
    uintptr_t h2d_mailbox_addr = shm_base + MBOX_H2D_OFFSET;
    TEST_ASSERT((h2d_mailbox_addr % _Alignof(mailbox_t)) == 0, 
               "H2D mailbox not properly aligned at real address");
    TEST_ASSERT((h2d_mailbox_addr % MBOX_ALIGN) == 0, 
               "H2D mailbox not aligned to MBOX_ALIGN at real address");
    
    // Test D2H mailbox alignment
    uintptr_t d2h_mailbox_addr = shm_base + MBOX_D2H_OFFSET;
    TEST_ASSERT((d2h_mailbox_addr % _Alignof(mailbox_t)) == 0, 
               "D2H mailbox not properly aligned at real address");
    TEST_ASSERT((d2h_mailbox_addr % MBOX_ALIGN) == 0, 
               "D2H mailbox not aligned to MBOX_ALIGN at real address");
    
    // Test individual slot alignment in both mailboxes
    for (uint32_t slot_idx = 0; slot_idx < MAILBOX_SLOT_COUNT; slot_idx++) {
        uintptr_t h2d_slot_addr = shm_base + MBOX_H2D_SLOT_OFFSET(slot_idx);
        uintptr_t d2h_slot_addr = shm_base + MBOX_D2H_SLOT_OFFSET(slot_idx);
        
        TEST_ASSERT((h2d_slot_addr % _Alignof(message_slot_t)) == 0, 
                   "H2D slot not properly aligned at real address");
        TEST_ASSERT((d2h_slot_addr % _Alignof(message_slot_t)) == 0, 
                   "D2H slot not properly aligned at real address");
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_mailbox_slots_occupied_at_real_addresses(void)
{
    // Test slots_occupied array access at real IVSHMEM addresses
    uintptr_t shm_base = IVSHMEM_SHM_BASE;
    
    // Test H2D mailbox slots_occupied array
    uintptr_t h2d_mailbox_addr = shm_base + MBOX_H2D_OFFSET;
    mailbox_t *h2d_mailbox = (mailbox_t*)h2d_mailbox_addr;
    
    // Clear and test H2D slots_occupied
    memset(h2d_mailbox->slots_occupied, 0, sizeof(h2d_mailbox->slots_occupied));
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        h2d_mailbox->slots_occupied[i] = 0xAA + i;
    }
    
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "H2D slots_occupied[%u] access failed at real address", i);
        TEST_ASSERT(h2d_mailbox->slots_occupied[i] == (0xAA + i), msg);
    }
    
    // Test D2H mailbox slots_occupied array
    uintptr_t d2h_mailbox_addr = shm_base + MBOX_D2H_OFFSET;
    mailbox_t *d2h_mailbox = (mailbox_t*)d2h_mailbox_addr;
    
    // Clear and test D2H slots_occupied
    memset(d2h_mailbox->slots_occupied, 0, sizeof(d2h_mailbox->slots_occupied));
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        d2h_mailbox->slots_occupied[i] = 0xBB + i;
    }
    
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "D2H slots_occupied[%u] access failed at real address", i);
        TEST_ASSERT(d2h_mailbox->slots_occupied[i] == (0xBB + i), msg);
    }
    
    TEST_PASS_RESULT();
}

static const test_case_t mailbox_test_cases[] = {
    TEST_CASE(test_mailbox_structure_size),
    TEST_CASE(test_mailbox_alignment),
    TEST_CASE(test_mailbox_slot_count),
    TEST_CASE(test_mailbox_initialization),
    TEST_CASE(test_mailbox_slots_occupied_offset),
    TEST_CASE(test_mailbox_slots_occupied_access),
    TEST_CASE(test_mailbox_slots_occupied_independence),
    TEST_CASE(test_mailbox_slots_occupied_vs_slots_isolation),
    TEST_CASE(test_mailbox_slots_access),
    TEST_CASE(test_mailbox_circular_buffer_bounds),
    TEST_CASE(test_mailbox_message_slot_size),
    TEST_CASE(test_mailbox_memory_layout),
    // Real IVSHMEM address tests
    TEST_CASE(test_all_real_slot_addresses),
    TEST_CASE(test_payload_alignment_in_all_real_slots),
    TEST_CASE(test_payload_access_in_all_real_slots),
    TEST_CASE(test_real_slot_address_calculations),
    TEST_CASE(test_mailbox_alignment_at_real_addresses),
    TEST_CASE(test_mailbox_slots_occupied_at_real_addresses)
};

static const test_suite_t mailbox_test_suite = TEST_SUITE(
    "Mailbox Structure Sanity Check Tests",
    mailbox_test_cases,
    NULL,
    NULL
);

uint32_t run_mailbox_unit_tests(void)
{
    return test_run_suite(&mailbox_test_suite);
}
