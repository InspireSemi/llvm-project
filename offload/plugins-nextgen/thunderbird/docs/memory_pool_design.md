# Thunderbird RTL Memory Pool Design

The Thunderbird OpenMP offload plugin (`libomptarget`) communicates with the RISC-V device through a shared memory region exposed as a PCIe BAR. Every device buffer allocation requires a kernel ioctl (`ADD_SHARED`), which allocates pages in the BAR, builds a page table, and registers the buffer with the device. The memory pool replaces per-allocation ioctls with a bump allocator over a small number of pre-allocated slabs, reducing kernel transitions and page-table overhead while keeping the allocation interface uniform for all consumers (ELF images, data maps, scalars).

## Motivation: Per-Allocation Overhead

Before the pool, every `#pragma omp target map(...)` triggered a separate ioctl to the host kernel driver. For a SAXPY operation with three mapped buffers (x, y, scalar), this meant three round-trips through the ioctl interface, three page-table constructions, and three 4 KB page-aligned allocations — even for a 4-byte scalar.

```mermaid
sequenceDiagram
    participant OMP as OpenMP Runtime
    participant RTL as Thunderbird RTL
    participant DRV as Kernel Driver
    participant BAR as Shared BAR (4 MiB)

    Note over OMP,BAR: Before: Per-Allocation Model (SAXPY)

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

    Note over BAR: 3 ioctls · 6 pages consumed · 12 KB for 8,004 bytes of data
```

The scalar allocation is particularly wasteful: 4 bytes of data consumes an entire 4 KB data page plus a 4 KB page-table page — a 2,048x overhead.

## Pool Architecture

The pool pre-allocates large slabs via a single ioctl and sub-allocates within them using a bump pointer. All allocations — ELF images, user data, scalars — go through the pool uniformly. There is no fallback to direct `tbird_alloc_buffer()`.

```mermaid
flowchart TD
    subgraph pool["Memory Pool (in ThunderbirdDeviceTy)"]
        direction TB
        state["next_slab_size floor: 64K → 128K → 256K → ... → 1 MiB<br/>total_pages: running BAR budget<br/>allocations: ptr → {slab, offset, size}"]

        subgraph slab0["Slab 0 · ~220 KB<br/>(grown to fit first allocation)"]
            elf["ELF Image<br/>~216 KB"]
        end

        subgraph slab1["Slab 1 · 128 KB<br/>(floor after first doubling)"]
            buf_x["x array<br/>4000 B"]
            buf_y["y array<br/>4000 B"]
            buf_s["scalar<br/>4 B"]
            free1["... free space ..."]
        end
    end

    omp_alloc["OpenMP Runtime<br/>allocate(size)"] --> pool
    pool -->|"bump pointer<br/>within slab"| slab1
    pool -->|"ioctl only when<br/>new slab needed"| driver["Kernel Driver<br/>ioctl ADD_SHARED"]
    driver --> bar["BAR Pages"]

    style elf fill:#4a9,color:#fff
    style buf_x fill:#49a,color:#fff
    style buf_y fill:#49a,color:#fff
    style buf_s fill:#49a,color:#fff
    style free1 fill:#ddd,color:#666
```

**Slab sizing.** A new slab is allocated at `max(next_slab_size, requested_size)`, page-aligned, capped at `TBIRD_MAX_BUFFER_SIZE`. `INITIAL_SLAB_SIZE = 64 KB` is the *floor* for the first slab — it is not a hard cap. An HPL run, for example, loads its ~216 KB device ELF through the pool before any data map, so Slab 0 grows to ~220 KB on the very first allocation. After that call, `next_slab_size` doubles to 128 KB, which becomes the floor for Slab 1. The doubling continues up to the 1 MiB per-buffer cap.

## Allocation Flow

When the OpenMP runtime requests a device buffer, the pool first tries to fit it in the current slab. If the slab is full, a new one is allocated from the driver with doubling growth (64 KB → 128 KB → ... → 1 MiB cap). A BAR page budget guard prevents overrunning the 4 MiB shared memory region.

