//====-RTLs/thunderbird/src/rtl.cpp - Target RTLs Implementation - C++ -*-=====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// RTL NextGen for InspireSemi Thunderbird
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ffi.h>
#include <string>
#include <variant>
#include <unordered_map>
#include <vector>

#include "Shared/Debug.h"
#include "Shared/Environment.h"
#include "Utils/ELF.h"

#include "GlobalHandler.h"
#include "OpenMP/OMPT/Callback.h"
#include "PluginInterface.h"
#include "omptarget.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Frontend/OpenMP/OMPConstants.h"
#include "llvm/Frontend/OpenMP/OMPDeviceConstants.h"
#include "llvm/Frontend/OpenMP/OMPGridValues.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/BinaryFormat/ELF.h"

// New offload-platform API
#include "tbird_offload_api.h"
#include "internal/tbird_types_internal.h"

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) ||           \
    !defined(__ORDER_BIG_ENDIAN__)
#error "Missing preprocessor definitions for endianness detection."
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define LITTLEENDIAN_CPU
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BIGENDIAN_CPU
#endif

// ---------------------------------------------------------------------------
// Memory Pool — bump allocator over tbird_buffer_t slabs
// ---------------------------------------------------------------------------
// All device memory allocations (ELF images, data maps, scalars) go through
// this pool. Sub-allocations within a slab reuse the slab's shared_key and
// are addressed via offset for write/read/launch operations. The pool is
// grow-only; slabs are freed in destroy() at device shutdown.
// NOTE: not thread-safe — the Thunderbird RTL uses a single mailbox.

struct MemoryPool {
  static constexpr size_t INITIAL_SLAB_SIZE = 64 * 1024;  // 64 KB
  static constexpr size_t ALIGNMENT = 16;
  static constexpr size_t PAGE_SIZE = 4096;
  // The 4 MiB BAR (~1018 usable pages) is shared among up to four
  // concurrent mailboxes.  Cap each instance's slab footprint at 750
  // pages (~3 MiB) so a second concurrent instance can still start
  // (1018 - 750 = 268 pages ~ 1 MiB for the second instance).
  // HPL at N=360 consumes significantly more pages than the
  // theoretical ~330 estimate due to grow-only scalar accumulation
  // across hundreds of target regions.
  static constexpr size_t MAX_POOL_PAGES = 750;

  struct Slab {
    tbird_buffer_t buffer;
    void *base;       // host VA from tbird_buffer_host_ptr()
    size_t capacity;
    size_t watermark;  // next free offset
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

  void init(tbird_context_t context) {
    ctx = context;
    total_pages = 0;
  }

