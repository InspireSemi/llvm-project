/**
 * @file ivshmem_dt.h
 * @brief IVSHMEM device tree helper macros and constants
 * @details Provides defines for base address, size, and
 *          control region layout.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.2
 * @date 2025-OCT-20
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once
#ifndef __cplusplus
	// IVSHMEM shared memory configuration (hardcoded values)
	#define IVSHMEM_SHM_BASE 0x0082000000000000ULL
	#define IVSHMEM_SHM_SIZE 0x00400000U

	// Reserve the first 64 KiB for control structures (mailboxes, etc.)
	#define IVSHMEM_CTRL_REGION_MAX_SIZE 0x00010000U

	// Memory alignment requirements  
	#define IVSHMEM_MIN_ALIGNMENT 8U
	#define IVSHMEM_MAX_ALIGNMENT 4096U

	#define HEAP_OFFSET 0x00010000U
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
