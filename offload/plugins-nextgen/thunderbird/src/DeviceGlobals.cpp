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

std::string copyInChunks(uint64_t DevAddr, size_t Size, size_t ChunkSize,
                         const DeviceChunkCopy &CopyChunk) {
  if (ChunkSize == 0)
    return "device copy with a chunk size of 0";
  if (Size != 0 && DevAddr + Size < DevAddr)
    return "device copy range wraps the address space";
  for (size_t Offset = 0; Offset < Size; Offset += ChunkSize) {
    size_t Len = Size - Offset < ChunkSize ? Size - Offset : ChunkSize;
    std::string Why = CopyChunk(DevAddr + Offset, Offset, Len);
    if (!Why.empty())
      return Why;
  }
  return "";
}

} // namespace thunderbird
