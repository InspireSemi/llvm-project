//===-- Thunderbird/src/Topology.h - Co-location predicate -*- C++ -*------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Pure functions describing the Thunderbird-stack mailbox topology. The
// load-bearing question for the d2d-exchange optimization is "are these
// two mailboxes co-located on the same physical Thunderbird device?" —
// co-located mailboxes share BAR2/ivshmem backing and can be served by
// an in-process memcpy in the host plugin; cross-physical mailboxes
// can't, and libomptarget falls back to its host-bounce d2d path.
//
// Implementations are kept here (string manipulation only, no plugin
// state) so the predicate is unit-testable in isolation: feed in
// hardcoded paths and compare against expected outcomes. The runtime
// glue (looking paths up by DeviceId from the discovered list) lives
// in rtl.cpp.
//
//===----------------------------------------------------------------------===//

#ifndef OFFLOAD_PLUGINS_NEXTGEN_THUNDERBIRD_TOPOLOGY_H
#define OFFLOAD_PLUGINS_NEXTGEN_THUNDERBIRD_TOPOLOGY_H

#include <string>

namespace llvm {
namespace omp {
namespace target {
namespace plugin {
namespace thunderbird {
namespace topology {

/// Strip the trailing "-<K>" mailbox-index suffix from a
/// /dev/tbird<PCI>-<K> path, leaving the per-physical-device prefix
/// (canonically the PCI BDF). Two mailbox paths share a prefix iff
/// they refer to mailboxes on the SAME physical Thunderbird.
///
/// Examples:
///   "/dev/tbird0018-0" -> "/dev/tbird0018"
///   "/dev/tbird0018-1" -> "/dev/tbird0018"
///   "/dev/tbird0019-0" -> "/dev/tbird0019"   (different physical device)
///   "/dev/tbird0018"   -> "/dev/tbird0018"   (no suffix; degenerate path)
///   ""                 -> ""
std::string getDevicePathPrefix(const std::string &path);

/// True iff two mailbox paths refer to the same physical Thunderbird
/// device — i.e., share the same prefix per getDevicePathPrefix.
/// This is the canonical co-location predicate used by the plugin's
/// isDataExchangable / dataExchangeImpl: when true, the two mailboxes
/// share BAR2/ivshmem backing and an in-process memcpy in the plugin
/// process suffices for d2d data movement; when false, libomptarget
/// host-bounces.
///
/// Behavior on trivially-degenerate input: two empty strings compare
/// equal (return true). One empty + one non-empty returns false. The
/// caller (rtl.cpp's int-id-based wrapper) only feeds in paths it
/// discovered via the host driver's char-device listing, so the
/// degenerate cases are not expected in practice — but the predicate
/// is well-defined for them.
bool arePathsCoLocated(const std::string &p0, const std::string &p1);

} // namespace topology
} // namespace thunderbird
} // namespace plugin
} // namespace target
} // namespace omp
} // namespace llvm

#endif // OFFLOAD_PLUGINS_NEXTGEN_THUNDERBIRD_TOPOLOGY_H