```mermaid
flowchart TD
    req["allocate(size)"] --> align["Align to 16 bytes"]
    align --> check{"Current slab<br/>has room?"}

    check -->|yes| bump["Bump watermark<br/>return base + offset"]
    bump --> record["Record in allocations map<br/>{slab_idx, offset, size}"]

    check -->|no| calc["Calculate slab size:<br/>max(next_slab_size, aligned)<br/>round up to page boundary<br/>cap at TBIRD_MAX_BUFFER_SIZE"]

    calc --> budget{"total_pages + new_pages<br/>≤ MAX_POOL_PAGES?"}
    budget -->|no| fail["Return nullptr<br/>(BAR exhausted)"]
    budget -->|yes| ioctl["tbird_alloc_buffer(ctx, slab_size)<br/>— single kernel ioctl"]

    ioctl --> push["Push new slab<br/>Update total_pages<br/>Double next_slab_size"]
    push --> bump

    style fail fill:#c44,color:#fff
    style ioctl fill:#48a,color:#fff
    style bump fill:#4a9,color:#fff
```

## SAXPY With Pool: 3x Fewer Ioctls

The same SAXPY operation now uses a single slab allocation. The three data buffers are bump-allocated within the slab at different offsets, and all DMA transfers use the slab's `shared_key` with the appropriate offset.

```mermaid
sequenceDiagram
    participant OMP as OpenMP Runtime
    participant RTL as Thunderbird RTL (Pool)
    participant DRV as Kernel Driver
    participant BAR as Shared BAR (4 MiB)

    Note over OMP,BAR: After: Pool Model (SAXPY)

    OMP->>RTL: allocate(4000) — x array
    RTL->>RTL: No slab exists yet
    RTL->>DRV: ioctl ADD_SHARED (65536)
    DRV->>BAR: alloc 16 data pages + 1 PT page
    DRV-->>RTL: shared_key
    RTL->>RTL: Bump: offset 0, watermark → 4000

    OMP->>RTL: allocate(4000) — y array
    RTL->>RTL: Slab has room (61,536 free)
    RTL->>RTL: Bump: offset 4000, watermark → 8000

    OMP->>RTL: allocate(4) — scalar
    RTL->>RTL: Slab has room (57,536 free)
    RTL->>RTL: Bump: offset 8000, watermark → 8016

    Note over BAR: 1 ioctl · 17 pages consumed · 4 B scalar wastes 0 pages
```

| Metric | Before (per-alloc) | After (pool) | Improvement |
|--------|-------------------|--------------|-------------|
| Kernel ioctls | 3 | 1 | **3x fewer** |
| BAR pages consumed | 6 (3 data + 3 PT) | 17 (16 data + 1 PT) | More data pages, but... |
| Wasted space (scalar) | 4,092 bytes | 0 bytes | **No page-alignment waste** |
| Page-table pages | 3 | 1 | **3x fewer** |
| Total BAR overhead | 24 KB | 68 KB | Higher raw BAR, but fewer ioctls |

The tradeoff: the 64 KB slab uses more BAR space upfront than three 4 KB pages. But the slab is reused across the program lifetime (grow-only), and the ioctl reduction dominates in latency-sensitive paths.

## BAR Budget and Page Math

The 4 MiB BAR is partitioned by the device-side driver into control structures (~24 KB) and user data. Each buffer allocation consumes data pages plus page-table pages (1 PT page per 511 data pages).

```mermaid
flowchart LR
    subgraph bar["PCIe BAR 2 · 4 MiB (1,024 pages)"]
        direction TB
        ctrl["Control Structures<br/>queues + mailboxes<br/>~6 pages"]
        user["User Data Region<br/>~1,018 pages available"]
        guard["Pool Guard<br/>MAX_POOL_PAGES = 900"]
    end

    subgraph budget["Page Budget Examples"]
        ex1["64 KB slab:<br/>16 data + 1 PT = 17 pages"]
        ex2["1 MiB slab:<br/>256 data + 1 PT = 257 pages"]
        ex3["HPL (N=360):<br/>~322 pages total<br/>ELF + matrix + panels"]
    end

    user --> guard
    guard --> budget

    style ctrl fill:#c94,color:#fff
    style guard fill:#cc4,color:#333
    style user fill:#4a9,color:#fff
```

The pool enforces a hard limit of 900 pages (~3.5 MiB), leaving ~118 pages of headroom below the 1,018-page BAR capacity. This prevents the pool from consuming the entire BAR and leaving no room for error recovery or unexpected allocations.

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
