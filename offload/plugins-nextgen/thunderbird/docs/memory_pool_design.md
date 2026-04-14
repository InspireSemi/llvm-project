# Thunderbird RTL Memory Pool Design

The Thunderbird OpenMP offload plugin (`libomptarget`) communicates with the RISC-V device through a shared memory region exposed as a PCIe BAR. Every device buffer allocation requires a kernel ioctl (`ADD_SHARED`), which allocates pages in the BAR, builds a page table, and registers the buffer with the device. The memory pool replaces per-allocation ioctls with a bump allocator over a small number of pre-allocated slabs, reducing kernel transitions and page-table overhead while keeping the allocation interface uniform for all consumers (ELF images, data maps, scalars).

## Motivation: Per-Allocation Overhead

Before the pool, every ELF-image load and every `#pragma omp target map(...)`
triggered a separate ioctl to the host kernel driver. For a SAXPY operation
with one ELF plus three data buffers (x, y, scalar), that's four round-trips
through the ioctl interface, four page-table constructions, and three
4 KB-page-aligned data allocations — even for a 4-byte scalar.

```mermaid
sequenceDiagram
    participant OMP as OpenMP Runtime
    participant RTL as Thunderbird RTL
    participant DRV as Kernel Driver
    participant BAR as Shared BAR (4 MiB)

    Note over OMP,BAR: Before: Per-Allocation Model (SAXPY)

    OMP->>RTL: loadBinary (ELF ~217 KB)
    RTL->>DRV: ioctl ADD_SHARED (~217 KB)
    DRV->>BAR: alloc 53 data pages + 1 PT page
    DRV-->>RTL: shared_key

    OMP->>RTL: allocate(4000) — x array
    RTL->>DRV: ioctl ADD_SHARED (4096 aligned)
    DRV->>BAR: alloc 1 data page + 1 PT page
    DRV-->>RTL: shared_key

    OMP->>RTL: allocate(4000) — y array
    RTL->>DRV: ioctl ADD_SHARED (4096 aligned)
    DRV->>BAR: alloc 1 data page + 1 PT page
    DRV-->>RTL: shared_key

    OMP->>RTL: allocate(4) — scalar
    RTL->>DRV: ioctl ADD_SHARED (4096 aligned)
    DRV->>BAR: alloc 1 data page + 1 PT page
    DRV-->>RTL: shared_key

    Note over BAR: 4 ioctls · 59 pages consumed · 12 KB wasted on 3 tiny data allocs
```

The scalar allocation is particularly wasteful: 4 bytes of data consumes an
entire 4 KB data page plus a 4 KB page-table page — a 2,048× overhead.

## Pool Architecture

The pool pre-allocates large slabs via a single ioctl and sub-allocates within them using a bump pointer. All allocations — ELF images, user data, scalars — go through the pool uniformly. There is no fallback to direct `tbird_alloc_buffer()`.

```mermaid
flowchart TD
    subgraph pool["Memory Pool (in ThunderbirdDeviceTy)"]
        direction TB
        state["Rule: new slab = max(64 KB floor, requested)<br/>total_pages ≤ MAX_POOL_PAGES (500)<br/>allocations: ptr → {slab, offset, size}"]

        subgraph slab0["Slab 0 · ~217 KB<br/>(sized to the ELF)"]
            elf["ELF Image<br/>~217 KB"]
        end

        subgraph slab1["Slab 1 · 1 MiB<br/>(sized to matrix A)"]
            matA["HPL matrix A<br/>(N=360 → ~1 MiB)"]
            freeA["...small tail..."]
        end

        subgraph slab2["Slab 2 · 64 KB<br/>(64 KB floor, many small sub-allocs)"]
            scalars["scalar args<br/>(4 B each, bump-packed)"]
            workbuf["HPL WORK<br/>(~4 KB)"]
            free2["... free space ..."]
        end
    end

    omp_alloc["OpenMP Runtime<br/>allocate(size)"] --> pool
    pool -->|"bump pointer<br/>within slab"| slab2
    pool -->|"ioctl only when<br/>current slab full"| driver["Kernel Driver<br/>ioctl ADD_SHARED"]
    driver --> bar["BAR Pages"]

    style elf fill:#4a9,color:#fff
    style matA fill:#49a,color:#fff
    style scalars fill:#49a,color:#fff
    style workbuf fill:#49a,color:#fff
    style freeA fill:#ddd,color:#666
    style free2 fill:#ddd,color:#666
```

**Slab sizing is stateless.** Every new slab is sized as
`max(64 KB floor, requested)`, page-aligned, capped at
`TBIRD_MAX_BUFFER_SIZE` (1 MiB). There is no `next_slab_size` state,
no doubling, no growth curve — each slab is independently sized to the
request that forced it. Big allocations (ELF ~217 KB, HPL matrix A ~1 MiB)
define their own slab exactly; small allocations land in a 64 KB slab
and amortize the ioctl across many bump-allocations.

