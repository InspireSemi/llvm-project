#include "batch_state.h"
#include "message_payloads.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(batch_state, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief State transition table - enforces strict BEGIN->END->BODY ordering
 */
typedef struct {
    batch_state_enum_t from_state;
    batch_event_t event;
    batch_state_enum_t to_state;
    bool is_valid;
} state_transition_t;

static const state_transition_t transition_table[] = {
    // From IDLE - only BEGIN allowed
    {BATCH_STATE_IDLE, BATCH_EVENT_BEGIN_RECEIVED, BATCH_STATE_WAITING_FOR_END, true},
    
    // From WAITING_FOR_END - only END allowed
    {BATCH_STATE_WAITING_FOR_END, BATCH_EVENT_END_RECEIVED, BATCH_STATE_COLLECTING_BODY, true},
    
    // From COLLECTING_BODY - body messages or completion
    // NOTE: BATCH_EVENT_ALL_BODY_COLLECTED is ONLY triggered by handle_body_event
    // when the last expected body message arrives. This enforces strict BEGIN->END->BODY ordering.
    {BATCH_STATE_COLLECTING_BODY, BATCH_EVENT_BODY_RECEIVED, BATCH_STATE_COLLECTING_BODY, true},
    {BATCH_STATE_COLLECTING_BODY, BATCH_EVENT_ALL_BODY_COLLECTED, BATCH_STATE_READY_TO_PROCESS, true},
    
    // From READY_TO_PROCESS - only processing start allowed
    {BATCH_STATE_READY_TO_PROCESS, BATCH_EVENT_PROCESSING_STARTED, BATCH_STATE_PROCESSING, true},
    
    // From PROCESSING
    {BATCH_STATE_PROCESSING, BATCH_EVENT_PROCESSING_COMPLETE, BATCH_STATE_COMPLETE, true},
    
    // COMPLETE automatically transitions to IDLE (brief completion state)
    // No direct transitions from COMPLETE - it should auto-transition to IDLE
    
    // Reset transitions from any state
    {BATCH_STATE_IDLE, BATCH_EVENT_RESET, BATCH_STATE_IDLE, true},
    {BATCH_STATE_WAITING_FOR_END, BATCH_EVENT_RESET, BATCH_STATE_IDLE, true},
    {BATCH_STATE_COLLECTING_BODY, BATCH_EVENT_RESET, BATCH_STATE_IDLE, true},
    {BATCH_STATE_READY_TO_PROCESS, BATCH_EVENT_RESET, BATCH_STATE_IDLE, true},
    {BATCH_STATE_PROCESSING, BATCH_EVENT_RESET, BATCH_STATE_IDLE, true},
    {BATCH_STATE_COMPLETE, BATCH_EVENT_RESET, BATCH_STATE_IDLE, true},
    {BATCH_STATE_ERROR, BATCH_EVENT_RESET, BATCH_STATE_IDLE, true},
    
    // Error transitions from any state - errors are sticky until reset
    {BATCH_STATE_IDLE, BATCH_EVENT_ERROR, BATCH_STATE_ERROR, true},
    {BATCH_STATE_WAITING_FOR_END, BATCH_EVENT_ERROR, BATCH_STATE_ERROR, true},
    {BATCH_STATE_COLLECTING_BODY, BATCH_EVENT_ERROR, BATCH_STATE_ERROR, true},
    {BATCH_STATE_READY_TO_PROCESS, BATCH_EVENT_ERROR, BATCH_STATE_ERROR, true},
    {BATCH_STATE_PROCESSING, BATCH_EVENT_ERROR, BATCH_STATE_ERROR, true},
    {BATCH_STATE_COMPLETE, BATCH_EVENT_ERROR, BATCH_STATE_ERROR, true},
    {BATCH_STATE_ERROR, BATCH_EVENT_ERROR, BATCH_STATE_ERROR, true},
};

static const size_t transition_table_size = sizeof(transition_table) / sizeof(transition_table[0]);

/**
 * @brief Find valid state transition
 */
static batch_state_enum_t find_next_state(batch_state_enum_t current_state, batch_event_t event)
{
    for (size_t i = 0; i < transition_table_size; i++) {
        if (transition_table[i].from_state == current_state && 
            transition_table[i].event == event && 
            transition_table[i].is_valid) {
            return transition_table[i].to_state;
        }
    }
    return current_state; // No valid transition found
}

/**
 * @brief Handle BEGIN event
 */
