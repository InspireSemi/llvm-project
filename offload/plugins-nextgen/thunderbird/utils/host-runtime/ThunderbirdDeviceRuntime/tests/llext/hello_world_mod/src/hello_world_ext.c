/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This very simple hello world C code can be used as a test case for building
 * probably the simplest loadable extension. It requires a single symbol be
 * linked, section relocation support, and the ability to export and call out to
 * a function.
 */

#include "device_memory.h"

#include <zephyr/inspire_hwpatch.h>
#include <zephyr/llext/symbol.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>


LOG_MODULE_REGISTER(hello_world_ext, LOG_LEVEL_INF);

void execute(void){
	LOG_INF("[execute] === execute function starting ===");

	LOG_INF("[execute] Testing device_shared_malloc");
	void* shared_ptr = device_shared_malloc(64);
	if (shared_ptr) {
		LOG_INF("[execute] device_shared_malloc succeeded: %p", shared_ptr);
		device_shared_free(shared_ptr);
		LOG_INF("[execute] device_shared_free completed");
	} else {
		LOG_ERR("[execute] device_shared_malloc failed");
	}

	LOG_INF("[execute] Testing device_exclusive_malloc");
	void* exclusive_ptr = device_exclusive_malloc(64);
	if (exclusive_ptr) {
		LOG_INF("[execute] device_exclusive_malloc succeeded: %p", exclusive_ptr);
		device_exclusive_free(exclusive_ptr);
		LOG_INF("[execute] device_exclusive_free completed");
	} else {
		LOG_ERR("[execute] device_exclusive_malloc failed");
	}

	LOG_INF("[execute] === execute function completing ===");
}
EXPORT_SYMBOL(execute);

void hello_world(void)
{
	LOG_INF("[hello_world_ext] === hello_world function starting ===");
	
	LOG_INF("[hello_world_ext] Testing basic k_malloc");
	void* ptr_basic = k_malloc(32);
	if (ptr_basic) {
		LOG_INF("[hello_world_ext] k_malloc succeeded: %p", ptr_basic);
		k_free(ptr_basic);
		LOG_INF("[hello_world_ext] k_free completed");
	} else {
		LOG_ERR("[hello_world_ext] k_malloc failed");
	}
	
	LOG_INF("[hello_world_ext] k_malloc test completed, moving to device_shared_malloc");
	void* ptr1 = device_shared_malloc(32);
	if (ptr1) {
		LOG_INF("[hello_world_ext] device_shared_malloc succeeded: %p", ptr1);
		device_shared_free(ptr1);
		LOG_INF("[hello_world_ext] device_shared_free completed");
	} else {
		LOG_ERR("[hello_world_ext] device_shared_malloc failed");
	}

	LOG_INF("[hello_world_ext] === hello_world function completing ===");
}
EXPORT_SYMBOL(hello_world);
