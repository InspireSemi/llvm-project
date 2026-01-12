//===-RTLs/thunderbird/src/rtl.cpp - Target RTLs Implementation - C++ -*-=====//
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
  Error initImpl(GenericDeviceTy &Device, DeviceImageTy &Image) override {
   
	  // Functions have zero size.
    GlobalTy Global(getName(), 0);

    // Get the metadata (address) of the kernel function.
    GenericGlobalHandlerTy &GHandler = Device.Plugin.getGlobalHandler();
    if (auto Err = GHandler.getGlobalMetadataFromDevice(Device, Image, Global))
      return Err;

    // Check that the function pointer is valid.
    if (!Global.getPtr())
      return Plugin::error(ErrorCode::INVALID_BINARY,
                           "invalid function for kernel %s", getName());

    // Save the function pointer.
    Func = (void (*)())Global.getPtr();

    KernelEnvironment.Configuration.ExecMode = OMP_TGT_EXEC_MODE_GENERIC;
    KernelEnvironment.Configuration.MayUseNestedParallelism = /*Unknown=*/2;
    KernelEnvironment.Configuration.UseGenericStateMachine = /*Unknown=*/2;

    return Plugin::success();
  }

  Error launchImpl(GenericDeviceTy &GenericDevice, uint32_t NumThreads[3],
                   uint32_t NumBlocks[3], KernelArgsTy &KernelArgs,
                   KernelLaunchParamsTy LaunchParams,
                   AsyncInfoWrapperTy &AsyncInfoWrapper) const override;