static error_code_t handle_begin_event(batch_state_t* state, const void* event_data)
{
    LOG_DBG("handle_begin_event: entry");
    
    const batch_begin_event_data_t* data = (const batch_begin_event_data_t*)event_data;
    if (!data) {
        LOG_ERR("handle_begin_event: event_data is NULL");
        return ERR_INVALID_PARAM;
    }
    
    if (!data->cmd) {
        LOG_ERR("handle_begin_event: data->cmd is NULL");
        return ERR_INVALID_PARAM;
    }
    
    if (!data->message_slot) {
        LOG_ERR("handle_begin_event: data->message_slot is NULL");  // ← This is probably the issue!
        return ERR_INVALID_PARAM;
    }
    
    LOG_DBG("handle_begin_event: all pointers valid, proceeding");
    
    // Reset context for new batch
    memset(&state->context, 0, sizeof(batch_context_t));
    state->context.total_messages = data->cmd->slot_count;
    state->context.expected_body_count = data->cmd->slot_count - 2; // Exclude BEGIN and END
    state->context.begin_message_slot = data->slot_index;
    state->context.next_body_index = 1; // Start at index 1 (first body slot, after BEGIN)
    
    // Store the valid batch slots
    memcpy(state->context.batch_slots, data->cmd->batch_slots, 
           data->cmd->slot_count * sizeof(uint8_t));
    
    // Copy the ENTIRE message slot to batch buffer (not just the command!)
    memcpy((void*)&state->context.batch_buffer.slots[data->slot_index], 
           data->message_slot, sizeof(message_slot_t));  // ← FIX: Copy entire slot
    state->context.batch_buffer.slots_occupied[data->slot_index] = 1;
    
    LOG_DBG("BEGIN event: expecting %u total messages (%u body)", 
            state->context.total_messages, state->context.expected_body_count);
    return ERR_OK;
}

/**
 * @brief Handle END event
 */
static error_code_t handle_end_event(batch_state_t* state, const void* event_data)
{
    const batch_end_event_data_t* data = (const batch_end_event_data_t*)event_data;
    if (!data || !data->cmd || !data->message_slot) {  // ← Need message_slot too!
        return ERR_INVALID_PARAM;
    }
    
    state->context.end_message_slot = data->slot_index;
    
    // Copy the ENTIRE message slot to batch buffer (not just the command!)
    memcpy((void*)&state->context.batch_buffer.slots[data->slot_index], 
           data->message_slot, sizeof(message_slot_t));  // ← FIX: Copy entire slot
    state->context.batch_buffer.slots_occupied[data->slot_index] = 1;
    
    LOG_DBG("END event received at slot %u", data->slot_index);
    
    // END message received - now we can start collecting body messages
    // Do NOT check for completion here - only body messages can complete the batch
    LOG_DBG("END message stored, now ready to collect %u body messages", 
            state->context.expected_body_count);
    
    return ERR_OK;
}

/**
 * @brief Check if a slot index is valid for the current batch
 */
