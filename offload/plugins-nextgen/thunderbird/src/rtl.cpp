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
#include "LaunchSlots.h"
#include "DeviceGlobals.h"

// offload-platform's host library
#include "tbird_offload_api.h"

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) ||           \
    !defined(__ORDER_BIG_ENDIAN__)
#error "Missing preprocessor definitions for endianness detection."
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define LITTLEENDIAN_CPU
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BIGENDIAN_CPU
#endif

// Device memory is the library's (tbird_alloc), named by device address; what a
// launch sends is LaunchSlots.h.

// The OpenMP devices the plugin exposes are named at init time; see
// getThunderbirdDevicePaths() below.

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

/// The names of the devices, in OpenMP device order; each is passed to
/// tbird_open. The first of these that applies:
///
///   - THUNDERBIRD_DEVICES=<name>,<name>,...: these devices, in this order.
///     Empty names (",,", a trailing ",") are skipped.
///   - THUNDERBIRD_DEVICE_PATH=<path>: that one device.
///   - Otherwise every application mailbox of the host driver
///     (tbird_offload.ko), which exposes one /dev/tbird<PCI>-<K> char device
///     per mailbox.
///
/// Cached in a function-static so initImpl() in plugin and device share a
/// consistent view across calls.
static const std::vector<std::string> &getThunderbirdDevicePaths() {
  static std::vector<std::string> paths = []() {
    std::vector<std::string> p;
    if (const char *list = getenv("THUNDERBIRD_DEVICES")) {
      std::string names(list);
      size_t start = 0;
      while (start <= names.size()) {
        size_t end = names.find(',', start);
        if (end == std::string::npos)
          end = names.size();
        if (end > start)
          p.emplace_back(names.substr(start, end - start));
        start = end + 1;
      }
      return p;
    }
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

  /// The loaded image this kernel is in.
  tbird_image_t LoadedImage = nullptr;
};

/// Class implementing the Thunderbird device images properties.
///
/// The image's bytes are written to device memory, and the device loads them
/// once (tbird_load_image); every kernel of the image runs in that loaded copy.
struct ThunderbirdDeviceImageTy : public DeviceImageTy {
  /// Create the Thunderbird image with the id and the image contents.
  ThunderbirdDeviceImageTy(int32_t ImageId, GenericDeviceTy &Device,
                           std::unique_ptr<MemoryBuffer> &&Image)
      : DeviceImageTy(ImageId, Device, std::move(Image)) {}

  /// The library's handle of the loaded image; null until loaded.
  tbird_image_t Handle = nullptr;
  /// Device address of the image's bytes.
  uint64_t Addr = 0;
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
    ODBG(OLDT_Init) << llvm::format("=== initImpl START (DeviceId=%d) ===", DeviceId);

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

    ctx = tbird_open(device_path.c_str());
    if (!ctx) {
      ODBG(OLDT_Init) << llvm::format("ERROR: tbird_open failed: %s", tbird_last_error());
      return Plugin::error(ErrorCode::UNKNOWN,
                          "Failed to initialize Thunderbird context: device=%s: %s",
                          device_path.c_str(), tbird_last_error());
    }

    ODBG(OLDT_Init) << llvm::format("SUCCESS: Thunderbird context initialized: ctx=%p", (void*)ctx);

    ODBG(OLDT_Init) << "=== initImpl COMPLETE ===";
    fprintf(stderr, "[THUNDERBIRD RTL] Device initImpl COMPLETE\n");
    fflush(stderr);
    return Plugin::success();
  }

  /// Unload the binary image
  ///
  /// Unload the binary image and free associated resources
  Error unloadBinaryImpl(DeviceImageTy *Image) override {
    ODBG(OLDT_Module) << "=== unloadBinaryImpl START ===";
    
    auto TBirdImage = reinterpret_cast<ThunderbirdDeviceImageTy *>(Image);
    
    if (!TBirdImage) {
      ODBG(OLDT_Module) << "WARNING: Image is NULL";
      return Plugin::success();
    }
    
    // The image's device memory is not freed: the device keeps a loaded
    // image until the context closes, so the library refuses to free it.
    ODBG(OLDT_Module) << llvm::format("Unloading image: Image=%p, device memory 0x%lx stays until the device closes", (void*)TBirdImage, (unsigned long)TBirdImage->Addr);

    ODBG(OLDT_Module) << "Freeing Image object";
    TBirdImage->~ThunderbirdDeviceImageTy();
    Plugin.free(TBirdImage);
    
    ODBG(OLDT_Module) << "=== unloadBinaryImpl COMPLETE ===";

    return Plugin::success();
  }

  /// Deinitialize the device: close the context, which releases its memory
  /// and images. The generic layer has unloaded the images and freed the
  /// remaining allocations by now, and makes no further calls on this device.
  Error deinitImpl() override {
    tbird_close(ctx);
    ctx = nullptr;
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
    ODBG(OLDT_Module) << llvm::format("=== loadBinaryImpl START (ImageId=%d) ===", ImageId);
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

    uint64_t ImageAddr = 0;
    if (auto Err = Plugin::check(tbird_alloc(ctx, ImageSize, &ImageAddr),
                                 "allocating %zu bytes for the image failed: %s",
                                 ImageSize))
      return Discard(std::move(Err));
    ODBG(OLDT_Module) << llvm::format("Image memory: %zu bytes at device address 0x%lx", ImageSize, (unsigned long)ImageAddr);

    if (auto Err = Plugin::check(tbird_write(ctx, ImageAddr, ImageStart, ImageSize),
                                 "tbird_write failed for image: %s")) {
      ODBG(OLDT_Module) << llvm::format("ERROR: tbird_write failed for image: %s", tbird_last_error());
      tbird_free(ctx, ImageAddr);
      return Discard(std::move(Err));
    }

    uint64_t LoadBase = 0;
    tbird_status_t Status =
        tbird_load_image(ctx, ImageAddr, ImageSize, &Image->Handle, &LoadBase);
    if (Status != TBIRD_SUCCESS) {
      ODBG(OLDT_Module) << llvm::format("ERROR: tbird_load_image failed: %s", tbird_last_error());
      // A device that cannot load the image says TBIRD_ERROR_DEVICE: what it
      // refused is the image.
      Error Err = Status == TBIRD_ERROR_DEVICE
                      ? Plugin::error(ErrorCode::INVALID_BINARY,
                                      "loading the image on the device failed: %s",
                                      tbird_last_error())
                      : Plugin::check(Status,
                                      "loading the image on the device failed: %s");
      tbird_free(ctx, ImageAddr);
      return Discard(std::move(Err));
    }

    Image->Addr = ImageAddr;
    Image->LoadBase = LoadBase;
    ODBG(OLDT_Module) << llvm::format("SUCCESS: image of %zu bytes at 0x%lx, loaded on the device at base 0x%lx", ImageSize, (unsigned long)ImageAddr, (unsigned long)LoadBase);
    ODBG(OLDT_Module) << "=== loadBinaryImpl COMPLETE ===";
    
    return Image;
  }

  /// Allocate memory: device, default and shared allocations are device
  /// memory (tbird_alloc, device addresses); host allocations std::malloc.
  Expected<void *> allocate(size_t Size, void *, TargetAllocTy Kind,
                            size_t Alignment) override {
    ODBG(OLDT_Alloc) << llvm::format("=== allocate(Size=%zu, Kind=%d, Alignment=%zu) ===", Size, (int)Kind, Alignment);

    // Both tbird_alloc and std::malloc return TBIRD_ALLOC_ALIGNMENT-aligned
    // memory and nothing stronger; a larger alignment is refused rather than
    // ignored.
    if (Alignment > TBIRD_ALLOC_ALIGNMENT)
      return Plugin::error(ErrorCode::UNSUPPORTED,
                           "allocation alignment %zu exceeds the %zu bytes "
                           "Thunderbird allocations are aligned to",
                           Alignment, (size_t)TBIRD_ALLOC_ALIGNMENT);

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

        uint64_t Addr = 0;
        tbird_status_t Status = tbird_alloc(ctx, Size, &Addr);
        if (Status == TBIRD_ERROR_NO_DEVICE_MEMORY) {
          // nullptr rather than an error: the memory manager then frees the
          // blocks it keeps and tries again.
          ODBG(OLDT_Alloc) << llvm::format("ERROR: tbird_alloc(%zu) failed: %s", Size, tbird_last_error());
          return nullptr;
        }
        if (auto Err = Plugin::check(Status, "tbird_alloc(%zu) failed: %s", Size))
          return std::move(Err);

        ODBG(OLDT_Alloc) << llvm::format("SUCCESS: tbird_alloc(%zu) → 0x%lx", Size, (unsigned long)Addr);
        return reinterpret_cast<void *>(Addr);
      }
    case TARGET_ALLOC_HOST:
      ODBG(OLDT_Alloc) << "HOST allocation (using malloc)";
      MemAlloc = std::malloc(Size);
      break;
    }
    return MemAlloc;
  }

  /// Free memory allocate() returned.
  Error free(void *TgtPtr, TargetAllocTy Kind) override {
    ODBG(OLDT_Alloc) << llvm::format("=== free(TgtPtr=%p, Kind=%d) ===", TgtPtr, (int)Kind);

    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
      return Plugin::check(tbird_free(ctx, reinterpret_cast<uint64_t>(TgtPtr)),
                           "freeing device memory %p failed: %s", TgtPtr);
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
  ODBG(OLDT_DataTransfer) << llvm::format("=== dataSubmit(TgtPtr=%p, HstPtr=%p, Size=%ld) ===", TgtPtr, HstPtr, Size);
  
  // Validate inputs
  if (!ctx) {
    ODBG(OLDT_DataTransfer) << "ERROR: ctx is NULL";
    return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in dataSubmitImpl");
  }
  
  if (!HstPtr) {
    ODBG(OLDT_DataTransfer) << "ERROR: HstPtr is NULL";
    return Plugin::error(ErrorCode::UNKNOWN, "HstPtr is NULL");
  }
  
  if (Size < 0)
    return Plugin::error(ErrorCode::INVALID_ARGUMENT,
                         "dataSubmit: negative size %ld", (long)Size);

  if (auto Err = Plugin::check(
          tbird_write(ctx, reinterpret_cast<uint64_t>(TgtPtr), HstPtr, Size),
          "tbird_write failed: %s")) {
    ODBG(OLDT_DataTransfer) << llvm::format("ERROR: tbird_write failed: %s", tbird_last_error());
    return Err;
  }

  ODBG(OLDT_DataTransfer) << llvm::format("SUCCESS: dataSubmit: %ld bytes to %p", Size, TgtPtr);

  return Plugin::success();
}

  /// Retrieve data from the device (device to host transfer).
  Error dataRetrieveImpl(void *HstPtr, const void *TgtPtr, int64_t Size,
                         AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    ODBG(OLDT_DataTransfer) << llvm::format("=== dataRetrieve(HstPtr=%p, TgtPtr=%p, Size=%ld) ===", HstPtr, TgtPtr, Size);
    
    // Validate inputs
    if (!ctx) {
      ODBG(OLDT_DataTransfer) << "ERROR: ctx is NULL";
      return Plugin::error(ErrorCode::UNKNOWN, "ctx is NULL in dataRetrieveImpl");
    }
    
    if (!HstPtr) {
      ODBG(OLDT_DataTransfer) << "ERROR: HstPtr is NULL";
      return Plugin::error(ErrorCode::UNKNOWN, "HstPtr is NULL");
    }
    
    if (Size < 0)
      return Plugin::error(ErrorCode::INVALID_ARGUMENT,
                           "dataRetrieve: negative size %ld", (long)Size);

    if (auto Err = Plugin::check(
            tbird_read(ctx, reinterpret_cast<uint64_t>(TgtPtr), HstPtr, Size),
            "tbird_read failed: %s")) {
      ODBG(OLDT_DataTransfer) << llvm::format("ERROR: tbird_read failed: %s", tbird_last_error());
      return Err;
    }

    ODBG(OLDT_DataTransfer) << llvm::format("SUCCESS: dataRetrieve: %ld bytes from %p", Size, TgtPtr);

    return Plugin::success();
  }

  /// Every data operation is a synchronous library call, so nothing can be
  /// reordered.
  Error dataFence(__tgt_async_info *AsyncInfo) override {
    return Plugin::success();
  }

  /// Fill device memory with a repeated pattern, built on the host and
  /// written to the device: a device pointer is the device's address, which
  /// the host cannot write through.
  Error dataFillImpl(void *TgtPtr, const void *PatternPtr, int64_t PatternSize,
                     int64_t Size,
                     AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    if (Size <= 0)
      return Plugin::success();
    if (PatternSize <= 0 || !PatternPtr)
      return Plugin::error(ErrorCode::INVALID_ARGUMENT,
                           "dataFill: invalid pattern (size %ld)",
                           (long)PatternSize);
    std::vector<char> Fill(Size);
    for (int64_t I = 0; I < Size; I += PatternSize)
      std::memcpy(Fill.data() + I, PatternPtr,
                  std::min<int64_t>(PatternSize, Size - I));
    return Plugin::check(tbird_write(ctx, reinterpret_cast<uint64_t>(TgtPtr),
                                     Fill.data(), Size),
                         "tbird_write failed: %s");
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

  /// The library's context of this device. The library runs the calls on it
  /// one at a time, so the plugin holds no lock of its own.
  tbird_context_t ctx = nullptr;

private:
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
  ODBG(OLDT_Init) << llvm::format("=== ThunderbirdKernelTy::initImpl START (kernel=%s) ===", getName());
  fprintf(stderr, "[THUNDERBIRD RTL] Kernel initImpl called for: %s\n", getName());
  fflush(stderr);
  
  // The loaded image the kernel runs in
  auto &TBirdImage = static_cast<ThunderbirdDeviceImageTy &>(Image);
  LoadedImage = TBirdImage.Handle;

  if (!LoadedImage) {
    ODBG(OLDT_Init) << llvm::format("ERROR: Image not loaded for kernel %s", getName());
    return Plugin::error(ErrorCode::INVALID_BINARY,
                        "Image not loaded for kernel %s", getName());
  }

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

  ODBG(OLDT_Init) << "=== ThunderbirdKernelTy::initImpl COMPLETE ===";
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

  // Every argument is one pointer-sized value, sent unchanged: device memory is
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

  ODBG(OLDT_Kernel) << llvm::format("tbird_launch: slots=%u, thread_limit=%u", LaunchParams.NumArgs, ThreadLimit);
  if (auto Err = Plugin::check(tbird_launch(TBirdDevice->ctx, LoadedImage,
                                            getName(), Slots,
                                            LaunchParams.NumArgs, ThreadLimit),
                               "kernel %s: %s", getName()))
    return Err;

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

  /// Thunderbird devices do not access each other's memory, so no pair of
  /// devices exchanges data directly: omp_target_memcpy between two devices
  /// copies through a host buffer (retrieveData, then submitData).
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

/// Check a libtbird_host status: an error with the ErrorCode closest to it,
/// and the library's reason for the last %s of ErrMsg. TIMEOUT, EXEC_FAILED
/// and DEVICE (a device-side failure, and on the mailbox transport any refusal
/// the device makes) have no closer code than UNKNOWN.
template <typename... ArgsTy>
static Error Plugin::check(int32_t Code, const char *ErrMsg, ArgsTy... Args) {
  if (Code == TBIRD_SUCCESS)
    return Plugin::success();

  ErrorCode OffloadErrCode;
  switch (Code) {
  case TBIRD_ERROR_INVALID_PARAM:
  case TBIRD_ERROR_OUT_OF_RANGE:
  case TBIRD_ERROR_FAILED_PRECONDITION:
    OffloadErrCode = ErrorCode::INVALID_ARGUMENT;
    break;
  case TBIRD_ERROR_NOT_FOUND:
    OffloadErrCode = ErrorCode::NOT_FOUND;
    break;
  case TBIRD_ERROR_NO_DEVICE_MEMORY:
  case TBIRD_ERROR_NO_HOST_MEMORY:
    OffloadErrCode = ErrorCode::OUT_OF_RESOURCES;
    break;
  case TBIRD_ERROR_IO:
    OffloadErrCode = ErrorCode::HOST_IO;
    break;
  case TBIRD_ERROR_DEVICE_LOST:
  case TBIRD_ERROR_PROTOCOL:
    OffloadErrCode = ErrorCode::BACKEND_FAILURE;
    break;
  default:
    OffloadErrCode = ErrorCode::UNKNOWN;
  }

  return Plugin::error(OffloadErrCode, ErrMsg, Args..., tbird_last_error());
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
