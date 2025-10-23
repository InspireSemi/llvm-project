#include "test_common.h"
#include "batch_state.h"
#include "message_payloads.h"
#include "error_codes.h"

LOG_MODULE_REGISTER(unit_batch_state, LOG_LEVEL_DBG);

// Test fixtures
static batch_state_t test_batch_state;
static batch_begin_cmd_t test_begin_cmd;
static batch_end_cmd_t test_end_cmd;
static message_slot_t test_begin_slot;
static message_slot_t test_end_slot;

static test_result_t test_batch_state_initialization(void) {
    LOG_DBG("Testing batch state initialization");
    
    // Test: Valid initialization
    error_code_t result = batch_state_init(&test_batch_state);
    TEST_ASSERT_EQ(ERR_OK, result, "Batch state init should succeed");
    TEST_ASSERT_EQ(BATCH_STATE_IDLE, batch_state_get_current(&test_batch_state), "Initial state should be IDLE");
    
    // Test: NULL parameter
    result = batch_state_init(NULL);
    TEST_ASSERT_NEQ(ERR_OK, result, "NULL parameter should fail");
    
    // Verify initial state properties
    TEST_ASSERT_TRUE(batch_state_can_accept_begin(&test_batch_state), "IDLE state should accept BEGIN");
    TEST_ASSERT_FALSE(batch_state_can_accept_end(&test_batch_state), "IDLE state should not accept END");
    TEST_ASSERT_FALSE(batch_state_can_accept_body(&test_batch_state), "IDLE state should not accept BODY");
    TEST_ASSERT_FALSE(batch_state_is_active(&test_batch_state), "IDLE state should not be active");
    TEST_ASSERT_FALSE(batch_state_is_complete(&test_batch_state), "IDLE state should not be complete");
    TEST_ASSERT_FALSE(batch_state_is_ready_to_process(&test_batch_state), "IDLE state should not be ready to process");
    
    LOG_DBG("Batch state initialization test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_state_transitions(void) {
    LOG_DBG("Testing batch state transitions");
    
    // Initialize state
    batch_state_init(&test_batch_state);
    
    // Setup test BEGIN command
    test_begin_cmd.slot_count = 4;
    test_begin_cmd.batch_slots[0] = 0;
    test_begin_cmd.batch_slots[1] = 1;
    test_begin_cmd.batch_slots[2] = 2;
    test_begin_cmd.batch_slots[3] = 3;
    
    // Setup test BEGIN message slot
    test_begin_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_begin_slot.length = sizeof(batch_begin_cmd_t);
    memcpy(test_begin_slot.data, &test_begin_cmd, sizeof(test_begin_cmd));
    
    batch_begin_event_data_t begin_data = {
        .cmd = &test_begin_cmd,
        .message_slot = &test_begin_slot,
        .slot_index = 0
    };
    
    // Test: IDLE -> WAITING_FOR_END transition
    error_code_t result = batch_state_transition(&test_batch_state, BATCH_EVENT_BEGIN_RECEIVED, &begin_data);
    TEST_ASSERT_EQ(ERR_OK, result, "BEGIN transition should succeed");
    TEST_ASSERT_EQ(BATCH_STATE_WAITING_FOR_END, batch_state_get_current(&test_batch_state), "Should transition to WAITING_FOR_END");
    
    // Verify state properties after BEGIN
    TEST_ASSERT_FALSE(batch_state_can_accept_begin(&test_batch_state), "Should not accept another BEGIN");
    TEST_ASSERT_TRUE(batch_state_can_accept_end(&test_batch_state), "Should accept END");
    TEST_ASSERT_FALSE(batch_state_can_accept_body(&test_batch_state), "Should not accept BODY yet");
    TEST_ASSERT_TRUE(batch_state_is_active(&test_batch_state), "Should be active");
    
    // Setup test END command
    test_end_cmd.slot_count = 4;
    memcpy(test_end_cmd.batch_slots, test_begin_cmd.batch_slots, sizeof(test_begin_cmd.batch_slots));
    
    // Setup test END message slot
    test_end_slot.msg_id = MSG_CMD_END_BATCH;
    test_end_slot.length = sizeof(batch_end_cmd_t);
    memcpy(test_end_slot.data, &test_end_cmd, sizeof(test_end_cmd));
    
    batch_end_event_data_t end_data = {
        .cmd = &test_end_cmd,
        .message_slot = &test_end_slot,
        .slot_index = 3
    };
    
    // Test: WAITING_FOR_END -> COLLECTING_BODY transition
    result = batch_state_transition(&test_batch_state, BATCH_EVENT_END_RECEIVED, &end_data);
    TEST_ASSERT_EQ(ERR_OK, result, "END transition should succeed");
    TEST_ASSERT_EQ(BATCH_STATE_COLLECTING_BODY, batch_state_get_current(&test_batch_state), "Should transition to COLLECTING_BODY");
    
    // Verify state properties after END
    TEST_ASSERT_TRUE(batch_state_can_accept_body(&test_batch_state), "Should accept BODY messages");
    TEST_ASSERT_FALSE(batch_state_can_accept_begin(&test_batch_state), "Should not accept BEGIN");
    TEST_ASSERT_FALSE(batch_state_can_accept_end(&test_batch_state), "Should not accept another END");
    
    // Test invalid transitions
    result = batch_state_transition(&test_batch_state, BATCH_EVENT_BEGIN_RECEIVED, &begin_data);
    TEST_ASSERT_NEQ(ERR_OK, result, "BEGIN should not be accepted in COLLECTING_BODY state");
    
    LOG_DBG("Batch state transitions test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_state_body_collection(void) {
    LOG_DBG("Testing batch state body collection");
    
    // Setup state in COLLECTING_BODY
    batch_state_init(&test_batch_state);
    
    test_begin_cmd.slot_count = 4; // BEGIN + END + 2 BODY
    test_begin_cmd.batch_slots[0] = 0;
    test_begin_cmd.batch_slots[1] = 1;
    test_begin_cmd.batch_slots[2] = 2;
    test_begin_cmd.batch_slots[3] = 3;
    
    // Setup test BEGIN message slot
    test_begin_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_begin_slot.length = sizeof(batch_begin_cmd_t);
    memcpy(test_begin_slot.data, &test_begin_cmd, sizeof(test_begin_cmd));
    
    batch_begin_event_data_t begin_data = {
        .cmd = &test_begin_cmd, 
        .message_slot = &test_begin_slot,
        .slot_index = 0
    };
    batch_state_transition(&test_batch_state, BATCH_EVENT_BEGIN_RECEIVED, &begin_data);
    
    test_end_cmd.slot_count = 4;
    memcpy(test_end_cmd.batch_slots, test_begin_cmd.batch_slots, sizeof(test_begin_cmd.batch_slots));
    
    // Setup test END message slot
    test_end_slot.msg_id = MSG_CMD_END_BATCH;
    test_end_slot.length = sizeof(batch_end_cmd_t);
    memcpy(test_end_slot.data, &test_end_cmd, sizeof(test_end_cmd));
    
    batch_end_event_data_t end_data = {
        .cmd = &test_end_cmd, 
        .message_slot = &test_end_slot,
        .slot_index = 3
    };
    batch_state_transition(&test_batch_state, BATCH_EVENT_END_RECEIVED, &end_data);
    
    TEST_ASSERT_EQ(BATCH_STATE_COLLECTING_BODY, batch_state_get_current(&test_batch_state), "Should be in COLLECTING_BODY state");
    
    // Test that we can now get the next body slot when in COLLECTING_BODY state
    uint8_t next_slot = batch_state_get_next_body_slot(&test_batch_state);
    TEST_ASSERT_EQ(1, next_slot, "First body slot should be slot 1");
    
    // Test body message collection
    message_slot_t body_slot = {0};
    body_slot.msg_id = MSG_CMD_MALLOC;
    body_slot.length = sizeof(malloc_cmd_t);
    
    body_event_data_t body_data = {.message = &body_slot, .slot_index = 1};
    
    // Collect first body message
    error_code_t result = batch_state_transition(&test_batch_state, BATCH_EVENT_BODY_RECEIVED, &body_data);
    TEST_ASSERT_EQ(ERR_OK, result, "First body message should be accepted");
    TEST_ASSERT_EQ(BATCH_STATE_COLLECTING_BODY, batch_state_get_current(&test_batch_state), "Should remain in COLLECTING_BODY");
    
    // Test that next body slot has advanced
    next_slot = batch_state_get_next_body_slot(&test_batch_state);
    TEST_ASSERT_EQ(2, next_slot, "Second body slot should be slot 2");
    
    // Collect second (final) body message
    body_data.slot_index = 2;
    result = batch_state_transition(&test_batch_state, BATCH_EVENT_BODY_RECEIVED, &body_data);
    TEST_ASSERT_EQ(ERR_OK, result, "Second body message should be accepted");
    
    // Test that no more body slots are available
    next_slot = batch_state_get_next_body_slot(&test_batch_state);
    TEST_ASSERT_EQ(0xFF, next_slot, "No more body slots should be available after collecting all");
    
    // Should have automatically transitioned to READY_TO_PROCESS after collecting all body messages
    TEST_ASSERT_EQ(BATCH_STATE_READY_TO_PROCESS, batch_state_get_current(&test_batch_state), "Should be READY_TO_PROCESS");
    TEST_ASSERT_TRUE(batch_state_is_ready_to_process(&test_batch_state), "Should be ready to process");
    
    LOG_DBG("Batch state body collection test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_state_context_access(void) {
    LOG_DBG("Testing batch state context access");
    
    // Setup state with some data
    batch_state_init(&test_batch_state);
    
    test_begin_cmd.slot_count = 3;
    test_begin_cmd.batch_slots[0] = 5;
    test_begin_cmd.batch_slots[1] = 6;
    test_begin_cmd.batch_slots[2] = 7;
    
    // Setup test BEGIN message slot
    test_begin_slot.msg_id = MSG_CMD_BEGIN_BATCH;
    test_begin_slot.length = sizeof(batch_begin_cmd_t);
    memcpy(test_begin_slot.data, &test_begin_cmd, sizeof(test_begin_cmd));
    
    batch_begin_event_data_t begin_data = {
        .cmd = &test_begin_cmd, 
        .message_slot = &test_begin_slot,
        .slot_index = 5
    };
    batch_state_transition(&test_batch_state, BATCH_EVENT_BEGIN_RECEIVED, &begin_data);
    
    // Test context access
    const batch_context_t* context = batch_state_get_context(&test_batch_state);
    TEST_ASSERT_NOT_NULL(context, "Context should be accessible");
    TEST_ASSERT_EQ(3, context->total_messages, "Total messages should match");
    TEST_ASSERT_EQ(5, context->begin_message_slot, "Begin slot should match");
    TEST_ASSERT_EQ(1, context->expected_body_count, "Expected body count should be total - 2");
    TEST_ASSERT_EQ(0, context->collected_body_count, "No body messages collected yet");
    
    // Test next body slot function - should return invalid since we're not in COLLECTING_BODY state yet
    uint8_t next_slot = batch_state_get_next_body_slot(&test_batch_state);
    TEST_ASSERT_EQ(0xFF, next_slot, "Next body slot should be invalid before END is received");
    
    LOG_DBG("Batch state context access test passed");
    TEST_PASS_RESULT();
}

static test_result_t test_batch_state_error_handling(void) {
    LOG_DBG("Testing batch state error handling");
    
    // Test: NULL state parameter
    error_code_t result = batch_state_transition(NULL, BATCH_EVENT_BEGIN_RECEIVED, NULL);
    TEST_ASSERT_NEQ(ERR_OK, result, "NULL state should return error");
    
    // Test: Invalid event
    batch_state_init(&test_batch_state);
    result = batch_state_transition(&test_batch_state, (batch_event_t)99, NULL);
    TEST_ASSERT_NEQ(ERR_OK, result, "Invalid event should return error");
    
    // Test: Error event puts state machine into ERROR state (sticky)
    result = batch_state_transition(&test_batch_state, BATCH_EVENT_ERROR, NULL);
    TEST_ASSERT_EQ(ERR_OK, result, "Error event should be handled");
    TEST_ASSERT_EQ(BATCH_STATE_ERROR, batch_state_get_current(&test_batch_state), "Error should set ERROR state");
    
    // Test: Reset event
    result = batch_state_transition(&test_batch_state, BATCH_EVENT_RESET, NULL);
    TEST_ASSERT_EQ(ERR_OK, result, "Reset event should be handled");
    TEST_ASSERT_EQ(BATCH_STATE_IDLE, batch_state_get_current(&test_batch_state), "Reset should set to IDLE");
    
    LOG_DBG("Batch state error handling test passed");
    TEST_PASS_RESULT();
}

static const test_case_t batch_state_test_cases[] = {
    TEST_CASE(test_batch_state_initialization),
    TEST_CASE(test_batch_state_transitions),
    TEST_CASE(test_batch_state_body_collection),
    TEST_CASE(test_batch_state_context_access),
    TEST_CASE(test_batch_state_error_handling)
};

static void batch_state_setup(void) {
    LOG_DBG("Setting up batch state test environment");
    memset(&test_batch_state, 0, sizeof(test_batch_state));
    memset(&test_begin_cmd, 0, sizeof(test_begin_cmd));
    memset(&test_end_cmd, 0, sizeof(test_end_cmd));
    memset(&test_begin_slot, 0, sizeof(test_begin_slot));
    memset(&test_end_slot, 0, sizeof(test_end_slot));
}

static void batch_state_teardown(void) {
    LOG_DBG("Tearing down batch state test environment");
    // No cleanup needed for batch state tests
}

static const test_suite_t batch_state_suite = TEST_SUITE(
    "Batch State Tests",
    batch_state_test_cases,
    batch_state_setup,
    batch_state_teardown
);

void run_batch_state_tests(void) {
    LOG_INF("Running batch state tests");
    test_run_suite(&batch_state_suite);
}