static bool is_valid_batch_slot(const batch_state_t* state, uint8_t slot_index)
{
    if (!state) {
        return false;
    }
    
    // Check if slot_index is in the batch_slots array
    for (uint8_t i = 0; i < state->context.total_messages; i++) {
        if (state->context.batch_slots[i] == slot_index) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Handle BODY event
 */
static error_code_t handle_body_event(batch_state_t* state, const void* event_data)
{
    const body_event_data_t* data = (const body_event_data_t*)event_data;
    if (!data || !data->message) {
        return ERR_INVALID_PARAM;
    }
    
    // Validate that the slot index is part of this batch
    if (!is_valid_batch_slot(state, data->slot_index)) {
        LOG_ERR("BODY event: slot %u is not valid for this batch", data->slot_index);
        return ERR_INVALID_PARAM;
    }
    
    // Check if this slot already has a body message
    if (state->context.batch_buffer.slots_occupied[data->slot_index]) {
        LOG_ERR("BODY event: slot %u already occupied", data->slot_index);
        return ERR_BODY_SLOT_ALREADY_RECEIVED;
    }
    
    // Copy body message to batch buffer
    memcpy((void*)&state->context.batch_buffer.slots[data->slot_index], 
           data->message, sizeof(message_slot_t));
    state->context.batch_buffer.slots_occupied[data->slot_index] = 1;
    
    state->context.collected_body_count++;
    state->context.next_body_index++;
    
    LOG_DBG("BODY event: collected %u/%u body messages", 
            state->context.collected_body_count, 
            state->context.expected_body_count);
    
    // Check if all body messages collected
    if (state->context.collected_body_count >= state->context.expected_body_count) {
        // Trigger completion event and STOP - don't continue with BODY_RECEIVED transition
        error_code_t result = batch_state_transition(state, BATCH_EVENT_ALL_BODY_COLLECTED, NULL);
        LOG_DBG("All body messages collected - batch ready for processing");
        return result;
    }
    
    // Only return ERR_OK if we're staying in COLLECTING_BODY
    return ERR_OK;
}

error_code_t batch_state_init(batch_state_t* state)
{
    if (!state) {
        return ERR_INVALID_PARAM;
    }
    
    memset(state, 0, sizeof(batch_state_t));
    state->current_state = BATCH_STATE_IDLE;
    
    LOG_DBG("Batch state machine initialized");
    return ERR_OK;
}

error_code_t batch_state_transition(batch_state_t* state, batch_event_t event, const void* event_data)
{
    if (!state) {
        return ERR_INVALID_PARAM;
    }
    
    batch_state_enum_t old_state = state->current_state;
    batch_state_enum_t new_state = find_next_state(old_state, event);
    
    // Check for invalid transitions (state didn't change when it should have)
    if (new_state == old_state && event != BATCH_EVENT_BODY_RECEIVED && 
        event != BATCH_EVENT_RESET && event != BATCH_EVENT_ERROR) {
        LOG_ERR("INVALID TRANSITION DETECTED: state=%d, event=%d (possible malfunction)", old_state, event);
        LOG_ERR("This should not happen in normal operation - forcing error state");
        
        // Force error state for invalid transitions
        state->current_state = BATCH_STATE_ERROR;
        return ERR_INVALID_BATCH_STATE;
    }
    
    // Handle event-specific logic before state change
    error_code_t result = ERR_OK;
    switch (event) {
        case BATCH_EVENT_BEGIN_RECEIVED:
            result = handle_begin_event(state, event_data);
            break;
            
        case BATCH_EVENT_END_RECEIVED:
            result = handle_end_event(state, event_data);
            break;
            
        case BATCH_EVENT_BODY_RECEIVED: {
            batch_state_enum_t state_before_body_handling = state->current_state;
            result = handle_body_event(state, event_data);
            
            // If handle_body_event changed the state (due to ALL_BODY_COLLECTED), 
            // don't apply the BODY_RECEIVED transition
            if (state->current_state != state_before_body_handling) {
                LOG_DBG("Body event handler changed state, skipping BODY_RECEIVED transition");
                return result; // Exit early, don't apply state change below
            }
            break;
        }
            
        case BATCH_EVENT_RESET:
            memset(&state->context, 0, sizeof(batch_context_t));
            LOG_INF("Batch state machine reset");
            break;
            
        case BATCH_EVENT_PROCESSING_STARTED:
            LOG_INF("Batch processing started");
            break;
            
        case BATCH_EVENT_PROCESSING_COMPLETE:
            LOG_INF("Batch processing completed");
            break;
            
        case BATCH_EVENT_ERROR:
            LOG_ERR("Batch state machine error event");
            break;
            
        default:
            break;
    }
    
    if (result == ERR_OK) {
        state->current_state = new_state;
        LOG_DBG("State transition: %d -> %d (event=%d) [%p]", old_state, new_state, event, state);
    } else {
        // Force error state on event handling failure
        state->current_state = BATCH_STATE_ERROR;
        LOG_ERR("Event handling failed, forced to error state");
    }
    
    return result;
}

// Query functions
batch_state_enum_t batch_state_get_current(const batch_state_t* state)
{
    return state ? state->current_state : BATCH_STATE_ERROR;
}

bool batch_state_is_active(const batch_state_t* state)
{
    if (!state) return false;
    
    return (state->current_state != BATCH_STATE_IDLE && 
            state->current_state != BATCH_STATE_COMPLETE &&
            state->current_state != BATCH_STATE_ERROR);
}

bool batch_state_is_complete(const batch_state_t* state)
{
    return state && (state->current_state == BATCH_STATE_COMPLETE);
}

bool batch_state_can_accept_begin(const batch_state_t* state)
{
    return state && (state->current_state == BATCH_STATE_IDLE);
}

bool batch_state_can_accept_end(const batch_state_t* state)
{
    return state && (state->current_state == BATCH_STATE_WAITING_FOR_END);
}

bool batch_state_can_accept_body(const batch_state_t* state)
{
    return state && (state->current_state == BATCH_STATE_COLLECTING_BODY);
}

bool batch_state_is_ready_to_process(const batch_state_t* state)
{
    return state && (state->current_state == BATCH_STATE_READY_TO_PROCESS);
}

const batch_context_t* batch_state_get_context(const batch_state_t* state)
{
    return state ? &state->context : NULL;
}

uint8_t batch_state_get_next_body_slot(const batch_state_t* state)
{
    if (!state || !batch_state_can_accept_body(state)) {
        return 0xFF; // Invalid slot - not ready to collect body messages
    }
    
    // next_body_index directly points to the slot in batch_slots array to collect next
    if (state->context.next_body_index >= state->context.total_messages - 1) {
        return 0xFF; // Would be accessing END slot or beyond
    }
    
    return state->context.batch_slots[state->context.next_body_index];
}