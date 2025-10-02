#include "test_common.h"
#include "message_slot.h"
#include "message_id.h"
#include "message_payloads.h"

LOG_MODULE_REGISTER(unit_new_message_slot, LOG_LEVEL_DBG);

// Test fixtures
static message_slot_t test_slot;

static test_result_t test_message_slot_structure_layout(void) {
    LOG_DBG("Testing message slot structure layout");
    
    // Test structure size constraints
    TEST_ASSERT_TRUE(sizeof(message_slot_t) <= MESSAGE_SLOT_MAX_SIZE_BYTES, 
                     "message_slot_t should fit within max size");
    
    // Test field offsets (these are verified by static assertions in header, but test runtime too)
    TEST_ASSERT_EQ(0, offsetof(message_slot_t, msg_id), "msg_id should be at offset 0");
    TEST_ASSERT_EQ(sizeof(message_id_t), offsetof(message_slot_t, length), "length should follow msg_id");
    TEST_ASSERT_EQ(sizeof(message_id_t) + sizeof(uint32_t), offsetof(message_slot_t, data), "data should follow length");
    
    // Test alignment
    TEST_ASSERT_EQ(8, _Alignof(message_slot_t), "message_slot_t should be 8-byte aligned");
    
    LOG_DBG("Message slot structure layout test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_message_slot_basic_operations(void) {
    LOG_DBG("Testing message slot basic operations");
    
    // Clear test slot
    memset(&test_slot, 0, sizeof(test_slot));
    
    // Test: Basic field assignment
    test_slot.msg_id = MSG_PING;
    test_slot.length = sizeof(ping_t);
    
    // Set up ping data
    ping_t* ping_data = (ping_t*)test_slot.data;
    ping_data->timestamp = 12345;
    ping_data->reserved = 0;
    
    // Verify assignments
    TEST_ASSERT_EQ(MSG_PING, test_slot.msg_id, "Message type should be set correctly");
    TEST_ASSERT_EQ(sizeof(ping_t), test_slot.length, "Length should be set correctly");
    TEST_ASSERT_EQ(12345, ping_data->timestamp, "Ping timestamp should be set correctly");
    
    // Test: Maximum payload size
    test_slot.length = MESSAGE_SLOT_DATA_SIZE;
    memset(test_slot.data, 0xAA, MESSAGE_SLOT_DATA_SIZE);
    TEST_ASSERT_EQ(MESSAGE_SLOT_DATA_SIZE, test_slot.length, "Should handle maximum payload size");
    TEST_ASSERT_EQ(0xAA, test_slot.data[0], "First byte should be set");
    TEST_ASSERT_EQ(0xAA, test_slot.data[MESSAGE_SLOT_DATA_SIZE - 1], "Last byte should be set");
    
    LOG_DBG("Message slot basic operations test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_message_slot_payload_types(void) {
    LOG_DBG("Testing message slot with different payload types");
    
    // Test: MALLOC command payload
    memset(&test_slot, 0, sizeof(test_slot));
    test_slot.msg_id = MSG_CMD_MALLOC;
    test_slot.length = sizeof(malloc_cmd_t);
    
    malloc_cmd_t* malloc_cmd = (malloc_cmd_t*)test_slot.data;
    malloc_cmd->size = 1024;
    malloc_cmd->alignment = 8;
    malloc_cmd->reserved = 0;
    
    TEST_ASSERT_EQ(1024, malloc_cmd->size, "Malloc size should be set correctly");
    TEST_ASSERT_EQ(8, malloc_cmd->alignment, "Malloc alignment should be set correctly");
    
    // Test: FREE command payload
    test_slot.msg_id = MSG_CMD_FREE;
    test_slot.length = sizeof(free_cmd_t);
    
    free_cmd_t* free_cmd = (free_cmd_t*)test_slot.data;
    free_cmd->address = 0x12345678;
    free_cmd->reserved = 0;
    
    TEST_ASSERT_EQ(0x12345678, free_cmd->address, "Free address should be set correctly");
    
    // Test: LAUNCH command payload
    test_slot.msg_id = MSG_CMD_LAUNCH;
    test_slot.length = sizeof(launch_cmd_t);
    
    launch_cmd_t* launch_cmd = (launch_cmd_t*)test_slot.data;
    launch_cmd->kernel_address = 0xABCDEF00;
    launch_cmd->grid_x = 16;
    launch_cmd->grid_y = 16;
    launch_cmd->grid_z = 1;
    launch_cmd->block_x = 256;
    launch_cmd->block_y = 1;
    launch_cmd->block_z = 1;
    launch_cmd->args_address = 1024;
    
    TEST_ASSERT_EQ(0xABCDEF00, launch_cmd->kernel_address, "Kernel address should be set correctly");
    TEST_ASSERT_EQ(16, launch_cmd->grid_x, "Grid X should be set correctly");
    TEST_ASSERT_EQ(256, launch_cmd->block_x, "Block X should be set correctly");
    TEST_ASSERT_EQ(1024, launch_cmd->args_address, "Shared memory size should be set correctly");
    
    LOG_DBG("Message slot payload types test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_message_slot_batch_payloads(void) {
    LOG_DBG("Testing message slot with batch payloads");
    
    // Test: BATCH_BEGIN command
    memset(&test_slot, 0, sizeof(test_slot));
    test_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_slot.length = sizeof(batch_begin_cmd_t);
    
    batch_begin_cmd_t* begin_cmd = (batch_begin_cmd_t*)test_slot.data;
    begin_cmd->slot_count = 4;
    begin_cmd->batch_slots[0] = 0;
    begin_cmd->batch_slots[1] = 1;
    begin_cmd->batch_slots[2] = 2;
    begin_cmd->batch_slots[3] = 3;
    begin_cmd->reserved = 0;
    
    TEST_ASSERT_EQ(4, begin_cmd->slot_count, "Batch slot count should be set correctly");
    TEST_ASSERT_EQ(0, begin_cmd->batch_slots[0], "First batch slot should be 0");
    TEST_ASSERT_EQ(3, begin_cmd->batch_slots[3], "Last batch slot should be 3");
    
    // Test: BATCH_END command
    test_slot.msg_id = MSG_CMD_END_BATCH;
    test_slot.length = sizeof(batch_end_cmd_t);
    
    batch_end_cmd_t* end_cmd = (batch_end_cmd_t*)test_slot.data;
    end_cmd->slot_count = 4;
    memcpy(end_cmd->batch_slots, begin_cmd->batch_slots, sizeof(begin_cmd->batch_slots));
    end_cmd->reserved = 0;
    
    TEST_ASSERT_EQ(4, end_cmd->slot_count, "End slot count should match begin");
    TEST_ASSERT_MEM_EQ(begin_cmd->batch_slots, end_cmd->batch_slots, 4, "Batch slots should match");
    
    // Test: Internal invalidate command
    test_slot.msg_id = MSG_INTERNAL_INVALIDATE_SLOTS;
    test_slot.length = sizeof(internal_invalidate_slots_cmd_t);
    
    internal_invalidate_slots_cmd_t* inv_cmd = (internal_invalidate_slots_cmd_t*)test_slot.data;
    inv_cmd->slot_count = 2;
    inv_cmd->slots[0] = 5;
    inv_cmd->slots[1] = 6;
    inv_cmd->reserved = 0;
    
    TEST_ASSERT_EQ(2, inv_cmd->slot_count, "Invalidate slot count should be set");
    TEST_ASSERT_EQ(5, inv_cmd->slots[0], "First invalidate slot should be 5");
    TEST_ASSERT_EQ(6, inv_cmd->slots[1], "Second invalidate slot should be 6");
    
    LOG_DBG("Message slot batch payloads test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_message_slot_checksum_field(void) {
    LOG_DBG("Testing message slot checksum field");
    
    // Test: Checksum field access
    memset(&test_slot, 0, sizeof(test_slot));
    test_slot.checksum = 0x12345678;
    
    TEST_ASSERT_EQ(0x12345678, test_slot.checksum, "Checksum should be set correctly");
    
    // Test: Checksum field position
    size_t expected_checksum_offset = sizeof(message_id_t) + sizeof(uint32_t) + MESSAGE_SLOT_DATA_SIZE;
    TEST_ASSERT_EQ(expected_checksum_offset, offsetof(message_slot_t, checksum), 
                   "Checksum should be at end of structure");
    
    // Test: Full slot with checksum
    test_slot.msg_id = MSG_PING;
    test_slot.length = sizeof(ping_t);
    ping_t* ping_data = (ping_t*)test_slot.data;
    ping_data->timestamp = 98765;
    test_slot.checksum = 0xDEADBEEF;
    
    TEST_ASSERT_EQ(MSG_PING, test_slot.msg_id, "Message type should be preserved");
    TEST_ASSERT_EQ(sizeof(ping_t), test_slot.length, "Length should be preserved");
    TEST_ASSERT_EQ(98765, ping_data->timestamp, "Payload should be preserved");
    TEST_ASSERT_EQ(0xDEADBEEF, test_slot.checksum, "Checksum should be preserved");
    
    LOG_DBG("Message slot checksum field test passed");
    TEST_PASS_RESULT();
}

static const test_case_t message_slot_test_cases[] = {
    TEST_CASE(test_message_slot_structure_layout),
    TEST_CASE(test_message_slot_basic_operations),
    TEST_CASE(test_message_slot_payload_types),
    TEST_CASE(test_message_slot_batch_payloads),
    TEST_CASE(test_message_slot_checksum_field)
};

static void message_slot_setup(void) {
    LOG_DBG("Setting up message slot test environment");
    memset(&test_slot, 0, sizeof(test_slot));
}

static void message_slot_teardown(void) {
    LOG_DBG("Tearing down message slot test environment");
    // No cleanup needed for message slot tests
}

static const test_suite_t message_slot_suite = TEST_SUITE(
    "New Message Slot Tests",
    message_slot_test_cases,
    message_slot_setup,
    message_slot_teardown
);

void run_new_message_slot_tests(void) {
    LOG_INF("Running new message slot tests");
    test_run_suite(&message_slot_suite);
}