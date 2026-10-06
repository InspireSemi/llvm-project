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
#include <cstring>
#include <glob.h>
#include <mutex>
#include <string>
#include <variant>
#include <unordered_map>
#include <vector>

#include "Shared/Debug.h"
#include "llvm/Support/Format.h"
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
#include "llvm/BinaryFormat/ELF.h"

// Thunderbird plugin modules
#include "MemoryPool.h"
#include "LaunchSlots.h"
#include "DeviceGlobals.h"
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

// Device memory is MemoryPool (MemoryPool.h); what a launch sends is LaunchSlots.h.

// The number of OpenMP devices the plugin exposes is discovered at init
// time by enumerating /dev/tbird*-K char devices the host driver creates,
// one per application mailbox. See discoverThunderbirdDevicePaths() below.
// THUNDERBIRD_DEVICE_PATH=<path> forces a single-device legacy mode using
// the supplied path.

// The maximum number of physical cores in this plugin.
#define THUNDERBIRD_MAX_THREADS 6144
#define THUNDERBIRD_MAX_THREADS_XILINX 4
#define THUNDERBIRD_MAX_THREADS_QEMU 64

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

using namespace error;
using namespace llvm::offload::debug;

/// Class implementing kernel functionalities for Thunderbird.
struct ThunderbirdKernelTy : public GenericKernelTy {
  /// Construct the kernel with a name and an execution mode.
  ThunderbirdKernelTy(const char *Name) : GenericKernelTy(Name), Func(nullptr) {}

  /// Initialize the kernel.
  Error initImpl(GenericDeviceTy &Device, DeviceImageTy &Image) override;

  Error launchImpl(GenericDeviceTy &GenericDevice, uint32_t NumThreads[3],
                   uint32_t NumBlocks[3], uint32_t DynBlockMemSize,
                   KernelArgsTy &KernelArgs, KernelLaunchParamsTy LaunchParams,
                   AsyncInfoWrapperTy &AsyncInfoWrapper) const override;

  /// Occupancy is not a concept here: a kernel is one call on the device.
  Expected<uint64_t> maxGroupSize(GenericDeviceTy &GenericDevice,
                                  uint64_t DynamicMemSize) const override {
    return Plugin::error(ErrorCode::UNSUPPORTED,
                         "occupancy calculations are not implemented for "
                         "the Thunderbird device");
  }

private:
  /// The kernel function to execute.
  void (*Func)(void);

  /// Image buffer handle containing this kernel's ELF image (Phase 4)
  tbird_buffer_t image_buffer = nullptr;
  /// Offset of ELF image within pool slab
  size_t kernel_elf_offset = 0;
  /// Size of the ELF image: with the buffer and offset, it names the image's
  /// loaded copy on the device.
  size_t kernel_image_size = 0;
};

/// Class implementing the Thunderbird device images properties.
///
/// The image's bytes sit in pool memory, and the device loads them once
/// (tbird_load_image); every kernel of the image runs in that loaded copy,
/// which the device names by (image_buffer, elf_offset, image_size).
struct ThunderbirdDeviceImageTy : public DeviceImageTy {
  /// Create the Thunderbird image with the id and the image contents.
  ThunderbirdDeviceImageTy(int32_t ImageId, GenericDeviceTy &Device,
                           std::unique_ptr<MemoryBuffer> &&Image)
      : DeviceImageTy(ImageId, Device, std::move(Image)) {}

  /// Buffer handle for loaded image (Phase 3 migration)
  tbird_buffer_t image_buffer = nullptr;
  /// Offset of ELF image within the pool slab (for elf_offset in kernel launch)
  size_t elf_offset = 0;
  /// Size of the ELF image in bytes.
  size_t image_size = 0;
  /// Where the device loaded the image: its symbol S is at LoadBase + S's
  /// st_value. 0 until the device has loaded it.
  uint64_t LoadBase = 0;
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
    ODBG(OLDT_Init) << llvm::format("=== Phase 2/3: initImpl START (DeviceId=%d) ===", DeviceId);

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
    ODBG(OLDT_Init) << llvm::format("DeviceId=%d -> device_path=%s", DeviceId, device_path.c_str());

    // Each plugin-level OpenMP device opens a single application mailbox
    // (1:1 mapping). num_mailboxes=1 is intentional: this is the count of
    // mailboxes per tbird_init() call, not the system-wide total.
    ctx = tbird_init(device_path.c_str(), 1);
    if (!ctx) {
      ODBG(OLDT_Init) << "ERROR: tbird_init returned NULL";
      return Plugin::error(ErrorCode::UNKNOWN,
                          "Failed to initialize Thunderbird context: device=%s",
                          device_path.c_str());
    }
    
    ODBG(OLDT_Init) << llvm::format("SUCCESS: Thunderbird context initialized: ctx=%p", (void*)ctx);

    pool.init(ctx);
    ODBG(OLDT_Init) << "Memory pool initialized";

