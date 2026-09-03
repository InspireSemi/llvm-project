//===-- ArgumentConversion.h - OpenMP to Thunderbird arg marshalling -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Converts OpenMP kernel arguments (map types, host pointers, scalars) into the
// tbird_arg_t format expected by the Thunderbird device FFI layer.
//
//===----------------------------------------------------------------------===//

#ifndef THUNDERBIRD_ARGUMENTCONVERSION_H
#define THUNDERBIRD_ARGUMENTCONVERSION_H

#include "MemoryPool.h"

#include "PluginInterface.h"
#include "Shared/Debug.h"
#include "omptarget.h"

#include "tbird_offload_api.h"
#include "internal/tbird_types_internal.h"

#include <cstdint>
#include <utility>

namespace llvm {
namespace omp {
namespace target {
namespace plugin {

/// Convert OpenMP C type enum to tbird_arg_type_t.
tbird_arg_type_t convert_omp_ctype_to_tbird(uint8_t omp_ctype);

/// Context passed to argument conversion helpers.
/// Uses MemoryPool reference (not ThunderbirdDeviceTy) to keep this
/// module independent of the plugin interface classes.
struct ArgConversionContext {
  MemoryPool &Pool;
  KernelArgsTy &KernelArgs;
  KernelLaunchParamsTy &LaunchParams;
  uint32_t KLEOffset; // KernelLaunchEnvironment offset in LaunchParams.Ptrs
};

/// Get OpenMP map type flags for argument (with safe bounds checking).
std::pair<int64_t, bool> getArgMapType(uint32_t ArgIdx,
                                       const ArgConversionContext &Ctx);

/// Get scalar type size in bytes.
size_t getScalarSize(tbird_arg_type_t Type);

/// Convert pointer argument from OpenMP to tbird format.
Error convertPointerArgument(uint32_t OmpIdx, tbird_arg_t &OutArg,
                             const ArgConversionContext &Ctx);

/// Convert scalar argument from OpenMP to tbird format.
/// Handles by-value (literal), by-reference, and promotion to PTR.
Error convertScalarArgument(uint32_t OmpIdx, tbird_arg_t &OutArg,
                            int64_t MapType, bool HasMapType,
                            const ArgConversionContext &Ctx);

/// Convert all OpenMP kernel arguments to tbird_arg_t format.
/// Returns the number of converted arguments (excluding VOID types).
Expected<uint32_t> convertKernelArguments(tbird_arg_t ArgsOut[TBIRD_MAX_ARGS],
                                          const ArgConversionContext &Ctx);

/// Prepend thread_id as first argument for GENERIC mode.
void prependThreadId(tbird_arg_t Args[TBIRD_MAX_ARGS], uint32_t &ArgCount);

} // namespace plugin
} // namespace target
} // namespace omp
} // namespace llvm

#endif // THUNDERBIRD_ARGUMENTCONVERSION_H
