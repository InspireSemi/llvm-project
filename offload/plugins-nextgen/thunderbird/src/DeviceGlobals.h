//===-- DeviceGlobals.h - Thunderbird image globals on the device -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Where a kernel image's symbols are on the device, and how the host reaches
// the memory they name.
//
// A Thunderbird kernel image is an ELF shared object that the device loads
// once (tbird_load_image), and every kernel of the image runs in that loaded
// copy. Its global variables therefore live in the device's copy, at the load
// base the device reports plus the symbol's st_value -- the dynamic loader's
// own rule for a shared object. That memory is not in a pool slab, so the host
// cannot write it directly; the device copies to and from it through a
// staging range of shared memory, one request per chunk (tbird_device_copy).
//
// Plain values in and out, so this is testable without the plugin.
//
//===----------------------------------------------------------------------===//

#ifndef THUNDERBIRD_DEVICEGLOBALS_H
#define THUNDERBIRD_DEVICEGLOBALS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace thunderbird {

/// A symbol as the image's own dynamic symbol table describes it.
struct ImageSymbol {
  bool Defined;   ///< false for an undefined (imported) symbol
  uint64_t Value; ///< st_value
  uint64_t Size;  ///< st_size
};

/// The device address of symbol Name of an image loaded at LoadBase, in *Addr.
/// Returns "" on success, or why not: the image is not loaded (LoadBase 0),
/// the symbol is undefined in the image (an import from another object is not
/// the image's own), or ExpectedSize is non-zero and differs from its size.
std::string deviceSymbolAddress(uint64_t LoadBase, const char *Name,
                                const ImageSymbol &Sym, uint64_t ExpectedSize,
                                uint64_t *Addr);

/// One device request: copy Len bytes at device address DevAddr, which is
/// Offset bytes into the whole copy. Returns "" or why not.
using DeviceChunkCopy =
    std::function<std::string(uint64_t DevAddr, size_t Offset, size_t Len)>;

/// Copy Size bytes starting at device address DevAddr in chunks of at most
/// ChunkSize, calling CopyChunk once per chunk in address order. Returns "" on
/// success, or the first failing chunk's reason; later chunks are not tried.
/// Size 0 copies nothing; ChunkSize 0, or a range that wraps the address
/// space, is refused.
std::string copyInChunks(uint64_t DevAddr, size_t Size, size_t ChunkSize,
                         const DeviceChunkCopy &CopyChunk);

} // namespace thunderbird

#endif // THUNDERBIRD_DEVICEGLOBALS_H
