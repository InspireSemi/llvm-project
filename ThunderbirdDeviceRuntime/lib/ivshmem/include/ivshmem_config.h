#pragma once

// Note: Using macros instead of constexpr even in C++ because these values
// are required for static_assert and array sizing in certain contexts
#define MESSAGE_SLOT_DATA_SIZE 64u
#define MESSAGE_SLOT_MAX_SIZE_BYTES 80u
#define MAILBOX_SLOT_COUNT 16u
#define MAX_BATCH_SLOTS MAILBOX_SLOT_COUNT
#define MAILBOX_MAX_SIZE_BYTES (MAILBOX_SLOT_COUNT * MESSAGE_SLOT_MAX_SIZE_BYTES) + 16u
#define IVSHMEM_ABI_VERSION 1u
