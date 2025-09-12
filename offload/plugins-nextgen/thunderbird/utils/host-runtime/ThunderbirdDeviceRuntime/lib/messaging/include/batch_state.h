#pragma once

#include "message_slot.h"
#include "mailbox.h"
#include "error_codes.h"
#include "message_payloads.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Batch processing states - enforces BEGIN->END->BODY ordering
 */
typedef enum {
    BATCH_STATE_IDLE = 0,           ///< No active batch, waiting for BEGIN
    BATCH_STATE_WAITING_FOR_END,    ///< Have BEGIN, waiting for END
    BATCH_STATE_COLLECTING_BODY,    ///< Have BEGIN+END, collecting body messages
    BATCH_STATE_READY_TO_PROCESS,   ///< All messages collected, ready to process
    BATCH_STATE_PROCESSING,         ///< Currently processing batch
    BATCH_STATE_COMPLETE,           ///< Batch processing complete
    BATCH_STATE_ERROR               ///< Error state - will auto-reset to IDLE
} batch_state_enum_t;

/**
 * @brief Batch processing events
 */
typedef enum {
    BATCH_EVENT_BEGIN_RECEIVED = 0,
    BATCH_EVENT_END_RECEIVED,
    BATCH_EVENT_BODY_RECEIVED,
    BATCH_EVENT_ALL_BODY_COLLECTED,
    BATCH_EVENT_PROCESSING_STARTED,
    BATCH_EVENT_PROCESSING_COMPLETE,
    BATCH_EVENT_RESET,
    BATCH_EVENT_ERROR
} batch_event_t;

/**
 * @brief Batch processing context data
 */
typedef struct {
    mailbox_t batch_buffer;          ///< Assembled messages
    uint8_t total_messages;          ///< Total messages expected in batch
    uint8_t begin_message_slot;      ///< Slot index of the begin message
    uint8_t end_message_slot;        ///< Slot index of the end message
    uint8_t next_body_index;         ///< Next index in batch_slots array to collect
    uint8_t collected_body_count;    ///< Number of body messages collected so far
    uint8_t expected_body_count;     ///< Expected number of body messages (total - 2)
    uint8_t batch_slots[MAX_BATCH_SLOTS]; ///< Valid slot indices for this batch
} batch_context_t;

/**
 * @brief Complete batch state machine
 */
typedef struct {
    batch_state_enum_t current_state;
    batch_context_t context;
} batch_state_t;

// State machine interface functions
error_code_t batch_state_init(batch_state_t* state);
error_code_t batch_state_transition(batch_state_t* state, batch_event_t event, const void* event_data);
batch_state_enum_t batch_state_get_current(const batch_state_t* state);

// State query functions
bool batch_state_is_active(const batch_state_t* state);
bool batch_state_is_complete(const batch_state_t* state);
bool batch_state_can_accept_begin(const batch_state_t* state);
bool batch_state_can_accept_end(const batch_state_t* state);
bool batch_state_can_accept_body(const batch_state_t* state);
bool batch_state_is_ready_to_process(const batch_state_t* state);

// Context access functions
const batch_context_t* batch_state_get_context(const batch_state_t* state);
uint8_t batch_state_get_next_body_slot(const batch_state_t* state);

// Event data structures
typedef struct {
    const batch_begin_cmd_t* cmd;          // Pointer to command in message data
    const message_slot_t* message_slot;    // ← ADD: Pointer to complete message slot
    uint8_t slot_index;
} batch_begin_event_data_t;

typedef struct {
    const batch_end_cmd_t* cmd;            // Pointer to command in message data  
    const message_slot_t* message_slot;    // ← ADD: Pointer to complete message slot
    uint8_t slot_index;
} batch_end_event_data_t;

typedef struct {
    const message_slot_t* message;
    uint8_t slot_index;
} body_event_data_t;
