//===-- MemoryPool.h - Thunderbird slab memory allocator ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Bump allocator over tbird_buffer_t slabs for the Thunderbird RTL plugin.
//
// All device memory allocations (ELF images, data maps, scalars) go through
// this pool. Sub-allocations within a slab reuse the slab's shared_key and
// are addressed via offset for write/read/launch operations. The pool is
// grow-only; slabs are freed in destroy() at device shutdown.
//
// NOTE: not thread-safe — the Thunderbird RTL uses a single mailbox.
//
//===----------------------------------------------------------------------===//

#ifndef THUNDERBIRD_MEMORYPOOL_H
#define THUNDERBIRD_MEMORYPOOL_H

#include "tbird_offload_api.h"
#include "internal/tbird_types_internal.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

struct MemoryPool {
  static constexpr size_t INITIAL_SLAB_SIZE = 64 * 1024;  // 64 KB
  static constexpr size_t ALIGNMENT = 16;
  static constexpr size_t PAGE_SIZE = 4096;
  // The 4 MiB BAR (~1018 usable pages) is shared among up to two
  // concurrent mailboxes.  Cap each instance's total slab footprint
  // at ~half the usable region (~2 MiB) so a second concurrent
  // mailbox can coexist even under memory pressure.
  static constexpr size_t MAX_POOL_PAGES = 500;

  struct Slab {
    tbird_buffer_t buffer;
    void *base;        // host VA from tbird_buffer_host_ptr()
    size_t capacity;
    size_t watermark;  // next free offset (resets to 0 when fully drained)
    size_t live_count; // active sub-allocations; when 0, slab is reclaimable
    bool exclusive;    // true = sized for one big allocation; no cohabitation
                       // while live_count > 0 (prevents small siblings from
                       // pinning the slab and blocking reclamation).
  };

  struct SubAlloc {
    size_t slab_idx;
    size_t offset;
    size_t size;
  };

  tbird_context_t ctx = nullptr;
  std::vector<Slab> slabs;
  size_t total_pages = 0;
  std::unordered_map<void *, SubAlloc> allocations;

  void init(tbird_context_t context);

  /// Bump-allocate `size` bytes. Returns host VA usable as OpenMP "device ptr".
  void *allocate(size_t size);

  /// Look up which slab and offset a pointer maps to (exact + interior).
  std::pair<tbird_buffer_t, size_t> lookup(void *ptr);

  /// Remove a sub-allocation from tracking.  When all sub-allocations in
  /// a slab have been deallocated, the slab's watermark resets to 0 and it
  /// becomes reusable for new bump-allocations.
  void deallocate(void *ptr);

  /// Free all slabs. Call from deinitImpl().
  void destroy();
};

#endif // THUNDERBIRD_MEMORYPOOL_H
