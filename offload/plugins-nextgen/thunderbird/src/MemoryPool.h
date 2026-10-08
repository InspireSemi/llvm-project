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
// this pool. A slab is one tbird_buffer_t; its base is the address the device
// mapped it at, so every pointer the pool hands out is a device address that
// kernels can use unchanged. Copies address a sub-allocation by its slab's
// buffer and an offset (lookup). The pool is grow-only; slabs are freed in
// destroy() at device shutdown.
//
// Thread safety: every member function takes the pool's lock for its whole
// body, so one device's pool may be used from several host threads at once, as
// OpenMP allows. A device address returned by allocate() or a {buffer, offset}
// returned by lookup() stays valid until that allocation is deallocated;
// slabs are reused, never released, before destroy(). allocate() and
// destroy() call the library on the device's context -- tbird_alloc_buffer()
// exchanges a message on the device's mailbox -- so callers serialise them
// with their other calls on that context; the pool itself never takes the
// caller's lock (lock order: the caller's, then the pool's).
//
//===----------------------------------------------------------------------===//

#ifndef THUNDERBIRD_MEMORYPOOL_H
#define THUNDERBIRD_MEMORYPOOL_H

#include "tbird_offload_api.h"
#include "internal/tbird_types_internal.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
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
    void *base;        // device address, from tbird_buffer_device_addr()
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

  /// Bump-allocate `size` bytes. Returns the device address of the allocation,
  /// or nullptr with the reason in `*Why` when it is given.
  void *allocate(size_t size, std::string *Why = nullptr);

  /// The slab buffer and offset a device address falls in, within a live
  /// sub-allocation (exact or interior); {nullptr, 0} if none.
  std::pair<tbird_buffer_t, size_t> lookup(void *ptr);

  /// Remove a sub-allocation from tracking.  When all sub-allocations in
  /// a slab have been deallocated, the slab's watermark resets to 0 and it
  /// becomes reusable for new bump-allocations.
  void deallocate(void *ptr);

  /// Free all slabs and detach from the context; allocate() then refuses.
  /// Call from deinitImpl().
  void destroy();

  /// Number of live sub-allocations.
  size_t liveAllocations() const;

  /// Guards every member above; see the thread-safety note at the top.
  mutable std::mutex Lock;
};

#endif // THUNDERBIRD_MEMORYPOOL_H
