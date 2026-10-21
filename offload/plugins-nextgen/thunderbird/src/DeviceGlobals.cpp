//===-- DeviceGlobals.cpp - Thunderbird image globals on the device -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DeviceGlobals.h"

namespace thunderbird {

std::string deviceSymbolAddress(uint64_t LoadBase, const char *Name,
                                const ImageSymbol &Sym, uint64_t ExpectedSize,
                                uint64_t *Addr) {
  const std::string What = std::string("'") + (Name ? Name : "") + "'";
  if (LoadBase == 0)
    return "the image holding " + What + " is not loaded on the device";
  if (!Sym.Defined)
    return What + " is not defined in the image";
  if (ExpectedSize != 0 && ExpectedSize != Sym.Size)
    return What + " is " + std::to_string(Sym.Size) +
           " bytes in the image, expected " + std::to_string(ExpectedSize);
  *Addr = LoadBase + Sym.Value;
  return "";
}

} // namespace thunderbird
