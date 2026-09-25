//===-- ArgumentConversion.h - OpenMP to Thunderbird arg marshalling -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Converts OpenMP kernel arguments (device pointers and by-copy scalars) into
// the tbird_arg_t format expected by the Thunderbird device FFI layer.
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

/// The parameter type codes in <kernel>_ctypes, as clang emits them
/// (CGOpenMPRuntime.cpp, emitThunderbirdKernelArgTypes). A kernel parameter is
/// a pointer or a by-copy scalar widened to i64; nothing else occurs.
enum : uint8_t { KernelParamInt64 = 7, KernelParamPointer = 11 };

/// Context passed to argument conversion helpers.
/// Uses MemoryPool reference (not ThunderbirdDeviceTy) to keep this
/// module independent of the plugin interface classes.
struct ArgConversionContext {
  MemoryPool &Pool;
  KernelArgsTy &KernelArgs;
  KernelLaunchParamsTy &LaunchParams;
  uint32_t KLEOffset; // KernelLaunchEnvironment offset in LaunchParams.Ptrs
  /// The kernel's parameter type codes, one per parameter in parameter order,
  /// read from the <kernel>_ctypes global the compiler emits in the device
  /// image. Code 0 is the leading dyn_ptr parameter; code k (k >= 1) describes
  /// the argument whose value is in LaunchParams.Ptrs[k - 1 + KLEOffset].
  const uint8_t *CTypes;
  uint32_t NumCTypes;
};

/// Convert pointer argument ArgIdx (0-based, dyn_ptr excluded) to tbird format.
Error convertPointerArgument(uint32_t ArgIdx, tbird_arg_t &OutArg,
                             const ArgConversionContext &Ctx);

/// Convert scalar argument ArgIdx (0-based, dyn_ptr excluded) to an INT64
/// tbird argument: the argument's cell holds its value, widened to i64.
Error convertScalarArgument(uint32_t ArgIdx, tbird_arg_t &OutArg,
                            const ArgConversionContext &Ctx);

/// Convert all OpenMP kernel arguments to tbird_arg_t format, typed by the
/// kernel's parameter codes. Returns the number of converted arguments
/// (dyn_ptr excluded; see prependThreadId).
Expected<uint32_t> convertKernelArguments(tbird_arg_t ArgsOut[TBIRD_MAX_ARGS],
                                          const ArgConversionContext &Ctx);

/// Prepend the leading dyn_ptr parameter, passed as thread_id 0.
void prependThreadId(tbird_arg_t Args[TBIRD_MAX_ARGS], uint32_t &ArgCount);

} // namespace plugin
} // namespace target
} // namespace omp
} // namespace llvm

#endif // THUNDERBIRD_ARGUMENTCONVERSION_H
