/**
 * @file ivshmem_dt.h
 * @brief IVSHMEM device tree helper macros and constants
 * @details Provides compile-time macros for extracting IVSHMEM configuration
 *          from device tree, including base address, size validation, and
 *          control region layout definitions.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once
#ifndef __cplusplus
	#include <zephyr/devicetree.h>
	#include <zephyr/sys/util.h>

	//#TODO #FIXME this needs to be simplified so we can use it in the x86 app.

	// IVSHMEM shared memory device tree helpers
	#define IVSHMEM_SHM_NODE DT_NODELABEL(ivshmem_shm)
	BUILD_ASSERT(DT_NODE_EXISTS(IVSHMEM_SHM_NODE), "ivshmem_shm node not found in devicetree");

	#define IVSHMEM_SHM_BASE DT_REG_ADDR(IVSHMEM_SHM_NODE)
	#define IVSHMEM_SHM_SIZE DT_REG_SIZE(IVSHMEM_SHM_NODE)
	BUILD_ASSERT(IVSHMEM_SHM_SIZE == 0x00400000u, "Expected 4 MiB shared memory region");

	// Reserve the first 64 KiB for control structures (mailboxes, etc.)
	#define IVSHMEM_CTRL_REGION_MAX_SIZE 0x00010000u
	BUILD_ASSERT(IVSHMEM_SHM_SIZE >= IVSHMEM_CTRL_REGION_MAX_SIZE, "IVSHMEM too small for control region");

	// Memory alignment requirements  
	#define IVSHMEM_MIN_ALIGNMENT 8u
	#define IVSHMEM_MAX_ALIGNMENT 4096u

	#define HEAP_OFFSET DT_PROP_OR(IVSHMEM_SHM_NODE, inspire_heap_offset, 0x00010000)
	#define HEAP_ALIGN 8U
#else
	constexpr uint64_t IVSHMEM_SHM_BASE = 0x0082000000000000ull;
	constexpr uint64_t IVSHMEM_SHM_SIZE = 0x00400000ull;
	constexpr uint64_t IVSHMEM_CTRL_REGION_MAX_SIZE = 0x00010000ull;
	constexpr uint64_t IVSHMEM_MIN_ALIGNMENT = 8ull;
	constexpr uint64_t IVSHMEM_MAX_ALIGNMENT = 4096ull;
	constexpr uint64_t HEAP_OFFSET = 0x00010000ull;
	constexpr uint64_t HEAP_ALIGN = 8;
#endif