  /// Bump-allocate `size` bytes. Returns host VA usable as OpenMP "device ptr".
  void *allocate(size_t size) {
    size_t aligned = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

    // Try current (last) slab
    if (!slabs.empty()) {
      Slab &cur = slabs.back();
      if (cur.watermark + aligned <= cur.capacity) {
        void *ptr = (char *)cur.base + cur.watermark;
        allocations[ptr] = {slabs.size() - 1, cur.watermark, size};
        cur.watermark += aligned;
        DP("POOL: sub-alloc %zu bytes in slab %zu at offset %zu → %p\n",
           size, slabs.size() - 1, cur.watermark - aligned, ptr);
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
    slabs.push_back({buf, base, slab_size, 0});
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
    DP("POOL: sub-alloc %zu bytes in new slab %zu at offset 0 → %p\n",
       size, slabs.size() - 1, ptr);
    return ptr;
  }

  /// Look up which slab and offset a pointer maps to (exact + interior).
  std::pair<tbird_buffer_t, size_t> lookup(void *ptr) {
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

  /// Remove a sub-allocation from tracking (no DMA release — grow-only).
  void deallocate(void *ptr) {
    allocations.erase(ptr);
  }

  /// Free all slabs. Call from deinitImpl().
  void destroy() {
    if (!ctx) return;
    for (auto &slab : slabs)
      tbird_free_buffer(ctx, slab.buffer);
    slabs.clear();
    allocations.clear();
    total_pages = 0;
    DP("POOL: destroyed all slabs\n");
  }
};

// The number of devices in this plugin.
#define THUNDERBIRD_NUM_DEVICES 1

// The maximum number of physical cores in this plugin.
#define THUNDERBIRD_MAX_THREADS 6144
#define THUNDERBIRD_MAX_THREADS_XILINX 4
#define THUNDERBIRD_MAX_THREADS_QEMU 64

// IVSHMEM base address for the purposes of Thunderbird.
constexpr uint64_t IVSHMEM_BASE_ADDRESS = 0x82000000000000ull;

/// Helper: Convert OpenMP C type enum to tbird_arg_type_t (Phase 4)
static tbird_arg_type_t convert_omp_ctype_to_tbird(uint8_t omp_ctype) {
  // OpenMP C types from offload/include/omptarget.h lines 135-152
  switch (omp_ctype) {
    case 0:  // OMP_TGT_CTYPE_VOID
      return TBIRD_TYPE_VOID;
    case 1:  // OMP_TGT_CTYPE_INT8
      return TBIRD_TYPE_INT8;
    case 2:  // OMP_TGT_CTYPE_UINT8
      return TBIRD_TYPE_UINT8;
    case 3:  // OMP_TGT_CTYPE_INT16
      return TBIRD_TYPE_INT16;
    case 4:  // OMP_TGT_CTYPE_UINT16
      return TBIRD_TYPE_UINT16;
    case 5:  // OMP_TGT_CTYPE_INT32
      return TBIRD_TYPE_INT32;
    case 6:  // OMP_TGT_CTYPE_UINT32
      return TBIRD_TYPE_UINT32;
    case 7:  // OMP_TGT_CTYPE_INT64
      return TBIRD_TYPE_INT64;
    case 8:  // OMP_TGT_CTYPE_UINT64
      return TBIRD_TYPE_UINT64;
    case 9:  // OMP_TGT_CTYPE_FLOAT
      return TBIRD_TYPE_FLOAT;
    case 10: // OMP_TGT_CTYPE_DOUBLE
      return TBIRD_TYPE_DOUBLE;
    case 11: // OMP_TGT_CTYPE_POINTER
      return TBIRD_TYPE_PTR;
    default:
      // Unknown type - default to INT64 for safety
      return TBIRD_TYPE_INT64;
  }
}

namespace llvm {
namespace omp {
namespace target {
namespace plugin {

/// Forward declarations for all specialized data structures.
struct ThunderbirdKernelTy;
struct ThunderbirdDeviceTy;
struct ThunderbirdPluginTy;

using llvm::sys::DynamicLibrary;
using namespace error;

/// Class implementing kernel functionalities for Thunderbird.
struct ThunderbirdKernelTy : public GenericKernelTy {
  /// Construct the kernel with a name and an execution mode.
  ThunderbirdKernelTy(const char *Name) : GenericKernelTy(Name), Func(nullptr) {}

  /// Initialize the kernel.
  Error initImpl(GenericDeviceTy &Device, DeviceImageTy &Image) override;

  Error launchImpl(GenericDeviceTy &GenericDevice, uint32_t NumThreads[3],
                   uint32_t NumBlocks[3], KernelArgsTy &KernelArgs,
                   KernelLaunchParamsTy LaunchParams,
                   AsyncInfoWrapperTy &AsyncInfoWrapper) const override;


private:
  /// The kernel function to execute.
  void (*Func)(void);
  
  /// Image buffer handle containing this kernel's ELF image (Phase 4)
  tbird_buffer_t image_buffer = nullptr;
  /// Offset of ELF image within pool slab
  size_t kernel_elf_offset = 0;
};

/// Class implementing the Thunderbird device images properties.
struct ThunderbirdDeviceImageTy : public DeviceImageTy {
  /// Create the Thunderbird image with the id and the target image pointer.
  ThunderbirdDeviceImageTy(int32_t ImageId, GenericDeviceTy &Device,
                        const __tgt_device_image *TgtImage)
      : DeviceImageTy(ImageId, Device, TgtImage), DynLib() {}

  /// Getter and setter for the dynamic library.
  DynamicLibrary &getDynamicLibrary() { return DynLib; }
  void setDynamicLibrary(const DynamicLibrary &Lib) { DynLib = Lib; }
  uint64_t &getBaseImageAddress() { return TBirdImageAddress; }
  void setBaseImageAddress(const uint64_t &Address) { TBirdImageAddress = Address; }

  void makeFuncTable(){
    llvm::ArrayRef<llvm::offloading::EntryTy> Entries(
      getTgtImage()->EntriesBegin, getTgtImage()->EntriesEnd);
    for (const auto &Entry : Entries) {
      // TODO: Verify that this if statement checks for this entry being a function
      if (Entry.Size != 0)
        continue;

      FuncTable[std::string(Entry.SymbolName)] = &Entry;

  }
  }
    const llvm::offloading::EntryTy * getEntryForName(std::string &name){
       return FuncTable[name];
     }

  uintptr_t MinVMA;

  /// Buffer handle for loaded image (Phase 3 migration)
  tbird_buffer_t image_buffer = nullptr;
  /// Offset of ELF image within the pool slab (for elf_offset in kernel launch)
  size_t elf_offset = 0;

private:
  /// The dynamic library that loaded the image.
  DynamicLibrary DynLib;

  /// The Thunderbird-side address containing the image
  uint64_t TBirdImageAddress;


  // Since the device won't have a mapping from name to function,
  // we have to. This is a way to do that.
  std::map<std::string, const llvm::offloading::EntryTy *> FuncTable;
};

/// Class implementing the device functionalities for Thunderbird.
struct ThunderbirdDeviceTy : public GenericDeviceTy {
  /// Create the device with a specific id.
  ThunderbirdDeviceTy(GenericPluginTy &Plugin, int32_t DeviceId,
                   int32_t NumDevices)
      : GenericDeviceTy(Plugin, DeviceId, NumDevices, ThunderbirdGridValues) {}

  ~ThunderbirdDeviceTy() {}

  /// Initialize the device
  Error initImpl(GenericPluginTy &Plugin) override {
    DP("=== Phase 2/3: initImpl START ===\n");
    fprintf(stderr, "[THUNDERBIRD RTL] Device initImpl called\n");
    fflush(stderr);
    
    // Get device path from environment or use default
    const char *device_path = getenv("THUNDERBIRD_DEVICE_PATH");
    if (!device_path) {
      device_path = "/dev/tbird0018-0";
      DP("Using default device path: %s\n", device_path);
    } else {
      DP("Using environment device path: %s\n", device_path);
    }
    
    DP("Calling tbird_init(device_path=%s, num_mailboxes=1)\n", device_path);
    
    // Initialize context with single mailbox (single-threaded operation)
    ctx = tbird_init(device_path, 1);
    if (!ctx) {
      DP("ERROR: tbird_init returned NULL\n");
      return Plugin::error(ErrorCode::UNKNOWN,
                          "Failed to initialize Thunderbird context: device=%s", 
                          device_path);
    }
    
    DP("SUCCESS: Thunderbird context initialized: ctx=%p\n", (void*)ctx);
    DP("Setting MaxNumThreads=%d\n", THUNDERBIRD_MAX_THREADS);

    MaxNumThreads = THUNDERBIRD_MAX_THREADS;

    pool.init(ctx);
    DP("Memory pool initialized\n");

    DP("=== Phase 2/3: initImpl COMPLETE ===\n");
    fprintf(stderr, "[THUNDERBIRD RTL] Device initImpl COMPLETE\n");
    fflush(stderr);
    return Plugin::success();
  }

  /// Unload the binary image
  ///
  /// Unload the binary image and free associated resources
  Error unloadBinaryImpl(DeviceImageTy *Image) override {
    DP("=== Phase 3: unloadBinaryImpl START ===\n");
    
    auto TBirdImage = reinterpret_cast<ThunderbirdDeviceImageTy *>(Image);
    
    if (!TBirdImage) {
      DP("WARNING: Image is NULL\n");
      return Plugin::success();
    }
    
    DP("Unloading image: Image=%p, image_buffer=%p\n",
       (void*)TBirdImage, (void*)TBirdImage->image_buffer);

    // Pool manages image buffer lifetime — no per-image free.
    // The slab is released in pool.destroy() at device shutdown.
    if (TBirdImage->image_buffer) {
      DP("Image buffer %p managed by pool (offset %zu) — no free\n",
         (void*)TBirdImage->image_buffer, TBirdImage->elf_offset);
      TBirdImage->image_buffer = nullptr;
    } else {
      DP("No image buffer to free\n");
    }

    DP("Freeing Image object\n");
    Plugin.free(TBirdImage);
    
    DP("=== Phase 3: unloadBinaryImpl COMPLETE ===\n");

    return Plugin::success();
  }

  /// Deinitialize the device — release all pool slabs.
  Error deinitImpl() override {
    pool.destroy();
    return Plugin::success();
  }

  /// See GenericDeviceTy::getComputeUnitKind().
  std::string getComputeUnitKind() const override { return "thunderbird-64bit"; }

  /// Construct the kernel for a specific image on the device.
  Expected<GenericKernelTy &> constructKernel(const char *Name) override {
    // Allocate and construct the kernel.
    ThunderbirdKernelTy *ThunderbirdKernel = Plugin.allocate<ThunderbirdKernelTy>();
    if (!ThunderbirdKernel)
      return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                           "failed to allocate memory for Thunderbird kernel");

    new (ThunderbirdKernel) ThunderbirdKernelTy(Name);

    return *ThunderbirdKernel;
  }

  /// Set the current context to this device, which is a no-op.
  // TODO: how do we specify the current context for Thunderbird?
  Error setContext() override { return Plugin::success(); }

  /// Load the binary image into the device and allocate an image object.
  Expected<DeviceImageTy *> loadBinaryImpl(const __tgt_device_image *TgtImage,
                                           int32_t ImageId) override {
    DP("=== Phase 3: loadBinaryImpl START (ImageId=%d) ===\n", ImageId);
    fprintf(stderr, "[THUNDERBIRD RTL] loadBinaryImpl called (ImageId=%d)\n", ImageId);
    fflush(stderr);
    
    // Validate context
    if (!ctx) {
      DP("ERROR: ctx is NULL, cannot load image\n");
      return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in loadBinaryImpl");
    }
    
    // Validate TgtImage
    if (!TgtImage) {
      DP("ERROR: TgtImage is NULL\n");
      return Plugin::error(ErrorCode::INVALID_BINARY, "TgtImage is NULL");
    }
    
    if (!TgtImage->ImageStart || !TgtImage->ImageEnd) {
      DP("ERROR: TgtImage pointers invalid: ImageStart=%p, ImageEnd=%p\n",
         TgtImage->ImageStart, TgtImage->ImageEnd);
      return Plugin::error(ErrorCode::INVALID_BINARY, "Invalid image pointers");
    }
    
    // Allocate and construct image object
    DP("Allocating ThunderbirdDeviceImageTy object\n");
    ThunderbirdDeviceImageTy *Image = Plugin.allocate<ThunderbirdDeviceImageTy>();
    if (!Image) {
      DP("ERROR: Failed to allocate ThunderbirdDeviceImageTy\n");
      return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                          "failed to allocate memory for device image");
    }
    
    DP("Constructing ThunderbirdDeviceImageTy (Image=%p)\n", (void*)Image);
    new (Image) ThunderbirdDeviceImageTy(ImageId, *this, TgtImage);
    
    // Get image size from ELF (cast void* to char* for pointer arithmetic)
    size_t ImageSize = reinterpret_cast<const char*>(TgtImage->ImageEnd) - 
                       reinterpret_cast<const char*>(TgtImage->ImageStart);
    
    DP("Image info: ImageStart=%p, ImageEnd=%p, Size=%zu bytes\n",
       TgtImage->ImageStart, TgtImage->ImageEnd, ImageSize);
    
    if (ImageSize == 0 || ImageSize > (1024 * 1024 * 100)) { // Sanity: 0-100MB
      DP("ERROR: Suspicious image size: %zu bytes\n", ImageSize);
      Plugin.free(Image);
      return Plugin::error(ErrorCode::INVALID_BINARY, "Invalid image size: %zu", ImageSize);
    }
    
    // Allocate image through pool
    DP("POOL: allocating image buffer: %zu bytes\n", ImageSize);

    void *img_ptr = pool.allocate(ImageSize);
    if (!img_ptr) {
      DP("ERROR: pool.allocate failed for image (%zu bytes)\n", ImageSize);
      Plugin.free(Image);
      return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                          "pool.allocate failed for image (%zu bytes)",
                          ImageSize);
    }

    // Resolve slab buffer and offset for this sub-allocation
    auto [image_buf, elf_off] = pool.lookup(img_ptr);
    DP("POOL: image at slab buffer=%p, elf_offset=%zu\n",
       (void*)image_buf, elf_off);

    // Upload image to shared buffer at the pool-assigned offset
    tbird_status_t status = tbird_buffer_write(ctx, image_buf, elf_off,
                                               TgtImage->ImageStart, ImageSize);
    if (status != TBIRD_SUCCESS) {
      DP("ERROR: tbird_buffer_write failed for image: %s\n", tbird_last_error(ctx));
      Plugin.free(Image);
      return Plugin::error(ErrorCode::UNKNOWN,
                          "tbird_buffer_write failed for image: %s",
                          tbird_last_error(ctx));
    }

    void *host_ptr = img_ptr;
    DP("SUCCESS: Image uploaded via pool: buffer=%p, elf_offset=%zu\n",
       (void*)image_buf, elf_off);
    
    // Store buffer handle and pool offset in image object
    DP("Storing image_buffer=%p, elf_offset=%zu in Image object\n",
       (void*)image_buf, elf_off);
    Image->image_buffer = image_buf;
    Image->elf_offset = elf_off;
    
    // Set base address to host pointer (for symbol resolution)
    // This allows getGlobalMetadataFromDevice to calculate offsets
    uint64_t base_addr = reinterpret_cast<uint64_t>(host_ptr);
    DP("Setting base address to host_ptr: 0x%lx\n", base_addr);
    Image->setBaseImageAddress(base_addr);
    
    // Calculate MinVMA for symbol offset computation
    // Use LLVM's ELF parser API
    DP("Parsing ELF to calculate MinVMA\n");
    auto ElfOrErr = llvm::object::ELF64LEFile::create(
        llvm::StringRef(reinterpret_cast<const char*>(TgtImage->ImageStart), ImageSize));
    if (!ElfOrErr) {
      DP("ERROR: Failed to parse ELF image\n");
      tbird_free_buffer(ctx, image_buf);
      Plugin.free(Image);
      return Plugin::error(ErrorCode::INVALID_BINARY,
                          "failed to parse ELF image");
    }
    
    DP("ELF parsed successfully\n");
    
    Image->MinVMA = ~0u;
    auto PhdrsOrErr = ElfOrErr->program_headers();
    if (!PhdrsOrErr) {
      DP("ERROR: Failed to read ELF program headers\n");
      tbird_free_buffer(ctx, image_buf);
      Plugin.free(Image);
      return Plugin::error(ErrorCode::INVALID_BINARY,
                          "failed to read ELF program headers");
    }
    
    DP("Reading program headers to find MinVMA\n");
    int pt_load_count = 0;
    for (const auto &PHdr : *PhdrsOrErr) {
      if (PHdr.p_type == llvm::ELF::PT_LOAD) {
        pt_load_count++;
        DP("  PT_LOAD: p_vaddr=0x%lx, p_memsz=%zu\n", 
           (unsigned long)PHdr.p_vaddr, (size_t)PHdr.p_memsz);
        if (PHdr.p_vaddr < Image->MinVMA)
          Image->MinVMA = PHdr.p_vaddr;
      }
    }
    
    DP("Found %d PT_LOAD segments, MinVMA=0x%lx\n", pt_load_count, Image->MinVMA);
    
    if (Image->MinVMA == (uintptr_t)~0u) {
      DP("WARNING: No PT_LOAD segments found, MinVMA remains ~0\n");
    }
    
    // Build function table for kernel name lookups
    DP("Building function table\n");
    Image->makeFuncTable();
    DP("Function table built\n");
    
    DP("SUCCESS: Image loaded: MinVMA=0x%lx, base_addr=0x%lx\n", 
       Image->MinVMA, base_addr);
    DP("=== Phase 3: loadBinaryImpl COMPLETE ===\n");
    
    return Image;
  }

  /// Allocate memory. Use std::malloc in all cases.
  // TODO: switch the malloc below for the target malloc
  void *allocate(size_t Size, void *, TargetAllocTy Kind) override {
    DP("=== Phase 2: allocate(Size=%zu, Kind=%d) ===\n", Size, (int)Kind);
    
    if (Size == 0) {
      DP("WARNING: Requested allocation of size 0\n");
      return nullptr;
    }

    void *MemAlloc = nullptr;
    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
    case TARGET_ALLOC_DEVICE_NON_BLOCKING:
      {
        if (!ctx) {
          DP("ERROR: ctx is NULL, cannot allocate buffer\n");
          return nullptr;
        }

        void *ptr = pool.allocate(Size);
        if (!ptr) {
          DP("ERROR: pool.allocate(%zu) failed\n", Size);
          return nullptr;
        }

        DP("SUCCESS: pool.allocate(%zu) → %p (now %zu tracked)\n",
           Size, ptr, pool.allocations.size());
        return ptr;
      }
    case TARGET_ALLOC_HOST:
      DP("HOST allocation (using malloc)\n");
      MemAlloc = std::malloc(Size);
      break;
    }
    return MemAlloc;
  }

  /// Free device memory — removes from pool tracking (grow-only, no DMA release).
  int free(void *TgtPtr, TargetAllocTy Kind) override {
    DP("=== free(TgtPtr=%p, Kind=%d) ===\n", TgtPtr, (int)Kind);

    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
    case TARGET_ALLOC_DEVICE_NON_BLOCKING:
      pool.deallocate(TgtPtr);
      DP("SUCCESS: pool.deallocate(%p) (now %zu tracked)\n",
         TgtPtr, pool.allocations.size());
      return OFFLOAD_SUCCESS;
    case TARGET_ALLOC_HOST:
      std::free(TgtPtr);
      return OFFLOAD_SUCCESS;
    }
    return OFFLOAD_FAIL;
  }

  /// This plugin does nothing to lock buffers. Do not return an error, just
  /// return the same pointer as the device pointer.
  Expected<void *> dataLockImpl(void *HstPtr, int64_t Size) override {
    return HstPtr;
  }

  /// Nothing to do when unlocking the buffer.
  Error dataUnlockImpl(void *HstPtr) override { return Plugin::success(); }

  /// Indicate that the buffer is not pinned.
  Expected<bool> isPinnedPtrImpl(void *HstPtr, void *&BaseHstPtr,
                                 void *&BaseDevAccessiblePtr,
                                 size_t &BaseSize) const override {
    return false;
  }

  Error dataSubmitImpl(void *TgtPtr, const void *HstPtr, int64_t Size,
                      AsyncInfoWrapperTy &AsyncInfoWrapper) override {
  DP("=== Phase 2: dataSubmit(TgtPtr=%p, HstPtr=%p, Size=%ld) ===\n", 
     TgtPtr, HstPtr, Size);
  
  // Validate inputs
  if (!ctx) {
    DP("ERROR: ctx is NULL\n");
    return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in dataSubmitImpl");
  }
  
  if (!HstPtr) {
    DP("ERROR: HstPtr is NULL\n");
    return Plugin::error(ErrorCode::UNKNOWN, "HstPtr is NULL");
  }
  
  if (Size <= 0) {
    DP("WARNING: Size=%ld is non-positive\n", Size);
  }
  
  // Look up buffer handle (supports interior pointers)
  DP("Looking up TgtPtr in pool (%zu tracked)\n", pool.allocations.size());
  auto [buffer, offset] = findContainingBuffer(TgtPtr);
  if (!buffer) {
    DP("ERROR: pointer %p not in buffer registry\n", TgtPtr);
    return Plugin::error(ErrorCode::UNKNOWN,
                        "dataSubmit: pointer %p not in buffer registry", TgtPtr);
  }

  DP("Found buffer=%p for TgtPtr=%p (offset=%zu)\n", (void*)buffer, TgtPtr, offset);

  // Transfer via driver ioctl
  DP("Calling tbird_buffer_write(ctx=%p, buffer=%p, offset=%zu, src=%p, size=%ld)\n",
     (void*)ctx, (void*)buffer, offset, HstPtr, Size);

  tbird_status_t status = tbird_buffer_write(ctx, buffer, offset, HstPtr, Size);
  if (status != TBIRD_SUCCESS) {
    DP("ERROR: tbird_buffer_write failed: %s\n", tbird_last_error(ctx));
    return Plugin::error(ErrorCode::UNKNOWN,
                        "tbird_buffer_write failed: %s", tbird_last_error(ctx));
  }
  
  DP("SUCCESS: dataSubmit: %ld bytes to buffer %p\n", Size, (void*)buffer);
  
  return Plugin::success();
}

  /// Retrieve data from the device (device to host transfer).
  Error dataRetrieveImpl(void *HstPtr, const void *TgtPtr, int64_t Size,
                         AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    DP("=== Phase 2: dataRetrieve(HstPtr=%p, TgtPtr=%p, Size=%ld) ===\n",
       HstPtr, TgtPtr, Size);
    
    // Validate inputs
    if (!ctx) {
      DP("ERROR: ctx is NULL\n");
      return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in dataRetrieveImpl");
    }
    
    if (!HstPtr) {
      DP("ERROR: HstPtr is NULL\n");
      return Plugin::error(ErrorCode::UNKNOWN, "HstPtr is NULL");
    }
    
    if (Size <= 0) {
      DP("WARNING: Size=%ld is non-positive\n", Size);
    }
    
    // Look up buffer handle (supports interior pointers)
    DP("Looking up TgtPtr in pool (%zu tracked)\n", pool.allocations.size());
    auto [buffer, offset] = findContainingBuffer(const_cast<void*>(TgtPtr));
    if (!buffer) {
      DP("ERROR: pointer %p not in buffer registry\n", TgtPtr);
      return Plugin::error(ErrorCode::UNKNOWN,
                          "dataRetrieve: pointer %p not in buffer registry", TgtPtr);
    }

    DP("Found buffer=%p for TgtPtr=%p (offset=%zu)\n", (void*)buffer, TgtPtr, offset);

    // Transfer via driver ioctl
    DP("Calling tbird_buffer_read(ctx=%p, buffer=%p, offset=%zu, dst=%p, size=%ld)\n",
       (void*)ctx, (void*)buffer, offset, HstPtr, Size);

    tbird_status_t status = tbird_buffer_read(ctx, buffer, offset, HstPtr, Size);
    if (status != TBIRD_SUCCESS) {
      DP("ERROR: tbird_buffer_read failed: %s\n", tbird_last_error(ctx));
      return Plugin::error(ErrorCode::UNKNOWN,
                          "tbird_buffer_read failed: %s", tbird_last_error(ctx));
    }
    
    DP("SUCCESS: dataRetrieve: %ld bytes from buffer %p\n", Size, (void*)buffer);
    
    return Plugin::success();
  }

  /// Exchange data between two devices within the plugin. This function is not
  /// supported in this plugin.
  Error dataExchangeImpl(const void *SrcPtr, GenericDeviceTy &DstGenericDevice,
                         void *DstPtr, int64_t Size,
                         AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    // This function should never be called because the function
    // ThunderbirdPluginTy::isDataExchangable() returns false.
    return Plugin::error(ErrorCode::UNSUPPORTED,
                         "dataExchangeImpl not supported");
  }

  /// All functions are already synchronous. No need to do anything on this
  /// synchronization function.
  Error synchronizeImpl(__tgt_async_info &AsyncInfo) override {
    return Plugin::success();
  }

  /// All functions are already synchronous. No need to do anything on this
  /// query function.
  Error queryAsyncImpl(__tgt_async_info &AsyncInfo) override {
    return Plugin::success();
  }

  /// This plugin does not support interoperability
  Error initAsyncInfoImpl(AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    return Plugin::error(ErrorCode::UNSUPPORTED,
                         "initAsyncInfoImpl not supported");
  }

  /// This plugin does not support interoperability
  Error initDeviceInfoImpl(__tgt_device_info *DeviceInfo) override {
    return Plugin::error(ErrorCode::UNSUPPORTED,
                         "initDeviceInfoImpl not supported");
  }

  /// This plugin does not support the event API. Do nothing without failing.
  Error createEventImpl(void **EventPtrStorage) override {
    *EventPtrStorage = nullptr;
    return Plugin::success();
  }
  Error destroyEventImpl(void *EventPtr) override { return Plugin::success(); }
  Error recordEventImpl(void *EventPtr,
                        AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    return Plugin::success();
  }
  Error waitEventImpl(void *EventPtr,
                      AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    return Plugin::success();
  }
  Error syncEventImpl(void *EventPtr) override { return Plugin::success(); }

  /// Print information about the device.
  Expected<InfoTreeNode> obtainInfoImpl() override {
    InfoTreeNode Info;
    Info.add("Device Type", "Thunderbird RISC-V-64bit");
    return Info;
  }

  /// This plugin should not setup the device environment or memory pool.
  virtual bool shouldSetupDeviceEnvironment() const override { return false; };
  virtual bool shouldSetupDeviceMemoryPool() const override { return false; };

  /// Getters and setters for stack size and heap size not relevant.
  Error getDeviceStackSize(uint64_t &Value) override {
    Value = 0;
    return Plugin::success();
  }
  Error setDeviceStackSize(uint64_t Value) override {
    return Plugin::success();
  }
  Error getDeviceHeapSize(uint64_t &Value) override {
    Value = 0;
    return Plugin::success();
  }
  Error setDeviceHeapSize(uint64_t Value) override { return Plugin::success(); }

  /// New offload-platform API context (Phase 5: old channels removed)
  tbird_context_t ctx = nullptr;

  /// Unified memory pool — all allocations (ELF + data) go through this.
  MemoryPool pool;

  /// Find the buffer containing ptr (supports interior pointers).
  /// Delegates to the memory pool for all allocations.
  std::pair<tbird_buffer_t, size_t> findContainingBuffer(void *ptr) {
    return pool.lookup(ptr);
  }

private:
  /// Grid values for Thunderbird plugins.
  static constexpr GV ThunderbirdGridValues = {
      1, // GV_Slot_Size
      1, // GV_Warp_Size
      THUNDERBIRD_MAX_THREADS, // GV_Max_Teams
      1, // GV_Default_Num_Teams
      1, // GV_SimpleBufferSize
      1, // GV_Max_WG_Size
      1, // GV_Default_WG_Size
  };

  /// Thunderbird write and read channels
  uint32_t MaxNumThreads = 0;
};

/// Implementation of ThunderbirdKernelTy::initImpl (defined after ThunderbirdDeviceImageTy)
Error ThunderbirdKernelTy::initImpl(GenericDeviceTy &Device, DeviceImageTy &Image) {
  DP("=== Phase 4: ThunderbirdKernelTy::initImpl START (kernel=%s) ===\n", getName());
  fprintf(stderr, "[THUNDERBIRD RTL] Kernel initImpl called for: %s\n", getName());
  fflush(stderr);
  
  // Get image buffer from DeviceImageTy
  auto &TBirdImage = static_cast<ThunderbirdDeviceImageTy &>(Image);
  image_buffer = TBirdImage.image_buffer;
  kernel_elf_offset = TBirdImage.elf_offset;

  if (!image_buffer) {
    DP("ERROR: Image buffer is NULL for kernel %s\n", getName());
    return Plugin::error(ErrorCode::INVALID_BINARY,
                        "Image buffer not loaded for kernel %s", getName());
  }

  DP("Stored image_buffer=%p, elf_offset=%zu for kernel %s\n",
     (void*)image_buffer, kernel_elf_offset, getName());
 
  // Functions have zero size.
  GlobalTy Global(getName(), 0);

  // Get the metadata (address) of the kernel function.
  GenericGlobalHandlerTy &GHandler = Device.Plugin.getGlobalHandler();
  if (auto Err = GHandler.getGlobalMetadataFromDevice(Device, Image, Global))
    return Err;

  // Check that the function pointer is valid (now a host pointer from Phase 3).
  if (!Global.getPtr()) {
    DP("ERROR: Global.getPtr() returned NULL for kernel %s\n", getName());
    return Plugin::error(ErrorCode::INVALID_BINARY,
                         "invalid function for kernel %s", getName());
  }

  // Save the function pointer (host address for verification, not used in launch).
  Func = (void (*)())Global.getPtr();
  DP("Kernel %s: Func=%p (host address)\n", getName(), (void*)Func);

  KernelEnvironment.Configuration.ExecMode = OMP_TGT_EXEC_MODE_GENERIC;
  KernelEnvironment.Configuration.MayUseNestedParallelism = 2; // Unknown
  KernelEnvironment.Configuration.UseGenericStateMachine = 2;  // Unknown

  DP("=== Phase 4: ThunderbirdKernelTy::initImpl COMPLETE ===\n");
  fprintf(stderr, "[THUNDERBIRD RTL] Kernel initImpl COMPLETE for: %s\n", getName());
  fflush(stderr);
  return Plugin::success();
}

//===----------------------------------------------------------------------===//
// Argument Conversion Helpers for launchImpl
//===----------------------------------------------------------------------===//

/// Context passed to argument conversion helpers
struct ArgConversionContext {
  ThunderbirdDeviceTy *Device;
  KernelArgsTy &KernelArgs;
  KernelLaunchParamsTy &LaunchParams;
  bool IsGenericMode;
  uint32_t KLEOffset; // KernelLaunchEnvironment offset in LaunchParams.Ptrs
};

/// Get OpenMP map type flags for argument (with safe bounds checking)
static std::pair<int64_t, bool> getArgMapType(uint32_t ArgIdx,
                                               const ArgConversionContext &Ctx) {
  int64_t MapType = 0;
  bool HasMapType = false;

  if (Ctx.KernelArgs.ArgTypes && ArgIdx < Ctx.KernelArgs.NumArgs) {
    MapType = Ctx.KernelArgs.ArgTypes[ArgIdx];
    HasMapType = true;
  }

  return {MapType, HasMapType};
}

/// Get scalar type size in bytes
static size_t getScalarSize(tbird_arg_type_t Type) {
  switch (Type) {
    case TBIRD_TYPE_INT8:
    case TBIRD_TYPE_UINT8:
      return 1;
    case TBIRD_TYPE_INT16:
    case TBIRD_TYPE_UINT16:
      return 2;
    case TBIRD_TYPE_INT32:
    case TBIRD_TYPE_UINT32:
    case TBIRD_TYPE_FLOAT:
      return 4;
    case TBIRD_TYPE_INT64:
    case TBIRD_TYPE_UINT64:
    case TBIRD_TYPE_DOUBLE:
      return 8;
    default:
      return 8;
  }
}

/// Convert pointer argument from OpenMP to tbird format
static Error convertPointerArgument(uint32_t OmpIdx, tbird_arg_t &OutArg,
                                     const ArgConversionContext &Ctx) {
  if (!Ctx.LaunchParams.Ptrs) {
    return Plugin::error(ErrorCode::UNKNOWN, "LaunchParams.Ptrs is NULL");
  }

  // LaunchParams.Ptrs is offset by KLEOffset (KernelLaunchEnvironment at [0])
  uint32_t PtrIndex = OmpIdx + Ctx.KLEOffset;
  uint32_t NumPtrs = Ctx.LaunchParams.Size / sizeof(void*);
  if (PtrIndex >= NumPtrs) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "Pointer index %u >= NumPtrs %u", PtrIndex, NumPtrs);
  }

