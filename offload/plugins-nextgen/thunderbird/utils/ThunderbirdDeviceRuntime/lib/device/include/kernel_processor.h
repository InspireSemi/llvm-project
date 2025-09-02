#pragma once

#include "message_payloads.h"
#include "error_codes.h"

#include <stdint.h>
#include <stdbool.h>

// Main processing function
int kernel_processor_execute(const launch_cmd_t* launch_cmd);