    ODBG(OLDT_Init) << "=== Phase 2/3: initImpl COMPLETE ===";
    fprintf(stderr, "[THUNDERBIRD RTL] Device initImpl COMPLETE\n");
    fflush(stderr);
    return Plugin::success();
  }

  /// Unload the binary image
  ///
  /// Unload the binary image and free associated resources
  Error unloadBinaryImpl(DeviceImageTy *Image) override {
    ODBG(OLDT_Module) << "=== Phase 3: unloadBinaryImpl START ===";
    
    auto TBirdImage = reinterpret_cast<ThunderbirdDeviceImageTy *>(Image);
    
    if (!TBirdImage) {
      ODBG(OLDT_Module) << "WARNING: Image is NULL";
      return Plugin::success();
    }
    
    ODBG(OLDT_Module) << llvm::format("Unloading image: Image=%p, image_buffer=%p", (void*)TBirdImage, (void*)TBirdImage->image_buffer);

    // Pool manages image buffer lifetime — no per-image free.
    // The slab is released in pool.destroy() at device shutdown.
    if (TBirdImage->image_buffer) {
      ODBG(OLDT_Module) << llvm::format("Image buffer %p managed by pool (offset %zu) — no free", (void*)TBirdImage->image_buffer, TBirdImage->elf_offset);
      TBirdImage->image_buffer = nullptr;
    } else {
      ODBG(OLDT_Module) << "No image buffer to free";
    }

    ODBG(OLDT_Module) << "Freeing Image object";
    TBirdImage->~ThunderbirdDeviceImageTy();
    Plugin.free(TBirdImage);
    
    ODBG(OLDT_Module) << "=== Phase 3: unloadBinaryImpl COMPLETE ===";

    return Plugin::success();
  }

  /// Deinitialize the device — release all pool slabs.
  Error deinitImpl() override {
    std::lock_guard<std::mutex> Lock(MailboxMutex);
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
  Expected<DeviceImageTy *>
  loadBinaryImpl(std::unique_ptr<MemoryBuffer> &&TgtImage,
                 int32_t ImageId) override {
    ODBG(OLDT_Module) << llvm::format("=== Phase 3: loadBinaryImpl START (ImageId=%d) ===", ImageId);
    fprintf(stderr, "[THUNDERBIRD RTL] loadBinaryImpl called (ImageId=%d)\n", ImageId);
    fflush(stderr);
    
    // Validate context
    if (!ctx) {
      ODBG(OLDT_Module) << "ERROR: ctx is NULL, cannot load image";
      return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in loadBinaryImpl");
    }
    
    // Validate TgtImage
    if (!TgtImage) {
      ODBG(OLDT_Module) << "ERROR: TgtImage is NULL";
      return Plugin::error(ErrorCode::INVALID_BINARY, "TgtImage is NULL");
    }

    size_t ImageSize = TgtImage->getBufferSize();
    ODBG(OLDT_Module) << llvm::format("Image info: ImageStart=%p, Size=%zu bytes", (const void *)TgtImage->getBufferStart(), ImageSize);

    if (ImageSize == 0 || ImageSize > (1024 * 1024 * 100)) { // Sanity: 0-100MB
      ODBG(OLDT_Module) << llvm::format("ERROR: Suspicious image size: %zu bytes", ImageSize);
      return Plugin::error(ErrorCode::INVALID_BINARY, "Invalid image size: %zu", ImageSize);
    }

    // Allocate and construct image object
    ODBG(OLDT_Module) << "Allocating ThunderbirdDeviceImageTy object";
    ThunderbirdDeviceImageTy *Image = Plugin.allocate<ThunderbirdDeviceImageTy>();
    if (!Image) {
      ODBG(OLDT_Module) << "ERROR: Failed to allocate ThunderbirdDeviceImageTy";
      return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                          "failed to allocate memory for device image");
    }

    ODBG(OLDT_Module) << llvm::format("Constructing ThunderbirdDeviceImageTy (Image=%p)", (void*)Image);
    new (Image) ThunderbirdDeviceImageTy(ImageId, *this, std::move(TgtImage));
    const char *ImageStart = static_cast<const char *>(Image->getStart());

    // Every failure below releases the image object the same way.
    auto Discard = [&](Error Err) -> Error {
      Image->~ThunderbirdDeviceImageTy();
      Plugin.free(Image);
      return Err;
    };

    // The device loads the image as a shared object and reports its load
    // base; symbol addresses are that base plus st_value, which holds for an
    // ELF shared object only. Checked before anything is allocated.
    auto ElfOrErr = llvm::object::ELF64LEFile::create(
        llvm::StringRef(ImageStart, ImageSize));
    if (!ElfOrErr)
      return Discard(Plugin::error(ErrorCode::INVALID_BINARY,
                                   "failed to parse ELF image: %s",
                                   toString(ElfOrErr.takeError()).c_str()));
    if (ElfOrErr->getHeader().e_type != llvm::ELF::ET_DYN)
      return Discard(Plugin::error(ErrorCode::INVALID_BINARY,
                                   "image is not an ELF shared object "
                                   "(e_type %u)",
                                   (unsigned)ElfOrErr->getHeader().e_type));

    // Allocating may add a slab, which waits for the device's acknowledgement,
    // and the load is a device request: both use the mailbox.
    std::lock_guard<std::mutex> Lock(MailboxMutex);

    ODBG(OLDT_Module) << llvm::format("POOL: allocating image buffer: %zu bytes", ImageSize);
    void *img_ptr = pool.allocate(ImageSize);
    if (!img_ptr) {
      ODBG(OLDT_Module) << llvm::format("ERROR: pool.allocate failed for image (%zu bytes)", ImageSize);
      return Discard(Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                                   "pool.allocate failed for image (%zu bytes)",
                                   ImageSize));
    }

    // Resolve slab buffer and offset for this sub-allocation
    auto [image_buf, elf_off] = pool.lookup(img_ptr);
    ODBG(OLDT_Module) << llvm::format("POOL: image at slab buffer=%p, elf_offset=%zu", (void*)image_buf, elf_off);

    // Upload image to shared buffer at the pool-assigned offset
    tbird_status_t status = tbird_buffer_write(ctx, image_buf, elf_off,
                                               ImageStart, ImageSize);
    if (status != TBIRD_SUCCESS) {
      ODBG(OLDT_Module) << llvm::format("ERROR: tbird_buffer_write failed for image: %s", tbird_last_error(ctx));
      Error Err = Plugin::error(ErrorCode::UNKNOWN,
                                "tbird_buffer_write failed for image: %s",
                                tbird_last_error(ctx));
      pool.deallocate(img_ptr);
      return Discard(std::move(Err));
    }

    uint64_t LoadBase = 0;
    status = tbird_load_image(ctx, image_buf, elf_off, ImageSize, &LoadBase);
    if (status != TBIRD_SUCCESS) {
      ODBG(OLDT_Module) << llvm::format("ERROR: tbird_load_image failed: %s", tbird_last_error(ctx));
      Error Err = Plugin::error(ErrorCode::INVALID_BINARY,
                                "loading the image on the device failed: %s",
                                tbird_last_error(ctx));
      pool.deallocate(img_ptr);
      return Discard(std::move(Err));
    }

    Image->image_buffer = image_buf;
    Image->elf_offset = elf_off;
    Image->image_size = ImageSize;
    Image->LoadBase = LoadBase;
    ODBG(OLDT_Module) << llvm::format("SUCCESS: image of %zu bytes at buffer=%p, elf_offset=%zu, loaded on the device at base 0x%lx", ImageSize, (void*)image_buf, elf_off, (unsigned long)LoadBase);
    ODBG(OLDT_Module) << "=== Phase 3: loadBinaryImpl COMPLETE ===";
    
    return Image;
  }

  /// Allocate memory: device, default and shared allocations come from the
  /// pool (device addresses); host allocations from std::malloc.
  Expected<void *> allocate(size_t Size, void *, TargetAllocTy Kind,
                            size_t Alignment) override {
    ODBG(OLDT_Alloc) << llvm::format("=== Phase 2: allocate(Size=%zu, Kind=%d, Alignment=%zu) ===", Size, (int)Kind, Alignment);

    // Both the pool and std::malloc return MemoryPool::ALIGNMENT-aligned memory
    // and nothing stronger; a larger alignment is refused rather than ignored.
    if (Alignment > MemoryPool::ALIGNMENT)
      return Plugin::error(ErrorCode::UNSUPPORTED,
                           "allocation alignment %zu exceeds the %zu bytes "
                           "Thunderbird allocations are aligned to",
                           Alignment, (size_t)MemoryPool::ALIGNMENT);

    if (Size == 0) {
      ODBG(OLDT_Alloc) << "WARNING: Requested allocation of size 0";
      return nullptr;
    }

    void *MemAlloc = nullptr;
    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
      {
        if (!ctx) {
          ODBG(OLDT_Alloc) << "ERROR: ctx is NULL, cannot allocate buffer";
          return nullptr;
        }

        std::lock_guard<std::mutex> Lock(MailboxMutex);
        void *ptr = pool.allocate(Size);
        if (!ptr) {
          ODBG(OLDT_Alloc) << llvm::format("ERROR: pool.allocate(%zu) failed", Size);
          return nullptr;
        }

        ODBG(OLDT_Alloc) << llvm::format("SUCCESS: pool.allocate(%zu) → %p (now %zu tracked)", Size, ptr, pool.liveAllocations());
        return ptr;
      }
    case TARGET_ALLOC_HOST:
      ODBG(OLDT_Alloc) << "HOST allocation (using malloc)";
      MemAlloc = std::malloc(Size);
      break;
    }
    return MemAlloc;
  }

  /// Free device memory — removes from pool tracking (grow-only, no DMA release).
  Error free(void *TgtPtr, TargetAllocTy Kind) override {
    ODBG(OLDT_Alloc) << llvm::format("=== free(TgtPtr=%p, Kind=%d) ===", TgtPtr, (int)Kind);

    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
      pool.deallocate(TgtPtr);
      ODBG(OLDT_Alloc) << llvm::format("SUCCESS: pool.deallocate(%p) (now %zu tracked)", TgtPtr, pool.liveAllocations());
      return Plugin::success();
    case TARGET_ALLOC_HOST:
      std::free(TgtPtr);
      return Plugin::success();
    }
    return Plugin::error(ErrorCode::INVALID_ARGUMENT,
                         "unknown allocation kind %d", (int)Kind);
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
  ODBG(OLDT_DataTransfer) << llvm::format("=== Phase 2: dataSubmit(TgtPtr=%p, HstPtr=%p, Size=%ld) ===", TgtPtr, HstPtr, Size);
  
  // Validate inputs
  if (!ctx) {
    ODBG(OLDT_DataTransfer) << "ERROR: ctx is NULL";
    return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in dataSubmitImpl");
  }
  
  if (!HstPtr) {
    ODBG(OLDT_DataTransfer) << "ERROR: HstPtr is NULL";
    return Plugin::error(ErrorCode::UNKNOWN, "HstPtr is NULL");
  }
  
  if (Size <= 0) {
    ODBG(OLDT_DataTransfer) << llvm::format("WARNING: Size=%ld is non-positive", Size);
  }
  
  // Look up buffer handle (supports interior pointers)
  ODBG(OLDT_DataTransfer) << llvm::format("Looking up TgtPtr in pool (%zu tracked)", pool.liveAllocations());
  auto [buffer, offset] = findContainingBuffer(TgtPtr);
  if (!buffer)
    return copyImageMemory(/*ToDevice=*/true, TgtPtr, const_cast<void *>(HstPtr),
                           Size, "dataSubmit");
  std::lock_guard<std::mutex> Lock(MailboxMutex);

  ODBG(OLDT_DataTransfer) << llvm::format("Found buffer=%p for TgtPtr=%p (offset=%zu)", (void*)buffer, TgtPtr, offset);

  // Transfer via driver ioctl
  ODBG(OLDT_DataTransfer) << llvm::format("Calling tbird_buffer_write(ctx=%p, buffer=%p, offset=%zu, src=%p, size=%ld)", (void*)ctx, (void*)buffer, offset, HstPtr, Size);

  tbird_status_t status = tbird_buffer_write(ctx, buffer, offset, HstPtr, Size);
  if (status != TBIRD_SUCCESS) {
    ODBG(OLDT_DataTransfer) << llvm::format("ERROR: tbird_buffer_write failed: %s", tbird_last_error(ctx));
    return Plugin::error(ErrorCode::UNKNOWN,
                        "tbird_buffer_write failed: %s", tbird_last_error(ctx));
  }
  
  ODBG(OLDT_DataTransfer) << llvm::format("SUCCESS: dataSubmit: %ld bytes to buffer %p", Size, (void*)buffer);
  
  return Plugin::success();
}

  /// Retrieve data from the device (device to host transfer).
  Error dataRetrieveImpl(void *HstPtr, const void *TgtPtr, int64_t Size,
                         AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    ODBG(OLDT_DataTransfer) << llvm::format("=== Phase 2: dataRetrieve(HstPtr=%p, TgtPtr=%p, Size=%ld) ===", HstPtr, TgtPtr, Size);
    
    // Validate inputs
    if (!ctx) {
      ODBG(OLDT_DataTransfer) << "ERROR: ctx is NULL";
      return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in dataRetrieveImpl");
    }
    
    if (!HstPtr) {
      ODBG(OLDT_DataTransfer) << "ERROR: HstPtr is NULL";
      return Plugin::error(ErrorCode::UNKNOWN, "HstPtr is NULL");
    }
    
    if (Size <= 0) {
      ODBG(OLDT_DataTransfer) << llvm::format("WARNING: Size=%ld is non-positive", Size);
    }
    
    // Look up buffer handle (supports interior pointers)
    ODBG(OLDT_DataTransfer) << llvm::format("Looking up TgtPtr in pool (%zu tracked)", pool.liveAllocations());
    auto [buffer, offset] = findContainingBuffer(const_cast<void*>(TgtPtr));
    if (!buffer)
      return copyImageMemory(/*ToDevice=*/false, TgtPtr, HstPtr, Size,
                             "dataRetrieve");
    std::lock_guard<std::mutex> Lock(MailboxMutex);

    ODBG(OLDT_DataTransfer) << llvm::format("Found buffer=%p for TgtPtr=%p (offset=%zu)", (void*)buffer, TgtPtr, offset);

    // Transfer via driver ioctl
    ODBG(OLDT_DataTransfer) << llvm::format("Calling tbird_buffer_read(ctx=%p, buffer=%p, offset=%zu, dst=%p, size=%ld)", (void*)ctx, (void*)buffer, offset, HstPtr, Size);

    tbird_status_t status = tbird_buffer_read(ctx, buffer, offset, HstPtr, Size);
    if (status != TBIRD_SUCCESS) {
      ODBG(OLDT_DataTransfer) << llvm::format("ERROR: tbird_buffer_read failed: %s", tbird_last_error(ctx));
      return Plugin::error(ErrorCode::UNKNOWN,
                          "tbird_buffer_read failed: %s", tbird_last_error(ctx));
    }
    
    ODBG(OLDT_DataTransfer) << llvm::format("SUCCESS: dataRetrieve: %ld bytes from buffer %p", Size, (void*)buffer);
    
    return Plugin::success();
  }

  /// Every data operation is a synchronous ioctl, so nothing can be reordered.
  Error dataFence(__tgt_async_info *AsyncInfo) override {
    return Plugin::success();
  }

  /// Fill device memory with a repeated pattern. The pattern is built on the
  /// host and written through the buffer it belongs to, or through the device
  /// for memory of a loaded image: a device pointer is the device's address,
  /// which the host cannot write through.
  Error dataFillImpl(void *TgtPtr, const void *PatternPtr, int64_t PatternSize,
                     int64_t Size,
                     AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    if (Size <= 0)
      return Plugin::success();
    if (PatternSize <= 0 || !PatternPtr)
      return Plugin::error(ErrorCode::INVALID_ARGUMENT,
                           "dataFill: invalid pattern (size %ld)",
                           (long)PatternSize);
    auto [buffer, offset] = findContainingBuffer(TgtPtr);
    std::vector<char> Fill(Size);
    for (int64_t I = 0; I < Size; I += PatternSize)
      std::memcpy(Fill.data() + I, PatternPtr,
                  std::min<int64_t>(PatternSize, Size - I));
    if (!buffer)
      return copyImageMemory(/*ToDevice=*/true, TgtPtr, Fill.data(), Size,
                             "dataFill");
    std::lock_guard<std::mutex> Lock(MailboxMutex);
    if (tbird_buffer_write(ctx, buffer, offset, Fill.data(), Size) !=
        TBIRD_SUCCESS)
      return Plugin::error(ErrorCode::UNKNOWN, "tbird_buffer_write failed: %s",
                           tbird_last_error(ctx));
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
  Error synchronizeImpl(__tgt_async_info &AsyncInfo,
                        bool ReleaseQueue) override {
    return Plugin::success();
  }

  /// Operations are synchronous, so a host callback runs immediately.
  Error enqueueHostCallImpl(void (*Callback)(void *), void *UserData,
                            AsyncInfoWrapperTy &AsyncInfo) override {
    Callback(UserData);
    return Plugin::success();
  }

  /// Nothing is ever queued: every operation completes before it returns.
  Expected<bool> hasPendingWorkImpl(AsyncInfoWrapperTy &AsyncInfo) override {
    return false;
  }

  /// All functions are already synchronous. No need to do anything on this
  /// query function.
  Error queryAsyncImpl(__tgt_async_info &AsyncInfo, bool ReleaseQueue,
                       bool *IsQueueWorkCompleted) override {
    if (IsQueueWorkCompleted)
      *IsQueueWorkCompleted = true;
    return Plugin::success();
  }

  /// This plugin does not support interoperability
  Error initAsyncInfoImpl(AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    return Plugin::error(ErrorCode::UNSUPPORTED,
                         "initAsyncInfoImpl not supported");
  }

  /// This plugin does not support the event API. Do nothing without failing.
  Error createEventImpl(void **EventPtrStorage, bool EnableProfiling) override {
    *EventPtrStorage = nullptr;
    return Plugin::success();
  }
  Error destroyEventImpl(void *EventPtr, bool EnableProfiling) override {
    return Plugin::success();
  }
  Error recordEventImpl(void *EventPtr, AsyncInfoWrapperTy &AsyncInfoWrapper,
                        bool EnableProfiling) override {
    return Plugin::success();
  }
  Error waitEventImpl(void *EventPtr,
                      AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    return Plugin::success();
  }
  Error syncEventImpl(void *EventPtr) override { return Plugin::success(); }
  Expected<bool> isEventCompleteImpl(void *EventPtr,
                                     AsyncInfoWrapperTy &AsyncInfo) override {
    return true;
  }
  /// Events record no time, so there is no elapsed time to report.
  Expected<float> getEventElapsedTimeImpl(void *StartEventPtr,
                                          void *EndEventPtr) override {
    return Plugin::error(ErrorCode::UNSUPPORTED,
                         "event timing is not supported on Thunderbird");
  }

  /// Print information about the device.
  Expected<InfoTreeNode> obtainInfoImpl() override {
    InfoTreeNode Info;
    Info.add("Device Type", "Thunderbird RISC-V-64bit");
    return Info;
  }

  /// Getters and setters for stack size not relevant.
  Error getDeviceStackSize(uint64_t &Value) override {
    Value = 0;
    return Plugin::success();
  }
  Error setDeviceStackSize(uint64_t Value) override {
    return Plugin::success();
  }

  /// New offload-platform API context (Phase 5: old channels removed)
  tbird_context_t ctx = nullptr;

  /// Unified memory pool — all allocations (ELF + data) go through this.
  MemoryPool pool;

  /// Find the buffer containing ptr (supports interior pointers).
  /// Delegates to the memory pool for all allocations.
  std::pair<tbird_buffer_t, size_t> findContainingBuffer(void *ptr) {
    return pool.lookup(ptr);
  }

  /// Serialises the device requests that share the one mailbox page: a slab
  /// allocation (which waits for the device's acknowledgement), an image load,
  /// a device copy and a kernel launch. A request and its reply occupy the
  /// same page, so two in flight would overwrite each other. A launch holds it
  /// for the kernel's duration, as the device serves one request at a time.
  /// It also guards the device's context: the library keeps the context's
  /// error text and buffer table without a lock, so every call on ctx after
  /// initImpl -- buffer reads and writes included -- is made holding it.
  std::mutex MailboxMutex;

  /// Copy Size bytes between host memory and device memory outside the pool,
  /// which is a loaded image's memory -- its global variables. The device
  /// copies through a staging range of pool memory, one request per chunk,
  /// and refuses an address that is not inside a loaded image.
  Error copyImageMemory(bool ToDevice, const void *TgtPtr, void *HstPtr,
                        int64_t Size, const char *Op) {
    if (Size <= 0)
      return Plugin::success();
    std::lock_guard<std::mutex> Lock(MailboxMutex);
    if (!StagingPtr) {
      StagingPtr = pool.allocate(STAGING_SIZE);
      if (!StagingPtr)
        return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                             "%s: no staging memory for a device copy: %s", Op,
                             tbird_last_error(ctx));
      std::tie(StagingBuf, StagingOff) = pool.lookup(StagingPtr);
    }
    char *Host = static_cast<char *>(HstPtr);
    std::string Why = ::thunderbird::copyInChunks(
        reinterpret_cast<uint64_t>(TgtPtr), static_cast<size_t>(Size),
        STAGING_SIZE, [&](uint64_t Addr, size_t Off, size_t Len) -> std::string {
          if (ToDevice && tbird_buffer_write(ctx, StagingBuf, StagingOff,
                                             Host + Off, Len) != TBIRD_SUCCESS)
            return tbird_last_error(ctx);
          if (tbird_device_copy(ctx,
                                ToDevice ? TBIRD_COPY_TO_DEVICE
                                         : TBIRD_COPY_FROM_DEVICE,
                                Addr, StagingBuf, StagingOff,
                                Len) != TBIRD_SUCCESS)
            return tbird_last_error(ctx);
          if (!ToDevice && tbird_buffer_read(ctx, StagingBuf, StagingOff,
                                             Host + Off, Len) != TBIRD_SUCCESS)
            return tbird_last_error(ctx);
          return "";
        });
    if (!Why.empty())
      return Plugin::error(ErrorCode::UNKNOWN,
                           "%s: %p is not pool memory, and the device did not "
                           "copy it as image memory: %s",
                           Op, TgtPtr, Why.c_str());
    return Plugin::success();
  }

