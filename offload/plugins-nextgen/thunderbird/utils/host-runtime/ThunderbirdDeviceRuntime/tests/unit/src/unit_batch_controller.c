#include "test_common.h"
#include "batch_controller.h"
#include "batch_state.h"
#include "mailbox_strategies.h"
#include "message_payloads.h"
#include "error_codes.h"

LOG_MODULE_REGISTER(unit_batch_controller, LOG_LEVEL_DBG);

// Test fixtures
static batch_begin_cmd_t test_begin_cmd;
static batch_end_cmd_t test_end_cmd;
static message_slot_t test_body_slot;
static message_slot_t test_begin_slot;
static message_slot_t test_end_slot;

static test_result_t test_batch_controller_strategy_access(void) {
    LOG_DBG("Testing batch controller strategy access");
    
    // Test: Get receive strategy
    receive_strategy_t recv_strategy = batch_controller_get_receive_strategy();
    // Strategy should have valid function pointer
    TEST_ASSERT_TRUE(recv_strategy.find_slot_fn != NULL, "Receive strategy should have valid function pointer");
    
    // Test: Get send strategy  
    send_strategy_t send_strategy = batch_controller_get_send_strategy();
    // Strategy should have valid function pointer
    TEST_ASSERT_TRUE(send_strategy.send_to_slot_fn != NULL, "Send strategy should have valid function pointer");
    
    LOG_DBG("Batch controller strategy access test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_controller_state_reset(void) {
    LOG_DBG("Testing batch controller state reset");
    
    // Test: Reset state
    error_code_t result = batch_controller_reset_state();
    TEST_ASSERT_EQ(ERR_OK, result, "State reset should succeed");
    
    // Verify batch is not active after reset
    bool is_active = batch_controller_is_batch_active();
    TEST_ASSERT_FALSE(is_active, "Batch should not be active after reset");
    
    LOG_DBG("Batch controller state reset test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_controller_begin_handling(void) {
    LOG_DBG("Testing batch controller begin handling");
    
    // Reset state first
    batch_controller_reset_state();
    
    // Setup test BEGIN command
    test_begin_cmd.slot_count = 4;
    test_begin_cmd.batch_slots[0] = 0;
    test_begin_cmd.batch_slots[1] = 1; 
    test_begin_cmd.batch_slots[2] = 2;
    test_begin_cmd.batch_slots[3] = 3;
    test_begin_cmd.reserved = 0;
    
    // Setup test BEGIN message slot
    test_begin_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_begin_slot.length = sizeof(batch_begin_cmd_t);
    test_begin_slot.checksum = 0;
    memcpy(test_begin_slot.data, &test_begin_cmd, sizeof(batch_begin_cmd_t));
    
    // Test: Valid begin command
    error_code_t result = batch_controller_handle_begin(&test_begin_cmd, 0, &test_begin_slot);
    TEST_ASSERT_EQ(ERR_OK, result, "Valid begin command should succeed");
    
    // Verify batch is now active
    bool is_active = batch_controller_is_batch_active();
    TEST_ASSERT_TRUE(is_active, "Batch should be active after begin");
    
    // Test: Duplicate begin command (should fail)
    result = batch_controller_handle_begin(&test_begin_cmd, 0, &test_begin_slot);
    TEST_ASSERT_NEQ(ERR_OK, result, "Duplicate begin should fail");
    
    // Test: NULL command parameter
    result = batch_controller_handle_begin(NULL, 0, &test_begin_slot);
    TEST_ASSERT_NEQ(ERR_OK, result, "NULL begin command should fail");
    
    LOG_DBG("Batch controller begin handling test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_controller_end_handling(void) {
    LOG_DBG("Testing batch controller end handling");
    
    // Reset and setup begin first
    batch_controller_reset_state();
    
    test_begin_cmd.slot_count = 3;
    test_begin_cmd.batch_slots[0] = 5;
    test_begin_cmd.batch_slots[1] = 6;
    test_begin_cmd.batch_slots[2] = 7;
    
    // Setup test BEGIN message slot
    test_begin_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_begin_slot.length = sizeof(batch_begin_cmd_t);
    test_begin_slot.checksum = 0;
    memcpy(test_begin_slot.data, &test_begin_cmd, sizeof(batch_begin_cmd_t));
    
    batch_controller_handle_begin(&test_begin_cmd, 5, &test_begin_slot);
    
    // Setup matching END command
    test_end_cmd.slot_count = 3;
    memcpy(test_end_cmd.batch_slots, test_begin_cmd.batch_slots, sizeof(test_begin_cmd.batch_slots));
    test_end_cmd.reserved = 0;
    
    // Setup test END message slot
    test_end_slot.msg_id = MSG_CMD_END_BATCH;
    test_end_slot.length = sizeof(batch_end_cmd_t);
    test_end_slot.checksum = 0;
    memcpy(test_end_slot.data, &test_end_cmd, sizeof(batch_end_cmd_t));
    
    // Test: Valid end command
    error_code_t result = batch_controller_handle_end(&test_end_cmd, 7, &test_end_slot);
    TEST_ASSERT_EQ(ERR_OK, result, "Valid end command should succeed");
    
    // Test: End without begin
    batch_controller_reset_state();
    result = batch_controller_handle_end(&test_end_cmd, 7, &test_end_slot);
    TEST_ASSERT_NEQ(ERR_OK, result, "End without begin should fail");
    
    // Test: Mismatched end command
    batch_controller_handle_begin(&test_begin_cmd, 5, &test_begin_slot);
    test_end_cmd.slot_count = 2; // Different from begin
    memcpy(test_end_slot.data, &test_end_cmd, sizeof(batch_end_cmd_t)); // Update slot data
    result = batch_controller_handle_end(&test_end_cmd, 7, &test_end_slot);
    TEST_ASSERT_NEQ(ERR_OK, result, "Mismatched end should fail");
    
    // Test: NULL command parameter
    result = batch_controller_handle_end(NULL, 7, &test_end_slot);
    TEST_ASSERT_NEQ(ERR_OK, result, "NULL end command should fail");
    
    LOG_DBG("Batch controller end handling test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_controller_body_handling(void) {
    LOG_DBG("Testing batch controller body handling");
    
    // Setup batch state: begin + end received
    batch_controller_reset_state();
    
    test_begin_cmd.slot_count = 4;
    test_begin_cmd.batch_slots[0] = 0;
    test_begin_cmd.batch_slots[1] = 1;
    test_begin_cmd.batch_slots[2] = 2;
    test_begin_cmd.batch_slots[3] = 3;
    
    // Setup test BEGIN message slot
    test_begin_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_begin_slot.length = sizeof(batch_begin_cmd_t);
    test_begin_slot.checksum = 0;
    memcpy(test_begin_slot.data, &test_begin_cmd, sizeof(batch_begin_cmd_t));
    
    batch_controller_handle_begin(&test_begin_cmd, 0, &test_begin_slot);
    
    test_end_cmd.slot_count = 4;
    memcpy(test_end_cmd.batch_slots, test_begin_cmd.batch_slots, sizeof(test_begin_cmd.batch_slots));
    
    // Setup test END message slot
    test_end_slot.msg_id = MSG_CMD_END_BATCH;
    test_end_slot.length = sizeof(batch_end_cmd_t);
    test_end_slot.checksum = 0;
    memcpy(test_end_slot.data, &test_end_cmd, sizeof(batch_end_cmd_t));
    
    batch_controller_handle_end(&test_end_cmd, 3, &test_end_slot);
    
    // Setup test body message
    test_body_slot.msg_id = MSG_CMD_MALLOC;  // Use the correct enum value
    test_body_slot.length = sizeof(malloc_cmd_t);
    malloc_cmd_t* malloc_cmd = (malloc_cmd_t*)test_body_slot.data;
    malloc_cmd->size = 1024;
    malloc_cmd->alignment = 8;
    malloc_cmd->reserved = 0;
    test_body_slot.checksum = 0;
    
    // Test: Valid body message in correct slot
    error_code_t result = batch_controller_handle_body_message(&test_body_slot, 1);
    TEST_ASSERT_EQ(ERR_OK, result, "Valid body message should succeed");
    
    // Test: Body message in wrong slot
    result = batch_controller_handle_body_message(&test_body_slot, 5);
    TEST_ASSERT_NEQ(ERR_OK, result, "Body in wrong slot should fail");
    
    // Test: Duplicate body message in same slot
    result = batch_controller_handle_body_message(&test_body_slot, 1);
    TEST_ASSERT_NEQ(ERR_OK, result, "Duplicate body message should fail");
    
    // Test: NULL message parameter
    result = batch_controller_handle_body_message(NULL, 2);
    TEST_ASSERT_NEQ(ERR_OK, result, "NULL body message should fail");
    
    LOG_DBG("Batch controller body handling test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_controller_state_access(void) {
    LOG_DBG("Testing batch controller state access");
    
    // Test: Peek at batch state
    const batch_state_t* state = batch_controller_peek_batch_state();
    TEST_ASSERT_NOT_NULL(state, "Batch state should be accessible");
    
    // Reset and verify state
    batch_controller_reset_state();
    state = batch_controller_peek_batch_state();
    TEST_ASSERT_NOT_NULL(state, "State should still be accessible after reset");
    
    // Test batch active status
    bool is_active = batch_controller_is_batch_active();
    TEST_ASSERT_FALSE(is_active, "Should not be active after reset");
    
    // Start batch and verify active status
    test_begin_cmd.slot_count = 2;
    test_begin_cmd.batch_slots[0] = 0;
    test_begin_cmd.batch_slots[1] = 1;
    
    // Setup test BEGIN message slot
    test_begin_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_begin_slot.length = sizeof(batch_begin_cmd_t);
    test_begin_slot.checksum = 0;
    memcpy(test_begin_slot.data, &test_begin_cmd, sizeof(batch_begin_cmd_t));
    
    batch_controller_handle_begin(&test_begin_cmd, 0, &test_begin_slot);
    
    is_active = batch_controller_is_batch_active();
    TEST_ASSERT_TRUE(is_active, "Should be active after begin");
    
    LOG_DBG("Batch controller state access test passed");
    TEST_PASS_RESULT();
}

static const test_case_t batch_controller_test_cases[] = {
    TEST_CASE(test_batch_controller_strategy_access),
    TEST_CASE(test_batch_controller_state_reset),
    TEST_CASE(test_batch_controller_begin_handling),
    TEST_CASE(test_batch_controller_end_handling),
    TEST_CASE(test_batch_controller_body_handling),
    TEST_CASE(test_batch_controller_state_access)
};

static void batch_controller_setup(void) {
    LOG_DBG("Setting up batch controller test environment");
    memset(&test_begin_cmd, 0, sizeof(test_begin_cmd));
    memset(&test_end_cmd, 0, sizeof(test_end_cmd));
    memset(&test_body_slot, 0, sizeof(test_body_slot));
}

static void batch_controller_teardown(void) {
    LOG_DBG("Tearing down batch controller test environment");
    // Reset state to clean up
    batch_controller_reset_state();
}

static const test_suite_t batch_controller_suite = TEST_SUITE(
    "Batch Controller Tests",
    batch_controller_test_cases,
    batch_controller_setup,
    batch_controller_teardown
);

void run_batch_controller_tests(void) {
    LOG_INF("Running batch controller tests");
    test_run_suite(&batch_controller_suite);
}