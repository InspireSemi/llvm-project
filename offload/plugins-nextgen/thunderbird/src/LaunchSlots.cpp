//===-- LaunchSlots.cpp - Thunderbird kernel launch arguments ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LaunchSlots.h"

#include <cstring>

namespace thunderbird {

std::string launchUnsupportedReason(bool IsCUDA, bool IsPtrArgs,
                                    uint32_t DynCGroupMem,
                                    uint32_t DynBlockMemSize) {
  if (IsCUDA)
    return "CUDA-style launches are not supported: their argument values have "
           "no stated size, so they cannot be copied to the device";
  if (IsPtrArgs)
    return "pointer-array launches are not supported: their argument values "
           "have no stated size, so they cannot be copied to the device";
  if (DynCGroupMem || DynBlockMemSize)
    return "dynamic group memory (" + std::to_string(DynCGroupMem) + " bytes, " +
           std::to_string(DynBlockMemSize) +
           " bytes per block) is not supported on Thunderbird";
  return "";
}

std::string buildLaunchSlots(uint32_t NumArgs, void *const *Args,
                             uint64_t *Slots, uint32_t MaxSlots) {
  if (NumArgs == 0)
    return "kernel launch carries no arguments; an OpenMP kernel always has "
           "dyn_ptr";
  if (NumArgs > MaxSlots)
    return "too many kernel arguments: " + std::to_string(NumArgs) + " (max " +
           std::to_string(MaxSlots) + ")";
  if (!Args)
    return "kernel launch has no argument array";
  for (uint32_t I = 0; I < NumArgs; ++I)
    std::memcpy(&Slots[I], Args[I], sizeof(uint64_t));
  return "";
}

} // namespace thunderbird