private:
  /// The staging range device copies go through, allocated at the first one.
  static constexpr size_t STAGING_SIZE = 64 * 1024;
  void *StagingPtr = nullptr;
  tbird_buffer_t StagingBuf = nullptr;
  size_t StagingOff = 0;

  /// Grid values for Thunderbird plugins.
  ///
  /// NOT WIRED to launch behaviour, and not a statement about the hardware.
  /// The generic plugin feeds these into a host-side thread/block clamp
  /// (PluginInterface.cpp: getEffectiveNumThreads, getEffectiveNumBlocks), whose result
  /// ThunderbirdKernelTy::launchImpl does not use: there is no launch geometry
  /// to configure. A kernel image is entered once as an ordinary function call
  /// and the resident libomp creates the team from __kmpc_fork_call.
  ///
  /// The thread ceiling does not live here either. launchImpl forwards the raw
  /// KernelArgs.UserThreadLimit[0] to the device, which applies it to
  /// thread-limit-var via __kmpc_set_thread_limit -- the home OpenMP 5.2
  /// Sec 10.1.1 gives it. It deliberately does not use the clamped NumThreads,
  /// because getEffectiveNumThreads inflates the clause by a warp for the
  /// generic-mode primary thread.
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
  ODBG(OLDT_Init) << llvm::format("=== Phase 4: ThunderbirdKernelTy::initImpl START (kernel=%s) ===", getName());
  fprintf(stderr, "[THUNDERBIRD RTL] Kernel initImpl called for: %s\n", getName());
  fflush(stderr);
  
  // Get image buffer from DeviceImageTy
  auto &TBirdImage = static_cast<ThunderbirdDeviceImageTy &>(Image);
  image_buffer = TBirdImage.image_buffer;
  kernel_elf_offset = TBirdImage.elf_offset;
  kernel_image_size = TBirdImage.image_size;

  if (!image_buffer) {
    ODBG(OLDT_Init) << llvm::format("ERROR: Image buffer is NULL for kernel %s", getName());
    return Plugin::error(ErrorCode::INVALID_BINARY,
                        "Image buffer not loaded for kernel %s", getName());
  }

  ODBG(OLDT_Init) << llvm::format("Stored image_buffer=%p, elf_offset=%zu for kernel %s", (void*)image_buffer, kernel_elf_offset, getName());

  // Functions have zero size.
  GlobalTy Global(getName(), 0);

  // Get the metadata (address) of the kernel function.
  GenericGlobalHandlerTy &GHandler = Device.Plugin.getGlobalHandler();
  if (auto Err = GHandler.getGlobalMetadataFromDevice(Device, Image, Global))
    return Err;

  // Check that the function pointer is valid.
  if (!Global.getPtr()) {
    ODBG(OLDT_Init) << llvm::format("ERROR: Global.getPtr() returned NULL for kernel %s", getName());
    return Plugin::error(ErrorCode::INVALID_BINARY,
                         "invalid function for kernel %s", getName());
  }

  // Save the function's device address (for diagnostics; the launch names the
  // kernel by symbol).
  Func = (void (*)())Global.getPtr();
  ODBG(OLDT_Init) << llvm::format("Kernel %s: Func=%p (device address)", getName(), (void*)Func);

  KernelEnvironment.Configuration.ExecMode = OMP_TGT_EXEC_MODE_GENERIC;
  KernelEnvironment.Configuration.MayUseNestedParallelism = 2; // Unknown
  KernelEnvironment.Configuration.UseGenericStateMachine = 2;  // Unknown

  ODBG(OLDT_Init) << "=== Phase 4: ThunderbirdKernelTy::initImpl COMPLETE ===";
  fprintf(stderr, "[THUNDERBIRD RTL] Kernel initImpl COMPLETE for: %s\n", getName());
  fflush(stderr);
  return Plugin::success();
}

