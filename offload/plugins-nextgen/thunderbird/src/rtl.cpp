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
#include <cstdlib>
#include <ffi.h>
#include <glob.h>
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

// Thunderbird plugin modules
#include "MemoryPool.h"
#include "ArgumentConversion.h"
#include "Topology.h"

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

// MemoryPool and ArgumentConversion are now in separate .h/.cpp files.
// See MemoryPool.h, MemoryPool.cpp, ArgumentConversion.h, ArgumentConversion.cpp.

// The number of OpenMP devices the plugin exposes is discovered at init
// time by enumerating /dev/tbird*-K char devices the host driver creates,
// one per application mailbox. See discoverThunderbirdDevicePaths() below.
// THUNDERBIRD_DEVICE_PATH=<path> forces a single-device legacy mode using
// the supplied path.

// The maximum number of physical cores in this plugin.
#define THUNDERBIRD_MAX_THREADS 6144
#define THUNDERBIRD_MAX_THREADS_XILINX 4
#define THUNDERBIRD_MAX_THREADS_QEMU 64

// IVSHMEM base address for the purposes of Thunderbird.
constexpr uint64_t IVSHMEM_BASE_ADDRESS = 0x82000000000000ull;

namespace llvm {
namespace omp {
namespace target {
namespace plugin {

/// Forward declarations for all specialized data structures.
struct ThunderbirdKernelTy;
struct ThunderbirdDeviceTy;
struct ThunderbirdPluginTy;

/// Application-mailbox device path discovery.
///
/// The host driver (tbird_offload.ko) exposes one /dev/tbird<PCI>-<K> char
/// device per application mailbox. Each maps 1:1 onto an OpenMP device at
/// the libomptarget layer. Discovery is cached in a function-static so
/// initImpl() in plugin and device share a consistent view across calls.
///
/// Override path: setting THUNDERBIRD_DEVICE_PATH=<path> forces a single
/// device using the explicit path (legacy single-device mode).
static const std::vector<std::string> &getThunderbirdDevicePaths() {
  static std::vector<std::string> paths = []() {
    std::vector<std::string> p;
    if (const char *override_path = getenv("THUNDERBIRD_DEVICE_PATH")) {
      p.emplace_back(override_path);
      return p;
    }
    glob_t g;
    if (glob("/dev/tbird*-*", 0, nullptr, &g) == 0) {
      for (size_t i = 0; i < g.gl_pathc; ++i)
        p.emplace_back(g.gl_pathv[i]);
    }
    globfree(&g);
    // glob() returns lexicographically-sorted output; for the current A0
    // cap of 2 ("tbird*-0", "tbird*-1") this produces the right order.
    // If K >= 10 ever ships, switch to numeric-suffix sort.
    return p;
  }();
  return paths;
}

/// Glue: look paths up by DeviceId and call into the Topology module's
/// pure predicate. The string-prefix logic lives in Topology.cpp where
/// it's unit-testable in isolation; this wrapper just resolves int IDs
/// against the cached path list before delegating.
static bool areThunderbirdDevicesCoLocated(int32_t SrcDeviceId,
                                           int32_t DstDeviceId) {
  const auto &paths = getThunderbirdDevicePaths();
  if (SrcDeviceId < 0 || (size_t)SrcDeviceId >= paths.size())
    return false;
  if (DstDeviceId < 0 || (size_t)DstDeviceId >= paths.size())
    return false;
  return thunderbird::topology::arePathsCoLocated(paths[SrcDeviceId],
                                                  paths[DstDeviceId]);
}

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
    DP("=== Phase 2/3: initImpl START (DeviceId=%d) ===\n", DeviceId);

    // Per-DeviceId path lookup. Plugin-level discovery already populated
    // the cached path list; index by DeviceId. Bounds check is a sanity
    // assertion since libomptarget never asks for a DeviceId outside
    // [0, NumDevices) returned by ThunderbirdPluginTy::initImpl().
    const auto &paths = getThunderbirdDevicePaths();
    if (DeviceId < 0 || (size_t)DeviceId >= paths.size()) {
      return Plugin::error(ErrorCode::UNKNOWN,
                           "Thunderbird plugin DeviceId %d out of range "
                           "(discovered %zu device paths)",
                           DeviceId, paths.size());
    }
    const std::string &device_path = paths[DeviceId];
    // Emit per-device-id path on stderr so external tests can verify the
    // discovered path list is indexed correctly (DeviceId K maps to path
    // path[K], not some other element). Format kept stable for parsing.
    fprintf(stderr,
            "[THUNDERBIRD RTL] Device %d initImpl called (path=%s)\n",
            DeviceId, device_path.c_str());
    fflush(stderr);
    DP("DeviceId=%d -> device_path=%s\n", DeviceId, device_path.c_str());

