//===-- MemoryPool.cpp - Thunderbird slab memory allocator --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MemoryPool.h"
#include "Shared/Debug.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

void MemoryPool::init(tbird_context_t context) {
  ctx = context;
  total_pages = 0;
}

void *MemoryPool::allocate(size_t size) {
  size_t aligned = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

  // Try existing slabs (prefer most-recently-created, scan backwards).
  // A slab whose live_count hit 0 has had its watermark reset to 0 and
  // is fully reusable — this is how HPL's residual check reuses the
  // solve-phase matrix slab instead of allocating a second one.
  //
  // Exclusive slabs are reserved for large one-shot tenants (ELF image,
  // HPL matrix A) so those buffers stay reclaimable.  A small allocation
  // (<= INITIAL_SLAB_SIZE) never lands in an exclusive slab — not even a
  // drained one.  Two failure modes this prevents:
  //   - live exclusive slab: a small sibling keeps live_count > 0 after
  //     the big tenant leaves, so the watermark never resets and the slab
  //     can never be reclaimed.
  //   - drained exclusive slab: the most-recent-first scan would place a
  //     small allocation into the just-freed big slab and re-pin it, so
  //     the next large buffer cannot reuse it and must allocate a fresh
  //     slab — blowing the page budget (HPL N=360: a 2880-byte alloc
  //     re-pinned the drained 255-page matrix slab → 273+255 > 500 →
  //     mandatory-offload SIGABRT).
  // A large allocation (> INITIAL_SLAB_SIZE) may still reuse a *drained*
  // exclusive slab; it only skips one with a live tenant.
  bool is_small = (aligned <= INITIAL_SLAB_SIZE);
  for (size_t i = slabs.size(); i > 0; i--) {
    Slab &s = slabs[i - 1];
    if (s.exclusive && (is_small || s.live_count > 0))
      continue;
    if (s.watermark + aligned <= s.capacity) {
      void *ptr = (char *)s.base + s.watermark;
      allocations[ptr] = {i - 1, s.watermark, size};
      s.watermark += aligned;
      s.live_count++;
      DP("POOL: sub-alloc %zu bytes in slab %zu at offset %zu → %p\n",
         size, i - 1, s.watermark - aligned, ptr);
      return ptr;
    }
  }

  // Need a new slab.  Size is the max of a fixed 64 KB floor and the
  // current request, page-aligned and capped at the per-buffer limit.
  // No stateful growth: small allocations land in 64 KB slabs, large
  // ones (ELF image, HPL matrix A) size their own slab exactly.
  size_t slab_size = std::max((size_t)INITIAL_SLAB_SIZE, aligned);
  slab_size = (slab_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
  if (slab_size > TBIRD_MAX_BUFFER_SIZE)
    slab_size = TBIRD_MAX_BUFFER_SIZE;
  if (aligned > TBIRD_MAX_BUFFER_SIZE) {
    DP("POOL ERROR: allocation %zu exceeds TBIRD_MAX_BUFFER_SIZE (%d)\n",
       size, TBIRD_MAX_BUFFER_SIZE);
    return nullptr;
  }

  // Page budget check (data pages + page-table pages)
  size_t data_pages = slab_size / PAGE_SIZE;
  size_t pt_pages = (data_pages + 510) / 511;
  if (total_pages + data_pages + pt_pages > MAX_POOL_PAGES) {
    DP("POOL ERROR: BAR budget exceeded (%zu + %zu + %zu > %zu)\n",
       total_pages, data_pages, pt_pages, MAX_POOL_PAGES);
    return nullptr;
  }

  DP("POOL: allocating new slab: %zu bytes (%zu pages)\n",
     slab_size, data_pages);
  tbird_buffer_t buf = tbird_alloc_buffer(ctx, slab_size);
  if (!buf) {
    DP("POOL ERROR: tbird_alloc_buffer(%zu) failed: %s\n",
       slab_size, tbird_last_error(ctx));
    return nullptr;
  }

  void *base = tbird_buffer_host_ptr(buf);
  // Mark the slab exclusive iff it was upsized past INITIAL_SLAB_SIZE for
  // this one allocation.  That matches the big-one-shot-tenant pattern
  // (ELF image, HPL matrix) we want reclaimable.  Plain INITIAL_SLAB_SIZE
  // slabs stay shared so small allocations pack together as before.
  bool is_exclusive = (slab_size > INITIAL_SLAB_SIZE);
  slabs.push_back({buf, base, slab_size, 0, 0, is_exclusive});
  total_pages += data_pages + pt_pages;

  // Always print slab creation — visible in session log even without
  // LIBOMPTARGET_DEBUG=1, critical for diagnosing budget exhaustion.
  fprintf(stderr, "POOL: new slab %zu: %zu bytes (%zu+%zu pages, total %zu/%zu)\n",
          slabs.size() - 1, slab_size, data_pages, pt_pages,
          total_pages, MAX_POOL_PAGES);

  // Allocate from fresh slab
  Slab &fresh = slabs.back();
  void *ptr = (char *)fresh.base + fresh.watermark;
  allocations[ptr] = {slabs.size() - 1, fresh.watermark, size};
  fresh.watermark += aligned;
  fresh.live_count++;
  DP("POOL: sub-alloc %zu bytes in new slab %zu at offset 0 → %p\n",
     size, slabs.size() - 1, ptr);
  return ptr;
}

std::pair<tbird_buffer_t, size_t> MemoryPool::lookup(void *ptr) {
  // Fast: exact match
  auto it = allocations.find(ptr);
  if (it != allocations.end()) {
    auto &sub = it->second;
    return {slabs[sub.slab_idx].buffer, sub.offset};
  }
  // Slow: interior pointer
  uintptr_t addr = (uintptr_t)ptr;
  for (auto &[base, sub] : allocations) {
    uintptr_t base_addr = (uintptr_t)base;
    if (addr >= base_addr && addr < base_addr + sub.size)
      return {slabs[sub.slab_idx].buffer, sub.offset + (addr - base_addr)};
  }
  return {nullptr, 0};
}

void MemoryPool::deallocate(void *ptr) {
  auto it = allocations.find(ptr);
  if (it != allocations.end()) {
    size_t idx = it->second.slab_idx;
    allocations.erase(it);
    if (--slabs[idx].live_count == 0) {
      slabs[idx].watermark = 0;
      DP("POOL: slab %zu fully drained → reclaimed (capacity %zu)\n",
         idx, slabs[idx].capacity);
    }
  }
}

void MemoryPool::destroy() {
  if (!ctx) return;
  for (auto &slab : slabs)
    tbird_free_buffer(ctx, slab.buffer);
  slabs.clear();
  allocations.clear();
  total_pages = 0;
  DP("POOL: destroyed all slabs\n");
}