private:
  /// The kernel function to execute.
  void (*Func)(void);
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
    // Get device path from environment or use default
    const char *device_path = getenv("THUNDERBIRD_DEVICE_PATH");
    if (!device_path) {
      device_path = "/dev/tbird0018-0";
    }
    
    DP("Initializing Thunderbird device: %s\n", device_path);
    
    // Initialize context with single mailbox (single-threaded operation)
    ctx = tbird_init(device_path, 1);
    if (!ctx) {
      return Plugin::error(ErrorCode::UNKNOWN,
                          "Failed to initialize Thunderbird context: device=%s", 
                          device_path);
    }
    
    DP("Thunderbird context initialized successfully: ctx=%p\n", (void*)ctx);
    
    MaxNumThreads = THUNDERBIRD_MAX_THREADS;
    return Plugin::success();
  }

  /// Unload the binary image
  ///
  /// TODO: This currently does nothing, and should be implemented as part of
  /// broader memory handling logic for this plugin
  /// TODO: Thunderbird: free any latent device memory
  Error unloadBinaryImpl(DeviceImageTy *Image) override {
    auto Elf = reinterpret_cast<ThunderbirdDeviceImageTy *>(Image);

    free((void *) Elf->getBaseImageAddress(), TARGET_ALLOC_DEFAULT);

    Plugin.free(Elf);

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
  // TODO: use the Thunderbird API to load the implementation into memory
  Expected<DeviceImageTy *> loadBinaryImpl(const __tgt_device_image *TgtImage,
                                           int32_t ImageId) override {
    DP("loadBinaryImpl: Phase 1 stub (ImageId=%d)\n", ImageId);
    return Plugin::error(ErrorCode::UNKNOWN, "loadBinaryImpl not yet implemented (Phase 1)");
    
    // Old code below - will be replaced in Phase 3
    /*
    ThunderbirdDeviceImageTy *Image = Plugin.allocate<ThunderbirdDeviceImageTy>();
    new (Image) ThunderbirdDeviceImageTy(ImageId, *this, TgtImage);
    ... old message batch malloc code ...
    Image->setBaseImageAddress(ImageLoc);
    return Image;
    */
  }

  /// Allocate memory. Use std::malloc in all cases.
  // TODO: switch the malloc below for the target malloc
  void *allocate(size_t Size, void *, TargetAllocTy Kind) override {
    if (Size == 0)
      return nullptr;

    void *MemAlloc = nullptr;
    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
    case TARGET_ALLOC_DEVICE_NON_BLOCKING:
      {
        DP("allocate: Phase 1 stub (size=%zu)\n", Size);
        return nullptr;  // Stub for Phase 1
      }
    case TARGET_ALLOC_HOST:
      MemAlloc = std::malloc(Size);
      break;
    }
    return MemAlloc;
  }

  /// Free the memory. Use std::free in all cases.
  // TODO: switch the free below for the target free
  int free(void *TgtPtr, TargetAllocTy Kind) override {
    switch (Kind) {
    case TARGET_ALLOC_DEFAULT:
    case TARGET_ALLOC_DEVICE:
    case TARGET_ALLOC_SHARED:
    case TARGET_ALLOC_DEVICE_NON_BLOCKING:
      {
        DP("free: Phase 1 stub (ptr=%p)\n", TgtPtr);
        return OFFLOAD_FAIL;  // Stub for Phase 1
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
  DP("dataSubmit: Phase 1 stub (size=%ld)\n", Size);
  return Plugin::error(ErrorCode::UNKNOWN, "dataSubmitImpl not yet implemented (Phase 1)");
}

  /// Retrieve data from the device (device to host transfer).
  Error dataRetrieveImpl(void *HstPtr, const void *TgtPtr, int64_t Size,
                         AsyncInfoWrapperTy &AsyncInfoWrapper) override {
    DP("dataRetrieve: Phase 1 stub (size=%ld)\n", Size);
    return Plugin::error(ErrorCode::UNKNOWN, "dataRetrieveImpl not yet implemented (Phase 1)");
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

  // FIXME: Old channel members - will be removed in Phase 5
  // std::unique_ptr<DataTransferEngineWriteBase> wrChannel;
  // std::unique_ptr<DataTransferEngineReadBase> rdChannel;

  /// New offload-platform API context
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

  Error ThunderbirdKernelTy::launchImpl(GenericDeviceTy &GenericDevice, uint32_t NumThreads[3],
                 uint32_t NumBlocks[3], KernelArgsTy &KernelArgs,
                 KernelLaunchParamsTy LaunchParams,
                 AsyncInfoWrapperTy &AsyncInfoWrapper) const {
  DP("launchImpl: Phase 1 stub\n");
  return Plugin::error(ErrorCode::UNKNOWN, "launchImpl not yet implemented (Phase 1)");
  
  // Old code below - will be replaced in Phase 4
  /*
  // Cast to tbrid device so we can access our methods
  auto *TbirdDevice = static_cast<ThunderbirdDeviceTy *>(&GenericDevice);

  // Allocate a buffer on device for kernel args
 // size_t ArgsSize = KernelArgs.NumArgs * sizeof(void *);
  size_t ArgsSize = LaunchParams.Size;
  void *DeviceArgsPtr = nullptr;

  if (ArgsSize > 0) {
    DeviceArgsPtr = TbirdDevice->allocate(LaunchParams.Size, nullptr, TARGET_ALLOC_DEVICE);
    if (!DeviceArgsPtr) {
      return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                           "Failed to allocate device memory for kernel args");
    }
  }

  // Copy args to device from host
  if (ArgsSize > 0) {
    if (auto Err = TbirdDevice->dataSubmitImpl(DeviceArgsPtr, LaunchParams.Data,
                                              ArgsSize, AsyncInfoWrapper)) {
      // If the copy fails, we must clean up the memory we allocated.
      TbirdDevice->free(DeviceArgsPtr, TARGET_ALLOC_DEVICE);
      return Err;
    }
  }

  // Allocate and transfer C types array to device
  void *DeviceCTypesPtr = nullptr;
  size_t CTypesSize = 0;
  if (KernelArgs.ArgCTypes && KernelArgs.NumArgs > 0) {
    CTypesSize = KernelArgs.NumArgs * sizeof(uint8_t);
    DeviceCTypesPtr = TbirdDevice->allocate(CTypesSize, nullptr, TARGET_ALLOC_DEVICE);
    if (!DeviceCTypesPtr) {
      if (DeviceArgsPtr) {
        TbirdDevice->free(DeviceArgsPtr, TARGET_ALLOC_DEVICE);
      }
      return Plugin::error(ErrorCode::OUT_OF_RESOURCES,
                           "Failed to allocate device memory for C types array");
    }

    // Copy C types to device
    if (auto Err = TbirdDevice->dataSubmitImpl(DeviceCTypesPtr, KernelArgs.ArgCTypes,
                                                CTypesSize, AsyncInfoWrapper)) {
      TbirdDevice->free(DeviceCTypesPtr, TARGET_ALLOC_DEVICE);
      if (DeviceArgsPtr) {
        TbirdDevice->free(DeviceArgsPtr, TARGET_ALLOC_DEVICE);
      }
      return Err;
    }
  }



  // Prepare and send the launch command via the mailbox.
  std::vector<message_slot_t> launch_batch_body(1);

  // 'this->Func' should be addr of kernel
  uint64_t kernel_device_addr = reinterpret_cast<uint64_t>(this->Func);
  //uint64_t args_device_addr = reinterpret_cast<uint64_t>(DeviceArgsPtr);
  
  // C types array available at: DeviceCTypesPtr (uint64_t cast for future use)
  uint64_t ctypes_device_addr = reinterpret_cast<uint64_t>(DeviceCTypesPtr);
  uint32_t num_args = KernelArgs.NumArgs;
  // TODO: When launch command message is extended, pass ctypes_device_addr and num_args

  if (!MessageUtils::createLaunchCmd(&launch_batch_body[0],
                                   kernel_device_addr,
                                   NumBlocks[0],   // grid_x
                                   NumBlocks[1],   // grid_y
                                   NumBlocks[2],   // grid_z
                                   NumThreads[0],  // block_x
                                   NumThreads[1],  // block_y
                                   NumThreads[2],  // block_z
                                   (uint64_t) DeviceArgsPtr,
                                   LaunchParams.Size)) {           
    //TbirdDevice->free(DeviceArgsPtr, TARGET_ALLOC_DEVICE);
    return Plugin::error(ErrorCode::UNKNOWN, "Failed to create launch command");
  }

  std::vector<std::pair<int, message_slot_t>> launch_batch;
  if (!create_command_batch(launch_batch_body, 0, launch_batch)) {
//    TbirdDevice->free(DeviceArgsPtr, TARGET_ALLOC_DEVICE);
    return Plugin::error(ErrorCode::UNKNOWN, "Failed to create launch batch");
  }

  for (const auto& [slot_index, slot] : launch_batch) {
    if (!MailboxUtils::writeH2DMessage(*TbirdDevice->wrChannel, slot_index, &slot)) {
        // TODO: Handle whatever errors we need to
    }
  }

  // Wait for the kernel to finish execution.
  std::vector<std::pair<int, message_slot_t>> respSlots;
  std::vector<std::pair<int, message_slot_t>> respBatch;
  if(!waitForResponseBatch(*TbirdDevice->rdChannel, launch_batch, respBatch, respSlots)){

      return Plugin::error(ErrorCode::UNKNOWN, "Device never responded to launch command.");
  }
  if (ArgsSize > 0) {
    if (auto Err = TbirdDevice->dataRetrieveImpl(LaunchParams.Data, DeviceArgsPtr,
                                                   ArgsSize, AsyncInfoWrapper)) {
      TbirdDevice->free(DeviceArgsPtr, TARGET_ALLOC_DEVICE);
      if (DeviceCTypesPtr) {
        TbirdDevice->free(DeviceCTypesPtr, TARGET_ALLOC_DEVICE);
      }
      return Err;
    }
  }

  // Old launch code ends here
  return Plugin::success();
  */
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