    // Each plugin-level OpenMP device opens a single application mailbox
    // (1:1 mapping). num_mailboxes=1 is intentional: this is the count of
    // mailboxes per tbird_init() call, not the system-wide total.
    ctx = tbird_init(device_path.c_str(), 1);
    if (!ctx) {
      DP("ERROR: tbird_init returned NULL\n");
      return Plugin::error(ErrorCode::UNKNOWN,
                          "Failed to initialize Thunderbird context: device=%s",
                          device_path.c_str());
    }
    
    DP("SUCCESS: Thunderbird context initialized: ctx=%p\n", (void*)ctx);

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

  /// Device-to-device data exchange. Currently always UNSUPPORTED —
  /// ThunderbirdPluginTy::isDataExchangable returns false on every
  /// pair (see the comment there for the slab-pool isolation finding
  /// that retired the previous co-located memcpy fast-path). With
  /// isDataExchangable=false, libomptarget never calls this function;
  /// the UNSUPPORTED return is a defense-in-depth assertion in case
  /// the predicate ever flips back on without a corresponding fast-
  /// path implementation.
  Error dataExchangeImpl(const void *SrcPtr, GenericDeviceTy &DstGenericDevice,
                         void *DstPtr, int64_t Size,
                         AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    int32_t SrcId = this->getDeviceId();
    int32_t DstId = DstGenericDevice.getDeviceId();
    return Plugin::error(ErrorCode::UNSUPPORTED,
                         "dataExchangeImpl unsupported on Thunderbird "
                         "(src DeviceId %d, dst DeviceId %d, %ld bytes); "
                         "libomptarget should have host-bounced via "
                         "isDataExchangable=false",
                         SrcId, DstId, (long)Size);
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
  ///
  /// NOT WIRED to launch behaviour, and not a statement about the hardware.
  /// The generic plugin feeds these into a host-side thread/block clamp
  /// (PluginInterface.cpp: getNumThreads, getNumBlocks), whose result
  /// ThunderbirdKernelTy::launchImpl does not use: there is no launch geometry
  /// to configure. A kernel image is entered once as an ordinary function call
  /// and the resident libomp creates the team from __kmpc_fork_call.
  ///
  /// The thread ceiling does not live here either. launchImpl forwards the raw
  /// KernelArgs.ThreadLimit[0] to the device, which applies it to
  /// thread-limit-var via __kmpc_set_thread_limit -- the home OpenMP 5.2
  /// Sec 10.1.1 gives it. It deliberately does not use the clamped NumThreads,
  /// because getNumThreads inflates the clause by a warp for the generic-mode
  /// primary thread.
  ///
  /// So GV_Max_WG_Size = 1 does not mean "one thread per team"; nothing reads
  /// it for that purpose, and the plugin has no way to learn the device's hart
  /// count -- there is no query for it in the tbird API. Giving these numbers
  /// invented "real" values would make them look authoritative without making
  /// them true.
  static constexpr GV ThunderbirdGridValues = {
      1, // GV_Slot_Size
      1, // GV_Warp_Size
      THUNDERBIRD_MAX_THREADS, // GV_Max_Teams
      1, // GV_Default_Num_Teams
      1, // GV_SimpleBufferSize
      1, // GV_Max_WG_Size
      1, // GV_Default_WG_Size
  };

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

// Argument conversion helpers are now in ArgumentConversion.h/.cpp.

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

  // Execution mode: Thunderbird is always GENERIC. It has no SPMD mode, because
  // SPMD/generic is a GPU state-machine distinction that lives in the device
  // runtime, and the resident runtime here is libomp, which has no such concept
  // -- it defines none of __kmpc_target_init/_deinit, __kmpc_is_spmd_exec_mode,
  // __kmpc_parallel_51, __kmpc_kernel_parallel or __kmpc_parallel_spmd. A kernel
  // image is entered once and __kmpc_fork_call creates the team, which is the
  // generic model by construction.
  //
  // This used to be derived from NumThreads[0] == 1, which held only because
  // ThunderbirdGridValues sets GV_Max_WG_Size to 1. That made the device calling
  // convention below (prependThreadId) depend on a constant that reads like a
  // hardware limit, so raising it would have silently changed the kernel ABI.
  // State the mode directly instead.
  const bool IsGeneric = true;
  DP("Execution mode: GENERIC (host-runtime threads=%u, blocks=%u)\n",
     NumThreads[0] * NumThreads[1] * NumThreads[2],
     NumBlocks[0] * NumBlocks[1] * NumBlocks[2]);

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
  ArgConversionContext Ctx{TBirdDevice->pool, KernelArgs, LaunchParams, KLEOffset};

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

  // thread-limit-var for this kernel, taken from the RAW clause value rather
  // than the clamped NumThreads[0] above. getNumThreads() adds a warp to the
  // clause for the generic-mode primary thread (PluginInterface.cpp,
  // `ThreadLimitClause[0] += getWarpSize()`), a GPU convention with no
  // counterpart here -- routing thread_limit(4) through it would apply 5. It is
  // private and cannot be overridden, so reading KernelArgs directly is the
  // only way to get the value the user wrote.
  //
  // Clang stores 0 here when no thread_limit clause applies, and otherwise the
  // clause value, or -- for a target region enclosing a single parallel -- that
  // region's num_threads. OpenMP 5.2 (ICVs, target construct) requires
  // thread-limit-var to be in [1, clause] when a clause is present, and permits
  // any value > 0 when none is, so forwarding the field unconditionally is
  // conforming in both cases. 0 means "leave the device runtime's default".
  uint32_t ThreadLimit = KernelArgs.ThreadLimit[0];

  DP("Calling tbird_launch_kernel_sync_ex: elf_offset=%zu, image_size=%zu, "
     "num_args=%u, thread_limit=%u\n",
     kernel_elf_offset, ImageSize, ArgCount, ThreadLimit);

  tbird_status_t Status = tbird_launch_kernel_sync_ex(
      TBirdDevice->ctx,
      image_buffer,
      kernel_elf_offset,
      ImageSize,
      getName(),   // entry point symbol
      Args,
      ArgCount,
      ThreadLimit
  );

  if (Status != TBIRD_SUCCESS) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "tbird_launch_kernel_sync_ex failed: %s",
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

    const auto &paths = getThunderbirdDevicePaths();
    DP("Thunderbird plugin discovered %zu application mailbox device(s)\n",
       paths.size());
    return static_cast<int32_t>(paths.size());
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

  /// Returns false unconditionally on this stack — libomptarget host-
  /// bounces every cross-device omp_target_memcpy via its malloc +
  /// retrieveData + submitData fallback (offload/libomptarget/OpenMP/
  /// API.cpp:258-280). That path is correct on every pair the runtime
  /// discovers, regardless of co-location.
  ///
  /// History: an earlier Phase 8 attempt returned true for co-located
  /// pairs and implemented dataExchangeImpl as a plain memcpy across
  /// the two host-side mmap VAs, on the assumption that mailboxes on
  /// the same physical Thunderbird shared BAR2/ivshmem backing.
  /// topology_correctness_e2e (offload-debug) falsified the assumption:
  /// even on co-located pairs, the host_ptr returned by
  /// omp_target_alloc on each device is in that device's OWN host-
  /// side slab pool, NOT a shared physical region. A naive memcpy
  /// across host VAs writes into the source process's memory but
  /// never reaches the destination device's worker view; the
  /// readback returns junk. (One direction of the test passed by
  /// coincidence — stale ref data from a prior iteration sat at the
  /// reused slab offset and matched the expected pattern.)
  ///
  /// A real co-located fast path would need to translate each
  /// host_ptr to its abstract handle (shared_key + offset in the
  /// per-device slab pool) and use a driver/firmware-mediated
  /// transfer between the two devices' slab pools. That work is out
  /// of scope here. The Topology module + co-location predicate are
  /// retained as the foundation for a future implementation.
  bool isDataExchangable(int32_t /*SrcDeviceId*/,
                         int32_t /*DstDeviceId*/) override {
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