  void *DevicePtr = *(void**)Ctx.LaunchParams.Ptrs[PtrIndex];

  // Verify it's in buffer registry (supports interior pointers)
  auto [buf, ofs] = Ctx.Device->findContainingBuffer(DevicePtr);
  if (!buf) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "Device pointer %p not in buffer registry", DevicePtr);
  }

  OutArg.value.ptr = DevicePtr;
  DP("    PTR: %p (from *LaunchParams.Ptrs[%u])\n", DevicePtr, PtrIndex);
  return Plugin::success();
}

/// Convert scalar argument from OpenMP to tbird format
/// Handles by-value (literal), by-reference, and promotion to PTR
static Error convertScalarArgument(uint32_t OmpIdx, tbird_arg_t &OutArg,
                                    int64_t MapType, bool HasMapType,
                                    const ArgConversionContext &Ctx) {
  bool IsLiteral = (MapType & 0x100); // OMP_TGT_MAPTYPE_LITERAL
  size_t ScalarSize = getScalarSize(OutArg.type);

  // Check if scalar has device memory mapping (by-reference via Ptrs array)
  if (Ctx.LaunchParams.Ptrs && !IsLiteral) {
    uint32_t PtrIndex = OmpIdx + Ctx.KLEOffset;
    if (PtrIndex < Ctx.LaunchParams.Size / sizeof(void*)) {
      void *PotentialDevicePtr = *(void**)Ctx.LaunchParams.Ptrs[PtrIndex];

      DP("    Checking LaunchParams.Ptrs[%u]=%p, dereferenced=*Ptrs[%u]=%p\n",
         PtrIndex, Ctx.LaunchParams.Ptrs[PtrIndex], PtrIndex, PotentialDevicePtr);

      // Check if this is in buffer registry (supports interior pointers)
      auto [foundBuf, foundOfs] = Ctx.Device->findContainingBuffer(PotentialDevicePtr);
      if (foundBuf) {
        // Scalar by-ref with device mapping - treat as PTR
        OutArg.value.ptr = PotentialDevicePtr;
        OutArg.type = TBIRD_TYPE_PTR;
        DP("    Scalar by-ref found in Ptrs[%u] as device ptr: %p -> treating as PTR\n",
           PtrIndex, PotentialDevicePtr);
        return Plugin::success();
      }
    }
  }

  // Firstprivate/by-value scalar
  if (IsLiteral) {
    // ArgPtrs[i] IS the value itself (not a pointer)
    DP("    Scalar by-value: ArgPtrs[%u]=%p (direct value)\n",
       OmpIdx, Ctx.KernelArgs.ArgPtrs[OmpIdx]);

    uintptr_t ValueAsInt = (uintptr_t)Ctx.KernelArgs.ArgPtrs[OmpIdx];

    // Write value with correct size
    switch (OutArg.type) {
      case TBIRD_TYPE_INT8:
      case TBIRD_TYPE_UINT8:
        *(uint8_t*)OutArg.value.scalar_bytes = (uint8_t)ValueAsInt;
        break;
      case TBIRD_TYPE_INT16:
      case TBIRD_TYPE_UINT16:
        *(uint16_t*)OutArg.value.scalar_bytes = (uint16_t)ValueAsInt;
        break;
      case TBIRD_TYPE_INT32:
      case TBIRD_TYPE_UINT32:
        *(uint32_t*)OutArg.value.scalar_bytes = (uint32_t)ValueAsInt;
        break;
      case TBIRD_TYPE_INT64:
      case TBIRD_TYPE_UINT64:
        *(uint64_t*)OutArg.value.scalar_bytes = (uint64_t)ValueAsInt;
        break;
      case TBIRD_TYPE_FLOAT: {
        uint32_t Bits = (uint32_t)ValueAsInt;
        memcpy(OutArg.value.scalar_bytes, &Bits, sizeof(Bits));
        break;
      }
      case TBIRD_TYPE_DOUBLE:
        memcpy(OutArg.value.scalar_bytes, &ValueAsInt, sizeof(ValueAsInt));
        break;
      default:
        *(uint64_t*)OutArg.value.scalar_bytes = ValueAsInt;
    }
  } else {
    // ArgPtrs[i] points to the data (by-reference on host)
    DP("    Scalar by-ref: ArgPtrs[%u]=%p\n", OmpIdx, Ctx.KernelArgs.ArgPtrs[OmpIdx]);

    // Check if this address is a mapped buffer (supports interior pointers)
    auto [scBuf, scOfs] = Ctx.Device->findContainingBuffer(Ctx.KernelArgs.ArgPtrs[OmpIdx]);
    if (scBuf) {
      // Scalar by-ref is actually a pointer to mapped buffer
      DP("    Found in buffer registry -> treating as PTR\n");
      OutArg.type = TBIRD_TYPE_PTR;
      OutArg.value.ptr = Ctx.KernelArgs.ArgPtrs[OmpIdx];
    } else {
      // True scalar by-reference - copy the value
      DP("    Not in buffer registry -> copying %zu bytes as scalar value\n", ScalarSize);
      memcpy(OutArg.value.scalar_bytes, Ctx.KernelArgs.ArgPtrs[OmpIdx], ScalarSize);
    }
  }

  // Debug print
  if (OutArg.type == TBIRD_TYPE_FLOAT) {
    float Val;
    memcpy(&Val, OutArg.value.scalar_bytes, sizeof(float));
    DP("    FLOAT: %f\n", Val);
  } else if (OutArg.type == TBIRD_TYPE_DOUBLE) {
    double Val;
    memcpy(&Val, OutArg.value.scalar_bytes, sizeof(double));
    DP("    DOUBLE: %f\n", Val);
  } else if (ScalarSize <= 4) {
    uint32_t Val;
    memcpy(&Val, OutArg.value.scalar_bytes, ScalarSize);
    DP("    SCALAR%zu: 0x%x (%u)\n", ScalarSize, Val, Val);
  } else {
    uint64_t Val;
    memcpy(&Val, OutArg.value.scalar_bytes, ScalarSize);
    DP("    SCALAR%zu: 0x%lx (%lu)\n", ScalarSize, Val, Val);
  }

  return Plugin::success();
}

