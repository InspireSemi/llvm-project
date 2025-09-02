/**
 * @file error_codes.h
 * @brief Common error code definitions for Thunderbird Device Runtime
 * @details Provides standardized error codes used throughout the IVSHMEM
 *          communication system and device runtime components.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ERR_OK = 0,
    ERR_INVALID_PARAM = -1,
    ERR_NO_MEMORY = -2,
    ERR_TIMEOUT = -3,
    ERR_BUSY = -4,
    ERR_NOT_FOUND = -5,
    ERR_PERMISSION = -6,
    ERR_IO = -7,
    ERR_BEGIN_RECEIVED_BUT_CURRENT_BATCH_STILL_PROCESSING = -8,
    ERR_END_RECEIVED_BUT_NO_BATCH_TO_END = -9,
    ERR_INVALID_BATCH_STATE = -10,
    ERR_END_CMD_MISMATCHES_BEGIN = -11,
    ERR_BODY_SLOT_INDEX_OUT_OF_RANGE = -12,
    ERR_BODY_SLOT_ALREADY_RECEIVED = -13,
    ERR_UNKNOWN = -99
} error_code_t;

#ifdef __cplusplus
}
#endif

// Compile-time layout guards (host/device must agree)
#ifdef __cplusplus
// Validate error_code_t size is stable across host/device
static_assert(sizeof(error_code_t) == sizeof(int32_t),
              "error_code_t size changed; ABI incompatible");
#else
_Static_assert(sizeof(error_code_t) == sizeof(int32_t),
               "error_code_t size changed; ABI incompatible");
#endif

