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

#include <cassert>
#include <cstddef>
#include <ffi.h>
#include <string>
#include <variant>
#include <unordered_map>

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

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) ||           \
    !defined(__ORDER_BIG_ENDIAN__)
#error "Missing preprocessor definitions for endianness detection."
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define LITTLEENDIAN_CPU
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BIGENDIAN_CPU
#endif

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

    // Free image buffer if allocated via new API
    if (TBirdImage->image_buffer) {
      if (!ctx) {
        DP("WARNING: ctx is NULL, cannot free image buffer\n");
      } else {
        DP("Freeing image buffer: tbird_free_buffer(ctx=%p, buffer=%p)\n",
           (void*)ctx, (void*)TBirdImage->image_buffer);
        tbird_free_buffer(ctx, TBirdImage->image_buffer);
        DP("Image buffer freed\n");
      }
      TBirdImage->image_buffer = nullptr;
    } else {
      DP("No image buffer to free\n");
    }

    DP("Freeing Image object\n");
    Plugin.free(TBirdImage);
    
    DP("=== Phase 3: unloadBinaryImpl COMPLETE ===\n");

    return Plugin::success();
  }

  /// Deinitialize the device, which is a no-op
  Error deinitImpl() override { return Plugin::success(); }

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
    
    // Allocate shared buffer for image
    DP("Allocating image buffer: tbird_alloc_buffer(ctx=%p, size=%zu)\n",
       (void*)ctx, ImageSize);
    
    tbird_buffer_t image_buf = tbird_alloc_buffer(ctx, ImageSize);
    if (!image_buf) {
      DP("ERROR: tbird_alloc_buffer failed for image: %s\n", tbird_last_error(ctx));
      Plugin.free(Image);
      return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                          "tbird_alloc_buffer failed for image: %s", 
                          tbird_last_error(ctx));
    }
    
    DP("SUCCESS: Allocated image buffer=%p\n", (void*)image_buf);
    
    // Upload image to shared buffer via ioctl
    DP("Uploading image: tbird_buffer_write(buffer=%p, offset=0, src=%p, size=%zu)\n",
       (void*)image_buf, TgtImage->ImageStart, ImageSize);
    
    tbird_status_t status = tbird_buffer_write(ctx, image_buf, 0, 
                                               TgtImage->ImageStart, ImageSize);
    if (status != TBIRD_SUCCESS) {
      DP("ERROR: tbird_buffer_write failed for image: %s\n", tbird_last_error(ctx));
      tbird_free_buffer(ctx, image_buf);
      Plugin.free(Image);
      return Plugin::error(ErrorCode::UNKNOWN,
                          "tbird_buffer_write failed for image: %s",
                          tbird_last_error(ctx));
    }
    
    void *host_ptr = tbird_buffer_host_ptr(image_buf);
    DP("SUCCESS: Image uploaded: buffer=%p, host_ptr=%p\n", 
       (void*)image_buf, host_ptr);
    
    if (!host_ptr) {
      DP("ERROR: tbird_buffer_host_ptr returned NULL\n");
      tbird_free_buffer(ctx, image_buf);
      Plugin.free(Image);
      return Plugin::error(ErrorCode::UNKNOWN, "tbird_buffer_host_ptr returned NULL");
    }
    
    // Store buffer handle in image object
    DP("Storing image_buffer=%p in Image object\n", (void*)image_buf);
    Image->image_buffer = image_buf;
    
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
        // Validate context
        if (!ctx) {
          DP("ERROR: ctx is NULL, cannot allocate buffer\n");
          return nullptr;
        }
        
        DP("Calling tbird_alloc_buffer(ctx=%p, size=%zu)\n", (void*)ctx, Size);
        
        // Call new API to allocate buffer
        tbird_buffer_t buffer = tbird_alloc_buffer(ctx, Size);
        if (!buffer) {
          DP("ERROR: tbird_alloc_buffer returned NULL: %s\n", tbird_last_error(ctx));
          return nullptr;
        }
        
        DP("SUCCESS: tbird_alloc_buffer returned buffer=%p\n", (void*)buffer);
        
        // Get host pointer for the buffer
        void *host_ptr = tbird_buffer_host_ptr(buffer);
        if (!host_ptr) {
          DP("ERROR: tbird_buffer_host_ptr returned NULL\n");
          tbird_free_buffer(ctx, buffer);
          return nullptr;
        }
        
        DP("Got host_ptr=%p from buffer\n", host_ptr);
        
        // Register mapping for future free/transfer operations
        address_to_buffer[host_ptr] = buffer;
        DP("Registered in address_to_buffer map (now %zu entries)\n", 
           address_to_buffer.size());
        
        DP("SUCCESS: Allocated buffer: size=%zu, host_ptr=%p, buffer=%p\n", 
           Size, host_ptr, (void*)buffer);
        
        return host_ptr;
      }
    case TARGET_ALLOC_HOST:
      DP("HOST allocation (using malloc)\n");
      MemAlloc = std::malloc(Size);
      break;
    }
    return MemAlloc;
  }

  /// Free the memory. Use std::free in all cases.
  // TODO: switch the free below for the target free
  int free(void *TgtPtr, TargetAllocTy Kind) override {
    DP("=== Phase 2: free(TgtPtr=%p, Kind=%d) ===\n", TgtPtr, (int)Kind);
    
    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
    case TARGET_ALLOC_DEVICE_NON_BLOCKING:
      {
        // Validate context
        if (!ctx) {
          DP("ERROR: ctx is NULL, cannot free buffer\n");
          return OFFLOAD_FAIL;
        }
        
        DP("Looking up pointer in address_to_buffer (%zu entries)\n",
           address_to_buffer.size());
        
        // Look up buffer handle from pointer
        auto it = address_to_buffer.find(TgtPtr);
        if (it == address_to_buffer.end()) {
          DP("ERROR: pointer %p not found in buffer registry\n", TgtPtr);
          DP("Registry contents: ");
          for (const auto &entry : address_to_buffer) {
            DP("  %p -> %p\n", entry.first, (void*)entry.second);
          }
          return OFFLOAD_FAIL;
        }
        
        tbird_buffer_t buffer = it->second;
        DP("Found buffer=%p for host_ptr=%p\n", (void*)buffer, TgtPtr);
        
        // Free via new API (returns void)
        DP("Calling tbird_free_buffer(ctx=%p, buffer=%p)\n", (void*)ctx, (void*)buffer);
        tbird_free_buffer(ctx, buffer);
        
        // Remove from registry
        address_to_buffer.erase(it);
        DP("Removed from registry (now %zu entries)\n", address_to_buffer.size());
        
        DP("SUCCESS: Freed buffer: host_ptr=%p, buffer=%p\n", TgtPtr, (void*)buffer);
        
        return OFFLOAD_SUCCESS;
      }
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
  
  // Look up buffer handle
  DP("Looking up TgtPtr in buffer registry (%zu entries)\n", address_to_buffer.size());
  auto it = address_to_buffer.find(TgtPtr);
  if (it == address_to_buffer.end()) {
    DP("ERROR: pointer %p not in buffer registry\n", TgtPtr);
    return Plugin::error(ErrorCode::UNKNOWN,
                        "dataSubmit: pointer %p not in buffer registry", TgtPtr);
  }
  
  tbird_buffer_t buffer = it->second;
  DP("Found buffer=%p for TgtPtr=%p\n", (void*)buffer, TgtPtr);
  
  // Transfer via driver ioctl
  DP("Calling tbird_buffer_write(ctx=%p, buffer=%p, offset=0, src=%p, size=%ld)\n",
     (void*)ctx, (void*)buffer, HstPtr, Size);
  
  tbird_status_t status = tbird_buffer_write(ctx, buffer, 0, HstPtr, Size);
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
    
    // Look up buffer handle
    DP("Looking up TgtPtr in buffer registry (%zu entries)\n", address_to_buffer.size());
    auto it = address_to_buffer.find(const_cast<void*>(TgtPtr));
    if (it == address_to_buffer.end()) {
      DP("ERROR: pointer %p not in buffer registry\n", TgtPtr);
      return Plugin::error(ErrorCode::UNKNOWN,
                          "dataRetrieve: pointer %p not in buffer registry", TgtPtr);
    }
    
    tbird_buffer_t buffer = it->second;
    DP("Found buffer=%p for TgtPtr=%p\n", (void*)buffer, TgtPtr);
    
    // Transfer via driver ioctl
    DP("Calling tbird_buffer_read(ctx=%p, buffer=%p, offset=0, dst=%p, size=%ld)\n",
       (void*)ctx, (void*)buffer, HstPtr, Size);
    
    tbird_status_t status = tbird_buffer_read(ctx, buffer, 0, HstPtr, Size);
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
  // TODO: do we need to setup memory pools?
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

  /// Buffer registry: maps host pointers to buffer handles
  std::unordered_map<void*, tbird_buffer_t> address_to_buffer;

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
  
  if (!image_buffer) {
    DP("ERROR: Image buffer is NULL for kernel %s\n", getName());
    return Plugin::error(ErrorCode::INVALID_BINARY,
                        "Image buffer not loaded for kernel %s", getName());
  }
  
  DP("Stored image_buffer=%p for kernel %s\n", (void*)image_buffer, getName());
 
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

Error ThunderbirdKernelTy::launchImpl(GenericDeviceTy &GenericDevice, uint32_t NumThreads[3],
                 uint32_t NumBlocks[3], KernelArgsTy &KernelArgs,
                 KernelLaunchParamsTy LaunchParams,
                 AsyncInfoWrapperTy &AsyncInfoWrapper) const {
  fprintf(stderr, "[THUNDERBIRD RTL] launchImpl START: kernel=%s, NumArgs=%u\n", getName(), KernelArgs.NumArgs);
  fflush(stderr);
  DP("=== Phase 4: launchImpl START ===\n");
  DP("Kernel: %s\n", getName());
  DP("NumBlocks: [%u, %u, %u]\n", NumBlocks[0], NumBlocks[1], NumBlocks[2]);
  DP("NumThreads: [%u, %u, %u]\n", NumThreads[0], NumThreads[1], NumThreads[2]);
  DP("NumArgs: %u\n", KernelArgs.NumArgs);
  
  // Check for execution mode hints
  bool is_spmd = (NumThreads[0] > 1 || NumThreads[1] > 1 || NumThreads[2] > 1);
  bool is_generic = (NumThreads[0] == 1 && NumThreads[1] == 1 && NumThreads[2] == 1);
  DP("Execution mode: %s (threads=%u)\n", 
     is_spmd ? "SPMD" : (is_generic ? "GENERIC" : "UNKNOWN"),
     NumThreads[0] * NumThreads[1] * NumThreads[2]);
  
  // In GENERIC mode, kernels typically expect a thread_id parameter
  // In SPMD mode, they use actual thread indices
  if (is_generic) {
    DP("GENERIC mode detected - kernel MAY expect thread_id as first parameter\n");
  }
  
  DP("LaunchParams.Size: %zu\n", LaunchParams.Size);
  DP("LaunchParams.Data: %p\n", LaunchParams.Data);
  DP("LaunchParams.Ptrs: %p\n", LaunchParams.Ptrs);
  DP("KernelArgs.ArgPtrs: %p\n", (void*)KernelArgs.ArgPtrs);
  DP("KernelArgs.ArgCTypes: %p\n", (void*)KernelArgs.ArgCTypes);
  fflush(stdout);
  
  // Cast to Thunderbird device to access ctx
  auto *TBirdDevice = static_cast<ThunderbirdDeviceTy *>(&GenericDevice);
  
  DP("Using ctx=%p, image_buffer=%p\n", (void*)TBirdDevice->ctx, (void*)image_buffer);
  
  // Debug: Print entire LaunchParams.Ptrs array
  if (LaunchParams.Ptrs) {
    size_t num_ptrs = LaunchParams.Size / sizeof(void*);
    DP("LaunchParams.Ptrs array (%zu entries):\n", num_ptrs);
    for (size_t i = 0; i < num_ptrs; i++) {
      void *entry = LaunchParams.Ptrs[i];
      void *dereferenced = entry ? *(void**)entry : nullptr;
      DP("  Ptrs[%zu]=%p -> *Ptrs[%zu]=%p\n", i, entry, i, dereferenced);
    }
    fflush(stdout);
  }
  
  // Debug: Print buffer registry
  DP("Buffer registry (%zu entries):\n", TBirdDevice->address_to_buffer.size());
  for (const auto &entry : TBirdDevice->address_to_buffer) {
    DP("  address=%p -> buffer=%p\n", entry.first, (void*)entry.second);
  }
  fflush(stdout);
  
  // Check if we have arguments to process
  if (KernelArgs.NumArgs == 0) {
    DP("WARNING: No kernel arguments\n");
  }
  
  if (KernelArgs.NumArgs > TBIRD_MAX_ARGS) {
    DP("ERROR: Too many arguments: %u > %d\n", KernelArgs.NumArgs, TBIRD_MAX_ARGS);
    return Plugin::error(ErrorCode::UNKNOWN, 
                        "Too many kernel arguments: %u (max %d)", 
                        KernelArgs.NumArgs, TBIRD_MAX_ARGS);
  }
  
  // Build typed argument array for new API
  DP("Converting %u arguments from OpenMP format to tbird_arg_t[]\n", KernelArgs.NumArgs);
  fflush(stdout);
  
  tbird_arg_t args[TBIRD_MAX_ARGS];
  memset(args, 0, sizeof(args));
  
  // Track actual number of arguments (excluding VOID)
  uint32_t actual_arg_count = 0;
  
  // Process each argument
  for (uint32_t i = 0; i < KernelArgs.NumArgs; i++) {
    // Get type from ArgCTypes array
    uint8_t omp_ctype = KernelArgs.ArgCTypes ? KernelArgs.ArgCTypes[i] : 11; // default to PTR
    tbird_arg_type_t arg_type_converted = convert_omp_ctype_to_tbird(omp_ctype);
    
    // Skip VOID arguments (padding/internal use)
    if (arg_type_converted == TBIRD_TYPE_VOID) {
      DP("  arg[%u]: VOID type - skipping\n", i);
      DP("    Position: %u of %u total args\n", i, KernelArgs.NumArgs);
      DP("    ArgPtrs[%u]=%p\n", i, KernelArgs.ArgPtrs[i]);
      
      // Check if this VOID might be a placeholder for a thread ID parameter
      if (i == KernelArgs.NumArgs - 1) {
        DP("    VOID is LAST argument - likely just padding\n");
      } else if (i == 0) {
        DP("    VOID is FIRST argument - could be thread ID placeholder!\n");
      } else {
        DP("    VOID is in MIDDLE at position %u\n", i);
      }
      fflush(stdout);
      continue;
    }
    
    // Use the actual argument index (after skipping VOIDs)
    args[actual_arg_count].type = arg_type_converted;
    
    // Get argument type flags to determine if by-value or by-reference
    // Note: ArgTypes may be shorter than ArgCTypes (doesn't include hidden params)
    int64_t arg_type = 0;
    bool has_map_type = false;
    
    // Check if this argument has a corresponding OpenMP mapping entry
    // Hidden/implicit args have ArgCTypes entry but no ArgTypes entry
    if (KernelArgs.ArgTypes) {
      // Heuristic: if ArgPtrs[i] looks like firstprivate value (< 0x10000)
      // it likely has an ArgTypes entry. Otherwise we may be past array bounds.
      uintptr_t ptr_val = (uintptr_t)KernelArgs.ArgPtrs[i];
      if (ptr_val < 0x10000 || (ptr_val & 0xFFFFFFFF00000000) == 0) {
        // Likely a by-value arg or has valid mapping
        arg_type = KernelArgs.ArgTypes[i];
        has_map_type = true;
      }
    }
    
    bool is_literal = (arg_type & 0x100);  // OMP_TGT_MAPTYPE_LITERAL (by-value)
    
    // Decode OpenMP map type flags for debugging
    bool is_to = (arg_type & 0x1);      // OMP_TGT_MAPTYPE_TO
    bool is_from = (arg_type & 0x2);    // OMP_TGT_MAPTYPE_FROM
    bool is_alloc = (arg_type & 0x4);   // OMP_TGT_MAPTYPE_ALLOC
    bool is_delete = (arg_type & 0x8);  // OMP_TGT_MAPTYPE_DELETE
    bool is_implicit = (arg_type & 0x200); // OMP_TGT_MAPTYPE_IMPLICIT
    
    DP("  arg[%u -> %u]: omp_ctype=%u -> tbird_type=%d\n", 
       i, actual_arg_count, omp_ctype, (int)args[actual_arg_count].type);
    DP("    arg_type=0x%lx: TO=%d FROM=%d ALLOC=%d DELETE=%d LITERAL=%d IMPLICIT=%d\n",
       arg_type, is_to, is_from, is_alloc, is_delete, is_literal, is_implicit);
    DP("    ArgPtrs[%u]=%p (as_uintptr=0x%lx)\n", 
       i, KernelArgs.ArgPtrs[i], (uintptr_t)KernelArgs.ArgPtrs[i]);
    
    // Check if ArgPtrs[i] looks like a pointer vs a scalar value
    uintptr_t ptr_as_int = (uintptr_t)KernelArgs.ArgPtrs[i];
    bool looks_like_address = (ptr_as_int > 0x10000) && 
                              ((ptr_as_int & 0xFFFF000000000000ULL) != 0 || 
                               (ptr_as_int & 0x00007FFFFFFFF000ULL) != 0);
    DP("    looks_like_address=%d (based on value pattern)\n", looks_like_address);
    fflush(stdout);
    
    if (args[actual_arg_count].type == TBIRD_TYPE_PTR) {
      // Pointer argument - OpenMP uses Ptrs[i+1] for PTR arguments
      if (!LaunchParams.Ptrs) {
        DP("ERROR: LaunchParams.Ptrs is NULL\n");
        return Plugin::error(ErrorCode::UNKNOWN, "LaunchParams.Ptrs is NULL");
      }
      
      // OpenMP stores device pointers at Ptrs[i+1] (confirmed by instrumentation)
      uint32_t ptr_index = i + 1;
      if (ptr_index >= KernelArgs.NumArgs) {
        DP("ERROR: Ptr index %u >= NumArgs %u\n", ptr_index, KernelArgs.NumArgs);
        return Plugin::error(ErrorCode::UNKNOWN, "Ptrs index out of bounds");
      }
      
      void *device_ptr = *(void**)LaunchParams.Ptrs[ptr_index];
      
      // Verify it's in our buffer registry
      if (TBirdDevice->address_to_buffer.find(device_ptr) == TBirdDevice->address_to_buffer.end()) {
        DP("ERROR: Device pointer %p not in buffer registry\n", device_ptr);
        return Plugin::error(ErrorCode::UNKNOWN, "Device pointer not in buffer registry");
      }
      
      args[actual_arg_count].value.ptr = device_ptr;
      DP("    PTR: %p (from *LaunchParams.Ptrs[%u])\n", device_ptr, ptr_index);
      fflush(stdout);
      
    } else {
      // Scalar argument
      size_t scalar_size = 8; // Default
      
      // Determine actual size based on type
      switch (args[actual_arg_count].type) {
        case TBIRD_TYPE_INT8:
        case TBIRD_TYPE_UINT8:
          scalar_size = 1;
          break;
        case TBIRD_TYPE_INT16:
        case TBIRD_TYPE_UINT16:
          scalar_size = 2;
          break;
        case TBIRD_TYPE_INT32:
        case TBIRD_TYPE_UINT32:
        case TBIRD_TYPE_FLOAT:
          scalar_size = 4;
          break;
        case TBIRD_TYPE_INT64:
        case TBIRD_TYPE_UINT64:
        case TBIRD_TYPE_DOUBLE:
          scalar_size = 8;
          break;
        default:
          scalar_size = 8;
      }
      
      // Check if this scalar has device memory (by-reference)
      // Use Ptrs[i+1] pattern like we do for PTR arguments
      if (LaunchParams.Ptrs && !is_literal) {
        uint32_t ptr_index = i + 1;
        if (ptr_index < LaunchParams.Size / sizeof(void*)) {
          void *potential_device_ptr = *(void**)LaunchParams.Ptrs[ptr_index];
          
          DP("    Checking LaunchParams.Ptrs[%u]=%p, dereferenced=*Ptrs[%u]=%p\n",
             ptr_index, LaunchParams.Ptrs[ptr_index], ptr_index, potential_device_ptr);
          fflush(stdout);
          
          // Check if this is in buffer registry (means it's a mapped device pointer)
          auto it = TBirdDevice->address_to_buffer.find(potential_device_ptr);
          if (it != TBirdDevice->address_to_buffer.end()) {
            // This scalar by-ref has a device mapping - pass pointer to it
            args[actual_arg_count].value.ptr = potential_device_ptr;
            DP("    Scalar by-ref found in Ptrs[%u] as device ptr: %p -> treating as PTR\n", 
               ptr_index, potential_device_ptr);
            fflush(stdout);
            // Change type to PTR for device API
            args[actual_arg_count].type = TBIRD_TYPE_PTR;
            actual_arg_count++;
            continue;
          }
        }
      }
      
      // Firstprivate/by-value scalar
      if (is_literal) {
        // ArgPtrs[i] IS the value itself (passed by value), not a pointer
        // This only happens for non-pointer scalar types
        DP("    Scalar by-value: ArgPtrs[%u]=%p (direct value)\n", i, KernelArgs.ArgPtrs[i]);
        fflush(stdout);
        
        // Cast the pointer value to the appropriate integer type
        uintptr_t value_as_int = (uintptr_t)KernelArgs.ArgPtrs[i];
        
        // Write the value with correct size based on type
        switch (args[actual_arg_count].type) {
          case TBIRD_TYPE_INT8:
          case TBIRD_TYPE_UINT8:
            *(uint8_t*)args[actual_arg_count].value.scalar_bytes = (uint8_t)value_as_int;
            break;
          case TBIRD_TYPE_INT16:
          case TBIRD_TYPE_UINT16:
            *(uint16_t*)args[actual_arg_count].value.scalar_bytes = (uint16_t)value_as_int;
            break;
          case TBIRD_TYPE_INT32:
          case TBIRD_TYPE_UINT32:
            *(uint32_t*)args[actual_arg_count].value.scalar_bytes = (uint32_t)value_as_int;
            break;
          case TBIRD_TYPE_INT64:
          case TBIRD_TYPE_UINT64:
            *(uint64_t*)args[actual_arg_count].value.scalar_bytes = (uint64_t)value_as_int;
            break;
          case TBIRD_TYPE_FLOAT: {
            // For float, interpret the lower 32 bits as float representation
            uint32_t bits = (uint32_t)value_as_int;
            memcpy(args[actual_arg_count].value.scalar_bytes, &bits, sizeof(bits));
            break;
          }
          case TBIRD_TYPE_DOUBLE: {
            // For double, interpret the 64 bits as double representation
            memcpy(args[actual_arg_count].value.scalar_bytes, &value_as_int, sizeof(value_as_int));
            break;
          }
          default:
            *(uint64_t*)args[actual_arg_count].value.scalar_bytes = value_as_int;
        }
      } else {
        // ArgPtrs[i] points to the data (by-reference on host)
        // Check if this address is actually a mapped buffer
        DP("    Scalar by-ref: ArgPtrs[%u]=%p\n", i, KernelArgs.ArgPtrs[i]);
        
        // For FLOATs, show what the bits would be if interpreted as float
        if (args[actual_arg_count].type == TBIRD_TYPE_FLOAT && KernelArgs.ArgPtrs[i]) {
          float value_as_float;
          memcpy(&value_as_float, KernelArgs.ArgPtrs[i], sizeof(float));
          DP("    If dereferenced as float: %.6f\n", value_as_float);
        }
        fflush(stdout);
        
        // Try to identify this as a buffer pointer
        auto it = TBirdDevice->address_to_buffer.find(KernelArgs.ArgPtrs[i]);
        if (it != TBirdDevice->address_to_buffer.end()) {
          // This scalar by-ref is actually a pointer to a mapped buffer!
          DP("    Found in buffer registry at address %p -> treating as PTR\n", 
             it->first);
          fflush(stdout);
          args[actual_arg_count].type = TBIRD_TYPE_PTR;
          args[actual_arg_count].value.ptr = KernelArgs.ArgPtrs[i];
        } else {
          // True scalar by-reference - copy the value
          DP("    Not in buffer registry -> copying %zu bytes as scalar value\n", scalar_size);
          fflush(stdout);
          memcpy(args[actual_arg_count].value.scalar_bytes, KernelArgs.ArgPtrs[i], scalar_size);
        }
      }
      
      // Debug print based on type
      if (args[actual_arg_count].type == TBIRD_TYPE_FLOAT) {
        float val;
        memcpy(&val, args[actual_arg_count].value.scalar_bytes, sizeof(float));
        DP("    FLOAT: %f\n", val);
      } else if (args[actual_arg_count].type == TBIRD_TYPE_DOUBLE) {
        double val;
        memcpy(&val, args[actual_arg_count].value.scalar_bytes, sizeof(double));
        DP("    DOUBLE: %f\n", val);
      } else if (scalar_size <= 4) {
        uint32_t val;
        memcpy(&val, args[actual_arg_count].value.scalar_bytes, scalar_size);
        DP("    SCALAR%zu: 0x%x (%u)\n", scalar_size, val, val);
      } else {
        uint64_t val;
        memcpy(&val, args[actual_arg_count].value.scalar_bytes, scalar_size);
        DP("    SCALAR%zu: 0x%lx (%lu)\n", scalar_size, val, val);
      }
      fflush(stdout);
    }
    
    // Increment actual argument count
    actual_arg_count++;
  }
  
  DP("Argument conversion complete. Actual args: %u (skipped %u VOID args)\n",
     actual_arg_count, KernelArgs.NumArgs - actual_arg_count);
  
  // GENERIC mode kernels expect a thread_id as first parameter
  // Reuse is_generic variable from earlier
  if (is_generic && actual_arg_count > 0) {
    DP("GENERIC mode: Prepending thread_id=0 as first argument\n");
    fflush(stdout);
    
    // Shift all arguments forward by one position
    for (uint32_t i = actual_arg_count; i > 0; i--) {
      args[i] = args[i-1];
    }
    
    // Insert thread_id=0 at position 0
    args[0].type = TBIRD_TYPE_INT64;
    memset(args[0].value.scalar_bytes, 0, sizeof(args[0].value.scalar_bytes));
    actual_arg_count++;
    
    DP("After prepending thread_id: actual_arg_count=%u\n", actual_arg_count);
    fflush(stdout);
  }
  
  // Debug: Print final argument array being sent to device
  DP("Final converted argument array for device:\n");
  for (uint32_t i = 0; i < actual_arg_count; i++) {
    DP("  args[%u]: type=%d ", i, (int)args[i].type);
    if (args[i].type == TBIRD_TYPE_PTR) {
      DP("PTR=%p\n", args[i].value.ptr);
    } else {
      DP("SCALAR bytes:");
      for (size_t j = 0; j < 8; j++) {
        DP(" %02x", args[i].value.scalar_bytes[j]);
      }
      DP("\n");
    }
  }
  fflush(stdout);
  
  // Get image sizefor launch API
  size_t image_size = tbird_buffer_size(image_buffer);
  DP("Image size: %zu bytes\n", image_size);
  
  // Launch kernel via new API
  DP("Calling tbird_launch_kernel_sync:\n");
  DP("  ctx=%p\n", (void*)TBirdDevice->ctx);
  DP("  image_buffer=%p\n", (void*)image_buffer);
  DP("  image_offset=0\n");
  DP("  image_size=%zu\n", image_size);
  DP("  entry_name=%s\n", getName());
  DP("  args=%p\n", (void*)args);
  DP("  num_args=%u (actual, after skipping VOID)\n", actual_arg_count);
  
  tbird_status_t status = tbird_launch_kernel_sync(
      TBirdDevice->ctx,
      image_buffer,
      0,               // image_offset (always 0 for full image)
      image_size,
      getName(),       // kernel entry point symbol name
      args,
      actual_arg_count  // Use actual argument count, not KernelArgs.NumArgs
  );
  
  if (status != TBIRD_SUCCESS) {
    DP("ERROR: tbird_launch_kernel_sync failed: %s\n", 
       tbird_last_error(TBirdDevice->ctx));
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