/// Convert all OpenMP kernel arguments to tbird_arg_t format
/// Returns the number of converted arguments (excluding VOID types)
///
/// Note: KernelArgs.NumArgs may have been incremented by prepareArgs() to
/// account for the KernelLaunchEnvironment (KLE) at LaunchParams.Ptrs[0].
/// However, the metadata arrays (ArgCTypes, ArgTypes, ArgPtrs) are NOT
/// extended - they still have the original argument count. We detect the
/// KLE offset by comparing LaunchParams entry count vs KernelArgs.NumArgs.
static Expected<uint32_t> convertKernelArguments(tbird_arg_t ArgsOut[TBIRD_MAX_ARGS],
                                                  const ArgConversionContext &Ctx) {
  // KernelArgs.NumArgs was incremented by KLEOffset in prepareArgs(), but
  // the metadata arrays (ArgCTypes, ArgTypes, ArgPtrs) were NOT extended.
  // Use KLEOffset from context to get the original argument count.
  uint32_t OrigNumArgs = Ctx.KernelArgs.NumArgs - Ctx.KLEOffset;
  DP("Converting %u arguments from OpenMP format to tbird_arg_t[] "
     "(NumArgs=%u, KLEOffset=%u, OrigArgs=%u)\n",
     OrigNumArgs, Ctx.KernelArgs.NumArgs, Ctx.KLEOffset, OrigNumArgs);

  memset(ArgsOut, 0, sizeof(tbird_arg_t) * TBIRD_MAX_ARGS);
  uint32_t ActualArgCount = 0;

  // Iterate over the ORIGINAL argument count (metadata array bounds)
  for (uint32_t i = 0; i < OrigNumArgs; i++) {
    // Get type from ArgCTypes (indexed by original arg index)
    uint8_t OmpCType = Ctx.KernelArgs.ArgCTypes ? Ctx.KernelArgs.ArgCTypes[i] : 11;
    tbird_arg_type_t TbirdType = convert_omp_ctype_to_tbird(OmpCType);

    // Skip VOID arguments (padding/internal use)
    if (TbirdType == TBIRD_TYPE_VOID) {
      DP("  arg[%u]: VOID type - skipping\n", i);
      continue;
    }

    ArgsOut[ActualArgCount].type = TbirdType;

    // Get map type flags (indexed by original arg index)
    auto [MapType, HasMapType] = getArgMapType(i, Ctx);
    bool IsLiteral = (MapType & 0x100);

    // Debug output
    DP("  arg[%u -> %u]: omp_ctype=%u -> tbird_type=%d\n",
       i, ActualArgCount, OmpCType, (int)TbirdType);
    DP("    arg_type=0x%lx: LITERAL=%d\n", MapType, IsLiteral);
    DP("    ArgPtrs[%u]=%p\n", i, Ctx.KernelArgs.ArgPtrs[i]);

    // Convert based on type
    Error Err = (TbirdType == TBIRD_TYPE_PTR)
      ? convertPointerArgument(i, ArgsOut[ActualArgCount], Ctx)
      : convertScalarArgument(i, ArgsOut[ActualArgCount], MapType, HasMapType, Ctx);

    if (Err)
      return std::move(Err);

    ActualArgCount++;
  }

  DP("Argument conversion complete. Actual args: %u (skipped %u VOID args)\n",
     ActualArgCount, OrigNumArgs - ActualArgCount);

  return ActualArgCount;
}