//===----------------------------------------------------------------------===//
// launchImpl - Main kernel launch method
//===----------------------------------------------------------------------===//

Error ThunderbirdKernelTy::launchImpl(GenericDeviceTy &GenericDevice,
                                      uint32_t NumThreads[3],
                                      uint32_t NumBlocks[3],
                                      uint32_t DynBlockMemSize,
                                      KernelArgsTy &KernelArgs,
                                      KernelLaunchParamsTy LaunchParams,
                                      AsyncInfoWrapperTy &AsyncInfoWrapper) const {
  ODBG(OLDT_Kernel) << llvm::format("launchImpl: kernel=%s, NumArgs=%u", getName(), LaunchParams.NumArgs);

  std::string Unsupported = ::thunderbird::launchUnsupportedReason(
      KernelArgs.Flags.IsCUDA, KernelArgs.Flags.IsPtrArgs,
      KernelArgs.DynCGroupMem, DynBlockMemSize);
  if (!Unsupported.empty())
    return Plugin::error(ErrorCode::UNSUPPORTED, "kernel %s: %s", getName(),
                         Unsupported.c_str());

  auto *TBirdDevice = static_cast<ThunderbirdDeviceTy *>(&GenericDevice);

  // Execution mode: Thunderbird is always GENERIC. It has no SPMD mode, because
  // SPMD/generic is a GPU state-machine distinction that lives in the device
  // runtime, and the resident runtime here is libomp, which has no such concept
  // -- it defines none of __kmpc_target_init/_deinit, __kmpc_is_spmd_exec_mode,
  // __kmpc_parallel_51, __kmpc_kernel_parallel or __kmpc_parallel_spmd. A kernel
  // image is entered once and __kmpc_fork_call creates the team, which is the
  // generic model by construction.
  ODBG(OLDT_Kernel) << llvm::format("Execution mode: GENERIC (host-runtime threads=%u, blocks=%u)", NumThreads[0] * NumThreads[1] * NumThreads[2], NumBlocks[0] * NumBlocks[1] * NumBlocks[2]);

  // Every argument is one pointer-sized value, sent unchanged: pool memory is
  // device addresses, so a pointer argument is already one the kernel can use,
  // and so is a pointer stored in memory it reaches. dyn_ptr is the last slot.
  uint64_t Slots[TBIRD_MAX_ARGS];
  std::string BadArgs = ::thunderbird::buildLaunchSlots(
      LaunchParams.NumArgs, LaunchParams.Args, Slots, TBIRD_MAX_ARGS);
  if (!BadArgs.empty())
    return Plugin::error(ErrorCode::INVALID_ARGUMENT, "kernel %s: %s", getName(),
                         BadArgs.c_str());
  for (uint32_t I = 0; I < LaunchParams.NumArgs; ++I)
    ODBG(OLDT_Kernel) << llvm::format("  slot[%u] = 0x%lx", I, (unsigned long)Slots[I]);

  // The image's buffer, offset and size name its loaded copy on the device,
  // the one tbird_load_image made at loadBinaryImpl.
  size_t ImageSize = kernel_image_size;

  // thread-limit-var for this kernel, taken from the RAW clause value rather
  // than the clamped NumThreads[0] above. getEffectiveNumThreads() adds a warp
  // to the clause for the generic-mode primary thread (PluginInterface.cpp,
  // `UserThreadLimit += GenericDevice.getWarpSize()`), a GPU convention with no
  // counterpart here -- routing thread_limit(4) through it would apply 5. It is
  // not overridable, so reading KernelArgs directly is the only way to get the
  // value the user wrote.
  //
  // Clang stores 0 here when no thread_limit clause applies, and otherwise the
  // clause value, or -- for a target region enclosing a single parallel -- that
  // region's num_threads. OpenMP 5.2 (ICVs, target construct) requires
  // thread-limit-var to be in [1, clause] when a clause is present, and permits
  // any value > 0 when none is, so forwarding the field unconditionally is
  // conforming in both cases. 0 means "leave the device runtime's default".
  uint32_t ThreadLimit = KernelArgs.UserThreadLimit[0];

  ODBG(OLDT_Kernel) << llvm::format("tbird_launch_kernel_slots: elf_offset=%zu, image_size=%zu, slots=%u, thread_limit=%u", kernel_elf_offset, ImageSize, LaunchParams.NumArgs, ThreadLimit);
  std::lock_guard<std::mutex> Lock(TBirdDevice->MailboxMutex);
  tbird_status_t Status = tbird_launch_kernel_slots(
      TBirdDevice->ctx, image_buffer, kernel_elf_offset, ImageSize, getName(),
      Slots, LaunchParams.NumArgs, ThreadLimit);
  if (Status != TBIRD_SUCCESS)
    return Plugin::error(ErrorCode::UNKNOWN, "kernel %s: %s", getName(),
                         tbird_last_error(TBirdDevice->ctx));

  ODBG(OLDT_Kernel) << llvm::format("SUCCESS: Kernel %s completed", getName());
  return Plugin::success();
}

