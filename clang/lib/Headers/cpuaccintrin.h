//===-- cpuaccintrin.h - CPU accelerator compatibility --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Provides compatibility definitions for CPU-based accelerators to work with
// the DeviceRTL infrastructure. This header defines GPU-style attributes and
// intrinsics in a way that works for traditional CPU architectures.
//
// CPU accelerators (like Thunderbird) use traditional threading models and
// don't have GPU-specific features like address spaces, warps, or SIMT
// execution. This header provides no-op or CPU-equivalent definitions.
//
//===----------------------------------------------------------------------===//

#ifndef __CPUACCINTRIN_H
#define __CPUACCINTRIN_H

#include <stdint.h>
#include <stdbool.h>

// GPU address space attributes are no-ops for CPU
#define __gpu_private
#define __gpu_constant const
#define __gpu_local
#define __gpu_global

// GPU dimension constants for compatibility with DeviceRTL
// CPU accelerators don't use these but they're referenced in common code
#define __GPU_X_DIM 0
#define __GPU_Y_DIM 1
#define __GPU_Z_DIM 2

// Default function attributes for CPU (no special attributes needed)
#define _DEFAULT_FN_ATTRS static inline __attribute__((always_inline))

//===----------------------------------------------------------------------===//
// Thread/Block/Grid mapping intrinsics
// These return runtime values from the OpenMP runtime for CPU execution
//===----------------------------------------------------------------------===//

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare OpenMP runtime functions with C linkage
int omp_get_thread_num(void);
int omp_get_num_threads(void);
int omp_get_team_num(void);
int omp_get_num_teams(void);

#ifdef __cplusplus
}
#endif

// Thread ID within block (uses OpenMP thread number)
_DEFAULT_FN_ATTRS uint32_t __gpu_thread_id_x(void) {
  return (uint32_t)omp_get_thread_num();
}

_DEFAULT_FN_ATTRS uint32_t __gpu_thread_id_y(void) {
  return 0; // CPU model uses 1D threading
}

_DEFAULT_FN_ATTRS uint32_t __gpu_thread_id_z(void) {
  return 0; // CPU model uses 1D threading
}

// Thread ID by dimension
_DEFAULT_FN_ATTRS uint32_t __gpu_thread_id(int __dim) {
  if (__dim == 0) return (uint32_t)omp_get_thread_num();
  return 0;
}

// Number of threads in block (uses OpenMP num threads)
_DEFAULT_FN_ATTRS uint32_t __gpu_num_threads_x(void) {
  return (uint32_t)omp_get_num_threads();
}

_DEFAULT_FN_ATTRS uint32_t __gpu_num_threads_y(void) {
  return 1; // CPU model uses 1D threading
}

_DEFAULT_FN_ATTRS uint32_t __gpu_num_threads_z(void) {
  return 1; // CPU model uses 1D threading
}

// Number of threads by dimension
_DEFAULT_FN_ATTRS uint32_t __gpu_num_threads(int __dim) {
  if (__dim == 0) return (uint32_t)omp_get_num_threads();
  return 1;
}

// Block ID (uses OpenMP team number)
_DEFAULT_FN_ATTRS uint32_t __gpu_block_id_x(void) {
  return (uint32_t)omp_get_team_num();
}

_DEFAULT_FN_ATTRS uint32_t __gpu_block_id_y(void) {
  return 0; // CPU model uses 1D blocks
}

_DEFAULT_FN_ATTRS uint32_t __gpu_block_id_z(void) {
  return 0; // CPU model uses 1D blocks
}

// Block ID by dimension
_DEFAULT_FN_ATTRS uint32_t __gpu_block_id(int __dim) {
  if (__dim == 0) return (uint32_t)omp_get_team_num();
  return 0;
}

// Number of blocks (uses OpenMP num teams)
_DEFAULT_FN_ATTRS uint32_t __gpu_num_blocks_x(void) {
  return (uint32_t)omp_get_num_teams();
}

_DEFAULT_FN_ATTRS uint32_t __gpu_num_blocks_y(void) {
  return 1; // CPU model uses 1D blocks
}

_DEFAULT_FN_ATTRS uint32_t __gpu_num_blocks_z(void) {
  return 1; // CPU model uses 1D blocks
}

// Number of blocks by dimension
_DEFAULT_FN_ATTRS uint32_t __gpu_num_blocks(int __dim) {
  if (__dim == 0) return (uint32_t)omp_get_num_teams();
  return 1;
}

//===----------------------------------------------------------------------===//
// Warp/Lane intrinsics
// CPU doesn't have warps, so these are simplified
//===----------------------------------------------------------------------===//

// Warp size - CPU model uses 1 (no SIMT execution)
_DEFAULT_FN_ATTRS uint32_t __gpu_num_lanes(void) {
  return 1;
}

// Lane ID within warp - always 0 for single-lane CPU
_DEFAULT_FN_ATTRS uint32_t __gpu_lane_id(void) {
  return 0;
}

// Lane mask - single active lane
_DEFAULT_FN_ATTRS uint64_t __gpu_lane_mask(void) {
  return 1;
}

//===----------------------------------------------------------------------===//
// Warp-level collective operations
// These are simplified for CPU (no actual warp execution)
//===----------------------------------------------------------------------===//

// CPU accelerators don't have warps/lanes - these are simplified stubs
// that allow the code to compile but may not provide meaningful SIMT behavior

// Shuffle intrinsics - on CPU these just return the value unchanged
// since there's no actual warp/SIMD lane concept
_DEFAULT_FN_ATTRS uint32_t
__gpu_shuffle_idx_u32(uint64_t __lane_mask, uint32_t __idx, uint32_t __x,
                      uint32_t __width) {
  // CPU has no warp shuffle - just return the value
  return __x;
}

_DEFAULT_FN_ATTRS uint64_t
__gpu_shuffle_idx_u64(uint64_t __lane_mask, uint32_t __idx, uint64_t __x,
                      uint32_t __width) {
  // CPU has no warp shuffle - just return the value
  return __x;
}

// Ballot - returns a bitmask indicating which threads have predicate true
// On CPU with single-thread execution, return 1 if true, 0 if false
_DEFAULT_FN_ATTRS uint64_t __gpu_ballot(uint64_t __lane_mask, bool __pred) {
  return __pred ? 1 : 0;
}

// Pointer address space queries - CPU doesn't have separate address spaces
_DEFAULT_FN_ATTRS bool __gpu_is_ptr_local(void *__ptr) {
  // CPU doesn't have GPU local/shared memory - always return false
  return false;
}

_DEFAULT_FN_ATTRS bool __gpu_is_ptr_private(void *__ptr) {
  // CPU doesn't have GPU private memory - always return false  
  return false;
}

//===----------------------------------------------------------------------===//
// Synchronization intrinsics
//===----------------------------------------------------------------------===//

// Thread barrier synchronization
// For CPU-based execution, this is implemented in the runtime's synchronization code
// We just need a declaration here - the implementation is in Synchronization.cpp
_DEFAULT_FN_ATTRS void __gpu_sync_threads(void) {
  // This will be implemented by the target-specific synchronization code
  // For Thunderbird, it's a simple compiler fence since true barriers
  // are handled at a higher level by the runtime
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

#endif // __CPUACCINTRIN_H