## Allocation Flow

When the OpenMP runtime requests a device buffer, the pool tries the current
slab first. If the slab is full, a new one is allocated at
`max(64 KB, requested)` and sanity-checked against the per-mailbox budget.

```mermaid
flowchart TD
    req["allocate(size)"] --> align["Align to 16 bytes"]
    align --> check{"Current slab<br/>has room?"}

    check -->|yes| bump["Bump watermark<br/>return base + offset"]
    bump --> record["Record in allocations map<br/>{slab_idx, offset, size}"]

    check -->|no| calc["slab_size =<br/>max(INITIAL_SLAB_SIZE, aligned)<br/>round up to page boundary<br/>cap at TBIRD_MAX_BUFFER_SIZE"]

    calc --> budget{"total_pages + new_pages<br/>≤ MAX_POOL_PAGES (500)?"}
    budget -->|no| fail["Return nullptr<br/>(mailbox budget exhausted)"]
    budget -->|yes| ioctl["tbird_alloc_buffer(ctx, slab_size)<br/>— single kernel ioctl"]

    ioctl --> push["Push new slab<br/>Update total_pages"]
    push --> bump

    style fail fill:#c44,color:#fff
    style ioctl fill:#48a,color:#fff
    style bump fill:#4a9,color:#fff
```

## SAXPY With Pool: 2 Ioctls vs 4

SAXPY touches one ELF image plus three data buffers. The Before/After
counts include the ELF load as well as the three data maps.

```mermaid
sequenceDiagram
    participant OMP as OpenMP Runtime
    participant RTL as Thunderbird RTL (Pool)
    participant DRV as Kernel Driver
    participant BAR as Shared BAR (4 MiB)

    Note over OMP,BAR: After: Pool Model (SAXPY)

    OMP->>RTL: loadBinary (ELF ~217 KB)
    RTL->>RTL: No slab yet · 217K (≥ 64K floor)
    RTL->>DRV: ioctl ADD_SHARED (~217 KB)
    DRV->>BAR: alloc 53 data pages + 1 PT page
    DRV-->>RTL: shared_key
    RTL->>RTL: Slab 0 now full (ELF fills it)

    OMP->>RTL: allocate(4000) — x array
    RTL->>RTL: Slab 0 full · new slab 64K (≥ 64K floor)
    RTL->>DRV: ioctl ADD_SHARED (65536)
    DRV->>BAR: alloc 16 data pages + 1 PT page
    DRV-->>RTL: shared_key
    RTL->>RTL: Slab 1 bump: offset 0 · watermark → 4000

    OMP->>RTL: allocate(4000) — y array
    RTL->>RTL: Slab 1 has room (61536 free)
    RTL->>RTL: Bump: offset 4000 · watermark → 8000

    OMP->>RTL: allocate(4) — scalar
    RTL->>RTL: Slab 1 has room (57536 free)
    RTL->>RTL: Bump: offset 8000 · watermark → 8016

    Note over BAR: 2 ioctls · ~71 pages consumed · no page-align waste on scalars
```

| Metric | Before (per-alloc) | After (pool) | Improvement |
|--------|-------------------|--------------|-------------|
| Kernel ioctls (ELF + 3 data maps) | 4 | 2 | **2× fewer** |
| Scalar (4 B) page waste | 4,092 bytes | 0 bytes | **No page-alignment waste** |
| Page-table pages | 4 | 2 | **2× fewer** |
| Total BAR overhead (SAXPY) | ~236 KB (54 ELF + 3 tiny slabs) | ~284 KB (54 ELF + 17-page data slab) | slightly more BAR, far fewer ioctls |

The tradeoff: the 64 KB data slab allocates more BAR up front than three
isolated 4 KB pages would. In exchange we pay one ioctl instead of three,
and that slab has ~57 KB of headroom to absorb subsequent allocations
without any further kernel transitions. The scalar waste (3 × 4 KB of
padding under the old scheme) disappears entirely.

## BAR Budget and Page Math

The 4 MiB BAR is partitioned by the device-side driver into control
structures (~24 KB, ~6 pages) and a user-data region of ~1,018 pages.
The BAR exposes **four concurrent mailboxes** — each plugin instance
attaches to one. Each instance's pool must therefore leave room for its
peers: a single instance that eats the entire user region would block
any concurrent mailbox from doing useful work.