/// Prepend thread_id as first argument for GENERIC mode
static void prependThreadId(tbird_arg_t Args[TBIRD_MAX_ARGS], uint32_t &ArgCount) {
  DP("GENERIC mode: Prepending thread_id=0 as first argument\n");

  // Shift all arguments forward by one
  for (uint32_t i = ArgCount; i > 0; i--) {
    Args[i] = Args[i-1];
  }

  // Insert thread_id=0 at position 0
  Args[0].type = TBIRD_TYPE_INT64;
  memset(Args[0].value.scalar_bytes, 0, sizeof(Args[0].value.scalar_bytes));
  ArgCount++;

  DP("After prepending thread_id: actual_arg_count=%u\n", ArgCount);
}

//===----------------------------------------------------------------------===//
// launchImpl - Main kernel launch method
//===----------------------------------------------------------------------===//

Error ThunderbirdKernelTy::launchImpl(GenericDeviceTy &GenericDevice, uint32_t NumThreads[3],
                 uint32_t NumBlocks[3], KernelArgsTy &KernelArgs,
                 KernelLaunchParamsTy LaunchParams,
                 AsyncInfoWrapperTy &AsyncInfoWrapper) const {
  fprintf(stderr, "[THUNDERBIRD RTL] launchImpl START: kernel=%s, NumArgs=%u\n",
          getName(), KernelArgs.NumArgs);
  fflush(stderr);

  DP("=== Phase 4: launchImpl START ===\n");
  DP("Kernel: %s\n", getName());
  DP("NumBlocks: [%u, %u, %u]\n", NumBlocks[0], NumBlocks[1], NumBlocks[2]);
  DP("NumThreads: [%u, %u, %u]\n", NumThreads[0], NumThreads[1], NumThreads[2]);
  DP("NumArgs: %u\n", KernelArgs.NumArgs);

  // Validate argument count
  if (KernelArgs.NumArgs > TBIRD_MAX_ARGS) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "Too many kernel arguments: %u (max %d)",
                        KernelArgs.NumArgs, TBIRD_MAX_ARGS);
  }

  // Cast to Thunderbird device
  auto *TBirdDevice = static_cast<ThunderbirdDeviceTy *>(&GenericDevice);
  DP("Using ctx=%p, image_buffer=%p\n", (void*)TBirdDevice->ctx, (void*)image_buffer);

  // Detect execution mode
  bool IsGeneric = (NumThreads[0] == 1 && NumThreads[1] == 1 && NumThreads[2] == 1);
  DP("Execution mode: %s (threads=%u)\n",
     IsGeneric ? "GENERIC" : "SPMD",
     NumThreads[0] * NumThreads[1] * NumThreads[2]);

  // Debug: Print LaunchParams.Ptrs array
  if (LaunchParams.Ptrs) {
    size_t NumPtrs = LaunchParams.Size / sizeof(void*);
    DP("LaunchParams.Ptrs array (%zu entries):\n", NumPtrs);
    for (size_t i = 0; i < NumPtrs; i++) {
      void *Entry = LaunchParams.Ptrs[i];
      void *Deref = Entry ? *(void**)Entry : nullptr;
      DP("  Ptrs[%zu]=%p -> *Ptrs[%zu]=%p\n", i, Entry, i, Deref);
    }
  }

  // Debug: Print pool state
  DP("Pool: %zu tracked allocations, %zu slabs\n",
     TBirdDevice->pool.allocations.size(), TBirdDevice->pool.slabs.size());

  // Detect KLE offset: prepareArgs() may have inserted a KernelLaunchEnvironment
  // pointer at LaunchParams.Ptrs[0] and incremented KernelArgs.NumArgs, but the
  // metadata arrays (ArgCTypes, ArgTypes, ArgPtrs) were not extended.
  uint32_t KLEOffset = 0;
  if (LaunchParams.Ptrs) {
    void *FirstVal = *(void**)LaunchParams.Ptrs[0];
    if (FirstVal == reinterpret_cast<void*>(~0ULL)) {
      KLEOffset = 1;
      DP("Detected KernelLaunchEnvironment at Ptrs[0] (KLEOffset=1)\n");
    }
  }

  // Convert OpenMP arguments to tbird format
  tbird_arg_t Args[TBIRD_MAX_ARGS];
  ArgConversionContext Ctx{TBirdDevice, KernelArgs, LaunchParams, IsGeneric, KLEOffset};

  auto ArgCountOrErr = convertKernelArguments(Args, Ctx);
  if (!ArgCountOrErr)
    return ArgCountOrErr.takeError();

  uint32_t ArgCount = *ArgCountOrErr;

  // Insert thread_id for GENERIC mode
  if (IsGeneric && ArgCount > 0) {
    prependThreadId(Args, ArgCount);
  }

  // Debug: Print final argument array
  DP("Final converted argument array for device:\n");
  for (uint32_t i = 0; i < ArgCount; i++) {
    DP("  args[%u]: type=%d ", i, (int)Args[i].type);
    if (Args[i].type == TBIRD_TYPE_PTR) {
      DP("PTR=%p\n", Args[i].value.ptr);
    } else {
      DP("SCALAR bytes:");
      for (size_t j = 0; j < 8; j++) {
        DP(" %02x", Args[i].value.scalar_bytes[j]);
      }
      DP("\n");
    }
  }

  // Launch kernel — elf_offset points to the ELF start within the pool slab.
  // ImageSize is the full slab size; the device server parses ELF headers
  // starting at elf_offset to determine actual image bounds.
  size_t ImageSize = tbird_buffer_size(image_buffer);
  DP("Calling tbird_launch_kernel_sync: elf_offset=%zu, image_size=%zu, num_args=%u\n",
     kernel_elf_offset, ImageSize, ArgCount);

  tbird_status_t Status = tbird_launch_kernel_sync(
      TBirdDevice->ctx,
      image_buffer,
      kernel_elf_offset,
      ImageSize,
      getName(),   // entry point symbol
      Args,
      ArgCount
  );

  if (Status != TBIRD_SUCCESS) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "tbird_launch_kernel_sync failed: %s",
                        tbird_last_error(TBirdDevice->ctx));
  }

  DP("SUCCESS: Kernel %s completed\n", getName());
  DP("=== Phase 4: launchImpl COMPLETE ===\n");

  return Plugin::success();
}

