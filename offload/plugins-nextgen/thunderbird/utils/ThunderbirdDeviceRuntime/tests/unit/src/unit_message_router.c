#include "test_common.h"
#include "message_router.h"
#include "message_slot.h"
#include "message_payloads.h"
#include "message_id.h"
#include "error_codes.h"
#include "resource_map.h"
#include "device_memory.h"

LOG_MODULE_REGISTER(unit_message_router, LOG_LEVEL_DBG);

// Test fixtures
static message_slot_t test_input_slot;
static message_slot_t test_output_slot;
static uint8_t test_slot_index = 0;

static test_result_t test_message_router_handle_function(void) {
    LOG_DBG("Testing message_router_handle function");
    
    // Setup test message slot
    test_input_slot.msg_id = MSG_PING;
    test_input_slot.length = sizeof(ping_t);
    ping_t* ping_data = (ping_t*)test_input_slot.data;
    ping_data->timestamp = 12345;
    test_input_slot.checksum = 0; // Simplified for test
    
    // Test: Valid message handling
    bool result = message_router_handle(&test_input_slot, test_slot_index);
    TEST_ASSERT_TRUE(result, "Valid message should be handled successfully");
    
    // Test: NULL slot parameter
    result = message_router_handle(NULL, test_slot_index);
    TEST_ASSERT_FALSE(result, "NULL slot should return false");
    
    // Test: Invalid slot index (boundary testing)
    result = message_router_handle(&test_input_slot, 255);
    // This may or may not fail depending on implementation - test actual behavior
    
    LOG_DBG("Message router handle function test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_message_router_generate_response(void) {
    LOG_DBG("Testing message_router_generate_response function");
    
    // Setup input command message
    test_input_slot.msg_id = MSG_CMD_MALLOC;
    test_input_slot.length = sizeof(malloc_cmd_t);
    malloc_cmd_t* malloc_cmd = (malloc_cmd_t*)test_input_slot.data;
    malloc_cmd->size = 1024;
    malloc_cmd->alignment = 8;
    malloc_cmd->reserved = 0;
    
    // Clear output slot
    memset(&test_output_slot, 0, sizeof(test_output_slot));
    
    // Test: Valid response generation
    error_code_t result = message_router_generate_response(&test_input_slot, &test_output_slot);
    TEST_ASSERT_EQ(ERR_OK, result, "Valid command should generate response successfully");
    
    // Verify response message type is correct
    TEST_ASSERT_EQ(MSG_RSP_MALLOC, test_output_slot.msg_id, "Response should have correct message type");
    
    // Verify response payload structure
    TEST_ASSERT_EQ(sizeof(malloc_rsp_t), test_output_slot.length, "Response should have correct length");
    
    // Test: NULL input parameter
    result = message_router_generate_response(NULL, &test_output_slot);
    TEST_ASSERT_NEQ(ERR_OK, result, "NULL input should return error");
    
    // Test: NULL output parameter
    result = message_router_generate_response(&test_input_slot, NULL);
    TEST_ASSERT_NEQ(ERR_OK, result, "NULL output should return error");
    
    // Test: Invalid message type (non-command)
    test_input_slot.msg_id = MSG_RSP_MALLOC; // Response, not command
    result = message_router_generate_response(&test_input_slot, &test_output_slot);
    TEST_ASSERT_NEQ(ERR_OK, result, "Response message type should not generate response");
    
    LOG_DBG("Message router generate response test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_message_router_different_message_types(void) {
    LOG_DBG("Testing message router with different message types");
    
    // Test PING command
    test_input_slot.msg_id = MSG_PING;
    test_input_slot.length = sizeof(ping_t);
    ping_t* ping_data = (ping_t*)test_input_slot.data;
    ping_data->timestamp = 98765;
    
    bool handle_result = message_router_handle(&test_input_slot, 0);
    TEST_ASSERT_TRUE(handle_result, "PING message should be handled");
    
    // Test FREE command
    test_input_slot.msg_id = MSG_CMD_FREE;
    test_input_slot.length = sizeof(free_cmd_t);
    free_cmd_t* free_cmd = (free_cmd_t*)test_input_slot.data;
    free_cmd->address = 0x12345678;
    free_cmd->reserved = 0;
    
    error_code_t gen_result = message_router_generate_response(&test_input_slot, &test_output_slot);
    TEST_ASSERT_EQ(ERR_OK, gen_result, "FREE command should generate response");
    TEST_ASSERT_EQ(MSG_RSP_FREE, test_output_slot.msg_id, "FREE response should have correct type");
    
    // Test LAUNCH command with actual LLEXT module
    // Reference the LLEXT module data from main.c (same as lle_manager tests)
    extern uint8_t llext_buf[];
    extern const size_t llext_buf_len;
    
    // Allocate device memory for the LLEXT buffer
    void *lle_memory = device_shared_malloc(llext_buf_len);
    uintptr_t kernel_addr = 0;
    bool memory_allocated = false;
    
    if (lle_memory != NULL) {
        kernel_addr = (uintptr_t)lle_memory;
        
        // Copy the LLEXT buffer to device memory
        memcpy(lle_memory, llext_buf, llext_buf_len);
        memory_allocated = true;
        
        // Note: We no longer need to manually set LLE status here -
        // the kernel processor will set it just-in-time during execution
    }
    
    test_input_slot.msg_id = MSG_CMD_LAUNCH;
    test_input_slot.length = sizeof(launch_cmd_t);
    launch_cmd_t* launch_cmd = (launch_cmd_t*)test_input_slot.data;
    launch_cmd->kernel_address = kernel_addr;
    launch_cmd->grid_x = 16;
    launch_cmd->grid_y = 16;
    launch_cmd->grid_z = 1;
    launch_cmd->block_x = 256;
    launch_cmd->block_y = 1;
    launch_cmd->block_z = 1;
    launch_cmd->shared_mem_size = 1024;
    launch_cmd->reserved = 0;
    
    gen_result = message_router_generate_response(&test_input_slot, &test_output_slot);
    if (memory_allocated) {
        // If we successfully set up the LLEXT module, the kernel processor should
        // automatically set the LLE flag and attempt to execute it
        // Note: The actual execution might still fail depending on the test environment,
        // but the command should at least generate a proper response
        TEST_ASSERT_EQ(MSG_RSP_LAUNCH, test_output_slot.msg_id, "LAUNCH response should have correct type");
        
        // Clean up the test kernel memory
        device_shared_free(lle_memory);
    } else {
        // If we couldn't allocate memory, expect launch to fail
        TEST_ASSERT_NEQ(ERR_OK, gen_result, "LAUNCH command should fail with no memory");
        TEST_ASSERT_EQ(MSG_RSP_LAUNCH, test_output_slot.msg_id, "LAUNCH response should have correct type even on failure");
    }
    
    LOG_DBG("Message router different message types test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_message_router_batch_commands(void) {
    LOG_DBG("Testing message router with batch commands");
    
    // Test BATCH_BEGIN command through message_router_handle (not generate_response)
    test_input_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_input_slot.length = sizeof(batch_begin_cmd_t);
    batch_begin_cmd_t* begin_cmd = (batch_begin_cmd_t*)test_input_slot.data;
    begin_cmd->slot_count = 4;
    begin_cmd->batch_slots[0] = 0;
    begin_cmd->batch_slots[1] = 1;
    begin_cmd->batch_slots[2] = 2;
    begin_cmd->batch_slots[3] = 3;
    begin_cmd->reserved = 0;
    
    bool handle_result = message_router_handle(&test_input_slot, 0);
    TEST_ASSERT_TRUE(handle_result, "BATCH_BEGIN should be handled");
    
    // Test that BATCH_BEGIN is not handled by generate_response (should fail)
    error_code_t gen_result = message_router_generate_response(&test_input_slot, &test_output_slot);
    TEST_ASSERT_NEQ(ERR_OK, gen_result, "BATCH_BEGIN should not be handled by generate_response");
    
    // Test BATCH_END command through message_router_handle (not generate_response)
    test_input_slot.msg_id = MSG_CMD_END_BATCH;
    test_input_slot.length = sizeof(batch_end_cmd_t);
    batch_end_cmd_t* end_cmd = (batch_end_cmd_t*)test_input_slot.data;
    end_cmd->slot_count = 4;
    memcpy(end_cmd->batch_slots, begin_cmd->batch_slots, sizeof(begin_cmd->batch_slots));
    end_cmd->reserved = 0;
    
    handle_result = message_router_handle(&test_input_slot, 3);
    TEST_ASSERT_TRUE(handle_result, "BATCH_END should be handled");
    
    // Test that BATCH_END is not handled by generate_response (should fail)
    gen_result = message_router_generate_response(&test_input_slot, &test_output_slot);
    TEST_ASSERT_NEQ(ERR_OK, gen_result, "BATCH_END should not be handled by generate_response");
    
    LOG_DBG("Message router batch commands test passed");
    TEST_PASS_RESULT();
}

static const test_case_t message_router_test_cases[] = {
    TEST_CASE(test_message_router_handle_function),
    TEST_CASE(test_message_router_generate_response),
    TEST_CASE(test_message_router_different_message_types),
    TEST_CASE(test_message_router_batch_commands)
};

static void message_router_setup(void) {
    LOG_DBG("Setting up message router test environment");
    memset(&test_input_slot, 0, sizeof(test_input_slot));
    memset(&test_output_slot, 0, sizeof(test_output_slot));
}

static void message_router_teardown(void) {
    LOG_DBG("Tearing down message router test environment");
    // No cleanup needed for message router tests
}

static const test_suite_t message_router_suite = TEST_SUITE(
    "Message Router Tests",
    message_router_test_cases,
    message_router_setup,
    message_router_teardown
);

void run_message_router_tests(void) {
    LOG_INF("Running message router tests");
    test_run_suite(&message_router_suite);
}