```mermaid
flowchart LR
    subgraph bar["PCIe BAR 2 · 4 MiB (1,024 pages)"]
        direction TB
        ctrl["Control Structures<br/>queues + mailboxes<br/>~6 pages"]
        user["User Data Region<br/>~1,018 pages"]
    end

    subgraph mbox["Per-Mailbox Pool Budget"]
        direction TB
        cap["MAX_POOL_PAGES = 500<br/>(~2 MiB per mailbox)"]
        rationale["half the BAR<br/>⇒ two concurrent mailboxes<br/>coexist comfortably;<br/>lower the constant further<br/>if four will overlap"]
        cap --> rationale
    end

    subgraph workload["HPL (N=360) — Example"]
        w1["ELF ~217 KB → ~54 pages"]
        w2["Matrix A ~1 MiB → ~257 pages"]
        w3["WORK + scalar args → ~10 pages"]
        wtot["Total ~322 pages<br/>(≈ 64% of a mailbox budget)"]
        w1 --> wtot
        w2 --> wtot
        w3 --> wtot
    end

    user --> mbox
    mbox --> workload

    style ctrl fill:#c94,color:#fff
    style cap fill:#cc4,color:#333
    style user fill:#4a9,color:#fff
    style wtot fill:#49a,color:#fff
```

A fresh slab request past 500 pages returns `nullptr` and propagates up
as an OOM. HPL at the current 1 MiB per-buffer matrix ceiling consumes
~322 pages, leaving ~178 pages of headroom inside a single mailbox's
budget. If future workloads push up against the cap, the right knob to
turn is whichever fits your concurrency story: raise `MAX_POOL_PAGES`
if you expect fewer concurrent mailboxes, or lower it further if four
are expected to share the BAR simultaneously.

## ELF Images and elf_offset

ELF device images (compiled RISC-V kernels) are allocated through the same pool as data buffers. When an image lands at a non-zero offset within a slab, the offset is recorded and passed to `tbird_launch_kernel_sync()` so the device server knows where the ELF starts within the shared buffer.

```mermaid
sequenceDiagram
    participant RTL as loadBinaryImpl
    participant Pool as Memory Pool
    participant Dev as Device Server

    RTL->>Pool: pool.allocate(ImageSize)
    Pool-->>RTL: img_ptr (base + watermark)

    RTL->>Pool: pool.lookup(img_ptr)
    Pool-->>RTL: {slab_buffer, elf_offset}

    RTL->>RTL: tbird_buffer_write(buffer, elf_offset, ELF data)
    RTL->>RTL: Image->elf_offset = elf_offset

    Note over RTL,Dev: Later, at kernel launch...

    RTL->>Dev: tbird_launch_kernel_sync(<br/>  buffer, elf_offset, ImageSize,<br/>  "kernel_name", args)
    Dev->>Dev: Parse ELF at buffer + elf_offset
    Dev->>Dev: dlopen via memfd, resolve symbol
    Dev->>Dev: Execute kernel
    Dev-->>RTL: TBIRD_SUCCESS
```

This unified approach means the pool never needs special-case handling for images vs data — the `elf_offset` parameter in the kernel launch API was already validated by the `test_kernel_elf_offset` test in the offload-platform test suite.

## Lifecycle: Grow-Only with Bulk Release

The pool is grow-only during execution. When the OpenMP runtime calls `free()`, the pool removes the allocation from its tracking map but does not release the slab's DMA memory. Slabs are released in bulk at device shutdown.

```mermaid
stateDiagram-v2
    [*] --> Initialized: pool.init(ctx)

    Initialized --> Allocating: allocate(size)

    state Allocating {
        [*] --> TrySlab: Check current slab
        TrySlab --> BumpAlloc: Room available
        TrySlab --> NewSlab: Slab full
        NewSlab --> BudgetCheck: Compute page cost
        BudgetCheck --> AllocSlab: Within budget
        BudgetCheck --> Error: Exceeds MAX_POOL_PAGES
        AllocSlab --> BumpAlloc: ioctl ADD_SHARED
        BumpAlloc --> [*]: Return host ptr
    }

    Allocating --> Tracking: Sub-alloc recorded

    Tracking --> Deallocated: pool.deallocate(ptr)
    Deallocated --> Tracking: Map entry removed<br/>(slab memory retained)

    Tracking --> Destroyed: pool.destroy()
    Destroyed --> [*]: All slabs freed<br/>via tbird_free_buffer()
```

This design is appropriate because:
1. The OpenMP runtime reuses device memory via its internal mapping table — actual allocations are rare after the first few target regions
2. HPL pre-maps the entire matrix once and reuses it across hundreds of panel iterations
3. The total memory footprint is bounded by `MAX_POOL_PAGES` regardless of allocation pattern

**Source:** [`llvm-project/offload/plugins-nextgen/thunderbird/src/rtl.cpp`](../../../llvm-project/offload/plugins-nextgen/thunderbird/src/rtl.cpp) (lines 62-188: MemoryPool struct; lines 361, 483, 601, 628, 834: integration points), [`offload-platform/docs/MEMORY_ARCHITECTURE.md`](../../localstore/offload/offload-platform/docs/MEMORY_ARCHITECTURE.md) (BAR layout, page budget)