class ThunderbirdGlobalHandlerTy final : public GenericGlobalHandlerTy {
public:
  Error getGlobalMetadataFromDevice(GenericDeviceTy &GenericDevice,
                                    DeviceImageTy &Image,
                                    GlobalTy &DeviceGlobal) override {

    auto &ThunderbirdImage = static_cast<ThunderbirdDeviceImageTy &>(Image);
    uint64_t DeviceImageBase = ThunderbirdImage.getBaseImageAddress();

    if (DeviceImageBase == 0) {
      return Plugin::error(ErrorCode::UNINITIALIZED,
                           "Device image base address is not set.");
    }

    // Get image data from host
    const __tgt_device_image *TgtImage = ThunderbirdImage.getTgtImage();
    const char *SymbolName = DeviceGlobal.getName().data();
    uintptr_t MinVMA = ThunderbirdImage.MinVMA;



    // Find entry for our symbol.
    for (llvm::offloading::EntryTy *entry = TgtImage->EntriesBegin;
         entry != TgtImage->EntriesEnd; ++entry) {
      if (strcmp(entry->SymbolName, SymbolName) == 0) {
        // Calc symbol offset within the image.
        uint64_t symbol_offset = (uintptr_t)entry->Address - MinVMA;
        uint64_t final_device_address = DeviceImageBase + symbol_offset;

        // Save absolute device address.
        DeviceGlobal.setPtr((void *)final_device_address);
        return Plugin::success();
      }
    }

    return Plugin::error(ErrorCode::NOT_FOUND, "failed to find global '%s' in the image entries",
                         SymbolName);
  }
};

