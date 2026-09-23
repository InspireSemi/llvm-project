//===-- Thunderbird/src/Topology.cpp - Co-location predicate -*- C++ -*----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Topology.h"

namespace llvm {
namespace omp {
namespace target {
namespace plugin {
namespace thunderbird {
namespace topology {

std::string getDevicePathPrefix(const std::string &path) {
  auto pos = path.rfind('-');
  if (pos == std::string::npos)
    return path;
  return path.substr(0, pos);
}

bool arePathsCoLocated(const std::string &p0, const std::string &p1) {
  return getDevicePathPrefix(p0) == getDevicePathPrefix(p1);
}

} // namespace topology
} // namespace thunderbird
} // namespace plugin
} // namespace target
} // namespace omp
} // namespace llvm
