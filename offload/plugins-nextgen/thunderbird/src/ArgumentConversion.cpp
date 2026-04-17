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

tbird_arg_type_t convert_omp_ctype_to_tbird(uint8_t omp_ctype) {
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

std::pair<int64_t, bool> getArgMapType(uint32_t ArgIdx,
                                       const ArgConversionContext &Ctx) {
  int64_t MapType = 0;
  bool HasMapType = false;

  if (Ctx.KernelArgs.ArgTypes && ArgIdx < Ctx.KernelArgs.NumArgs) {
    MapType = Ctx.KernelArgs.ArgTypes[ArgIdx];
    HasMapType = true;
  }

  return {MapType, HasMapType};
}

size_t getScalarSize(tbird_arg_type_t Type) {
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

Error convertPointerArgument(uint32_t OmpIdx, tbird_arg_t &OutArg,
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
  auto [buf, ofs] = Ctx.Pool.lookup(DevicePtr);
  if (!buf) {
    return Plugin::error(ErrorCode::UNKNOWN,
                        "Device pointer %p not in buffer registry", DevicePtr);
  }

  OutArg.value.ptr = DevicePtr;
  DP("    PTR: %p (from *LaunchParams.Ptrs[%u])\n", DevicePtr, PtrIndex);
  return Plugin::success();
}

Error convertScalarArgument(uint32_t OmpIdx, tbird_arg_t &OutArg,
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
      auto [foundBuf, foundOfs] = Ctx.Pool.lookup(PotentialDevicePtr);
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
    auto [scBuf, scOfs] = Ctx.Pool.lookup(Ctx.KernelArgs.ArgPtrs[OmpIdx]);
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

Expected<uint32_t> convertKernelArguments(tbird_arg_t ArgsOut[TBIRD_MAX_ARGS],
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

void prependThreadId(tbird_arg_t Args[TBIRD_MAX_ARGS], uint32_t &ArgCount) {
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

} // namespace plugin
} // namespace target
} // namespace omp
} // namespace llvm
