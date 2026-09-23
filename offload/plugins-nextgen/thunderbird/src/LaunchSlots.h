//===-- LaunchSlots.h - Thunderbird kernel launch arguments --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// What a Thunderbird kernel launch carries to the device, and what it refuses.
//
// A Thunderbird kernel is void K(void **args), the OpenMP prototype for non-GPU
// targets: args[i] points at the i-th argument's value. libomptarget's
// prepareArgs makes every OpenMP argument one pointer-sized value -- a device
// address, or a by-value scalar in pointer-sized storage -- with dyn_ptr in the
// last slot. The launch therefore sends those values unchanged as 8-byte slots
// (tbird_launch_kernel_slots); the device rebuilds the array of pointers.
//
// Plain values in and out, so this is testable without the plugin.
//
//===----------------------------------------------------------------------===//

#ifndef THUNDERBIRD_LAUNCHSLOTS_H
#define THUNDERBIRD_LAUNCHSLOTS_H

#include <cstdint>
#include <string>

namespace thunderbird {

/// Why a launch with these properties cannot run on Thunderbird, or "" if it can.
///
/// - IsCUDA, IsPtrArgs: the caller lays out its own argument values, whose
///   sizes the launch is not told, so they cannot be copied to the device.
/// - DynCGroupMem, DynBlockMemSize: the device has no per-team dynamic memory,
///   and nothing on it reads a kernel launch environment.
std::string launchUnsupportedReason(bool IsCUDA, bool IsPtrArgs,
                                    uint32_t DynCGroupMem,
                                    uint32_t DynBlockMemSize);

/// Copy the NumArgs argument values Args[i] points at into Slots[0..NumArgs).
/// Returns "" on success, or why not: NumArgs is 0 (an OpenMP kernel always
/// has dyn_ptr), exceeds MaxSlots, or Args is null.
std::string buildLaunchSlots(uint32_t NumArgs, void *const *Args,
                             uint64_t *Slots, uint32_t MaxSlots);

} // namespace thunderbird

#endif // THUNDERBIRD_LAUNCHSLOTS_H
