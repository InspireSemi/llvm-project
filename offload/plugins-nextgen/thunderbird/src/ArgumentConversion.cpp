//===-- ArgumentConversion.cpp - OpenMP to Thunderbird arg marshalling -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ArgumentConversion.h"

#include <cstring>

namespace llvm {
namespace omp {
namespace target {
namespace plugin {

using namespace error;

Error convertPointerArgument(uint32_t ArgIdx, tbird_arg_t &OutArg,
                             const ArgConversionContext &Ctx) {
  if (!Ctx.LaunchParams.Ptrs) {
    return Plugin::error(ErrorCode::UNKNOWN, "LaunchParams.Ptrs is NULL");
  }

  // LaunchParams.Ptrs is offset by KLEOffset (KernelLaunchEnvironment at [0])
  uint32_t PtrIndex = ArgIdx + Ctx.KLEOffset;
  uint32_t NumPtrs = Ctx.LaunchParams.Size / sizeof(void*);
  if (PtrIndex >= NumPtrs) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "Pointer index %u >= NumPtrs %u", PtrIndex, NumPtrs);
  }

  // Guard against a null slot in LaunchParams.Ptrs — the indirection below
  // would otherwise segfault. A pointer-typed argument with no entry is an
  // error; the caller set up LaunchParams incorrectly.
  if (!Ctx.LaunchParams.Ptrs[PtrIndex]) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "LaunchParams.Ptrs[%u] is NULL", PtrIndex);
  }

  void *DevicePtr = *(void**)Ctx.LaunchParams.Ptrs[PtrIndex];

  // Verify it's in buffer registry (supports interior pointers)
  auto [buf, ofs] = Ctx.Pool.lookup(DevicePtr);
  if (!buf) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "Device pointer %p not in buffer registry", DevicePtr);
  }

  OutArg.value.ptr = DevicePtr;
  DP("    PTR: %p (from *LaunchParams.Ptrs[%u])\n", DevicePtr, PtrIndex);
  return Plugin::success();
}

Error convertScalarArgument(uint32_t ArgIdx, tbird_arg_t &OutArg,
                            const ArgConversionContext &Ctx) {
  if (!Ctx.LaunchParams.Ptrs)
    return Plugin::error(ErrorCode::UNKNOWN, "LaunchParams.Ptrs is NULL");
  uint32_t PtrIndex = ArgIdx + Ctx.KLEOffset;
  if (PtrIndex >= Ctx.LaunchParams.Size / sizeof(void *) ||
      !Ctx.LaunchParams.Ptrs[PtrIndex])
    return Plugin::error(ErrorCode::UNKNOWN,
                         "no launch cell for scalar argument %u", ArgIdx);

  // The cell holds the value itself: clang passes a by-copy scalar widened to
  // the kernel's i64 parameter, and libomptarget resolved it into this cell.
  // All eight bytes are passed; the kernel narrows them.
  uint64_t Bits;
  std::memcpy(&Bits, Ctx.LaunchParams.Ptrs[PtrIndex], sizeof(Bits));
  std::memset(OutArg.value.scalar_bytes, 0, sizeof(OutArg.value.scalar_bytes));
  std::memcpy(OutArg.value.scalar_bytes, &Bits, sizeof(Bits));
  DP("    INT64 from Ptrs[%u]: 0x%lx\n", PtrIndex, (unsigned long)Bits);
  return Plugin::success();
}

Expected<uint32_t> convertKernelArguments(tbird_arg_t ArgsOut[TBIRD_MAX_ARGS],
                                          const ArgConversionContext &Ctx) {
  if (!Ctx.CTypes || Ctx.NumCTypes == 0)
    return Plugin::error(ErrorCode::INVALID_BINARY,
                         "kernel has no parameter types (<kernel>_ctypes)");

  // Code 0 is dyn_ptr, which the device receives as thread_id 0
  // (prependThreadId); the remaining codes pair with the launch cells.
  if (Ctx.CTypes[0] != KernelParamPointer)
    return Plugin::error(ErrorCode::INVALID_BINARY,
                         "kernel parameter 0 has type code %u, not the "
                         "dyn_ptr pointer (%u)",
                         Ctx.CTypes[0], KernelParamPointer);
  uint32_t NumParams = Ctx.NumCTypes - 1;
  uint32_t NumPassed = Ctx.KernelArgs.NumArgs - Ctx.KLEOffset;
  if (NumParams != NumPassed)
    return Plugin::error(ErrorCode::INVALID_ARGUMENT,
                         "kernel takes %u arguments but the launch passes %u",
                         NumParams, NumPassed);
  if (NumParams >= TBIRD_MAX_ARGS)
    return Plugin::error(ErrorCode::INVALID_ARGUMENT,
                         "too many kernel arguments: %u (max %d)", NumParams,
                         TBIRD_MAX_ARGS - 1);

  memset(ArgsOut, 0, sizeof(tbird_arg_t) * TBIRD_MAX_ARGS);
  for (uint32_t i = 0; i < NumParams; i++) {
    uint8_t Code = Ctx.CTypes[i + 1];
    DP("  arg[%u]: code=%u\n", i, Code);
    switch (Code) {
    case KernelParamPointer:
      ArgsOut[i].type = TBIRD_TYPE_PTR;
      if (Error Err = convertPointerArgument(i, ArgsOut[i], Ctx))
        return std::move(Err);
      break;
    case KernelParamInt64:
      ArgsOut[i].type = TBIRD_TYPE_INT64;
      if (Error Err = convertScalarArgument(i, ArgsOut[i], Ctx))
        return std::move(Err);
      break;
    default:
      return Plugin::error(ErrorCode::INVALID_BINARY,
                           "kernel parameter %u has unknown type code %u",
                           i + 1, Code);
    }
  }
  return NumParams;
}

void prependThreadId(tbird_arg_t Args[TBIRD_MAX_ARGS], uint32_t &ArgCount) {
  DP("Prepending thread_id=0 for dyn_ptr\n");

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

} // namespace plugin
} // namespace target
} // namespace omp
} // namespace llvm