/// Class implementing the plugin functionalities for Thunderbird.
struct ThunderbirdPluginTy final : public GenericPluginTy {
  /// Create the Thunderbird plugin.
  ThunderbirdPluginTy() : GenericPluginTy(getTripleArch()) {}

  /// This class should not be copied.
  ThunderbirdPluginTy(const ThunderbirdPluginTy &) = delete;
  ThunderbirdPluginTy(ThunderbirdPluginTy &&) = delete;

  /// Initialize the plugin and return the number of devices.
  Expected<int32_t> initImpl() override {
#ifdef USES_DYNAMIC_FFI
    if (auto Err = Plugin::check(ffi_init(), "failed to initialize libffi"))
      return std::move(Err);
#endif

    return THUNDERBIRD_NUM_DEVICES;
  }

  /// Deinitialize the plugin.
  Error deinitImpl() override { return Plugin::success(); }

  /// Creates a generic ELF device.
  GenericDeviceTy *createDevice(GenericPluginTy &Plugin, int32_t DeviceId,
                                int32_t NumDevices) override {
    return new ThunderbirdDeviceTy(Plugin, DeviceId, NumDevices);
  }

  /// Creates a generic global handler.
  GenericGlobalHandlerTy *createGlobalHandler() override {
    return new ThunderbirdGlobalHandlerTy();
  }

  /// Get the ELF code to recognize the compatible binary images.
  uint16_t getMagicElfBits() const override {
    return llvm::ELF::EM_RISCV;
  }

  /// This plugin does not support exchanging data between two devices.
  bool isDataExchangable(int32_t SrcDeviceId, int32_t DstDeviceId) override {
    return false;
  }

  /// All images (ELF-compatible) should be compatible with this plugin.
  Expected<bool> isELFCompatible(uint32_t, StringRef) const override {

    return true;
  }

  Triple::ArchType getTripleArch() const override {
    return llvm::Triple::riscv64;
  }

  const char *getName() const override { return GETNAME(TARGET_NAME); }
};

template <typename... ArgsTy>
static Error Plugin::check(int32_t Code, const char *ErrMsg, ArgsTy... Args) {
  if (Code == 0)
    return Plugin::success();

  return Plugin::error(ErrorCode::UNKNOWN, ErrMsg, Args...,
                       std::to_string(Code).data());
}

} // namespace plugin
} // namespace target
} // namespace omp
} // namespace llvm

extern "C" {
llvm::omp::target::plugin::GenericPluginTy *createPlugin_thunderbird() {
  return new llvm::omp::target::plugin::ThunderbirdPluginTy();
}
}