class ThunderbirdGlobalHandlerTy final : public GenericGlobalHandlerTy {
public:
  Error getGlobalMetadataFromDevice(GenericDeviceTy &GenericDevice,
                                    DeviceImageTy &Image,
                                    GlobalTy &DeviceGlobal) override {

    auto &ThunderbirdImage = static_cast<ThunderbirdDeviceImageTy &>(Image);

    // The symbol as the image's dynamic symbol table describes it; the device
    // loaded the image at LoadBase, so a symbol it defines is at LoadBase +
    // st_value in the copy every kernel of the image runs in.
    auto ELFObj = getELFObjectFile(Image);
    if (!ELFObj)
      return ELFObj.takeError();
    auto SymOrErr = utils::elf::getSymbol(**ELFObj, DeviceGlobal.getName());
    if (!SymOrErr)
      return SymOrErr.takeError();
    if (!SymOrErr->has_value())
      return Plugin::error(ErrorCode::NOT_FOUND,
                           "failed to find global '%s' in the image",
                           DeviceGlobal.getName().data());
    auto ValueOrErr = (*SymOrErr)->getValue();
    if (!ValueOrErr)
      return ValueOrErr.takeError();
    auto FlagsOrErr = (*SymOrErr)->getFlags();
    if (!FlagsOrErr)
      return FlagsOrErr.takeError();

    ::thunderbird::ImageSymbol Sym{
        !(*FlagsOrErr & llvm::object::SymbolRef::SF_Undefined), *ValueOrErr,
        (*SymOrErr)->getSize()};
    uint64_t Addr = 0;
    std::string Why = ::thunderbird::deviceSymbolAddress(
        ThunderbirdImage.LoadBase, DeviceGlobal.getName().data(), Sym,
        DeviceGlobal.getSize(), &Addr);
    if (!Why.empty())
      return Plugin::error(ErrorCode::NOT_FOUND, "%s", Why.c_str());

    DeviceGlobal.setPtr(reinterpret_cast<void *>(Addr));
    DeviceGlobal.setSize(Sym.Size);
    return Plugin::success();
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
    const auto &paths = getThunderbirdDevicePaths();
    ODBG(OLDT_Init) << llvm::format("Thunderbird plugin discovered %zu application mailbox device(s)", paths.size());
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
