// clang-format off
// RUN: %libomptarget-compile-generic
// RUN: %not --crash %libomptarget-run-generic 2>&1 \
// RUN:   | %fcheck-generic --implicit-check-not=error
//
// RUN: %libomptarget-compile-generic -DPROBE_FIRST
// RUN: %not --crash %libomptarget-run-generic 2>&1 \
// RUN:   | %fcheck-generic --implicit-check-not=error --check-prefixes=CHECK,PROBE
// clang-format on

// An image whose global fails to load is not used, and stays unusable.
//
// An offload entry for a global the image does not define fails in get_global,
// so the global has no device address; its entry would otherwise keep the host
// global's address and every transfer or mapping of it would go to host
// memory. Loading the image fails instead, and every later request for the
// device reports the same failure. Every error line is checked: nothing may
// report a different failure.
//
// The entry is written by hand, in the section clang places its entries in,
// so it is part of the image's entry table like any other. PROBE_FIRST first
// asks for the device through interop, which reports the failure without
// aborting, so that the target region is a second request: it must fail the
// same way, not find the device half loaded.

#include <omp.h>
#include <stdint.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Various definitions copied from OpenMP RTL

typedef struct {
  uint64_t Reserved;
  uint16_t Version;
  uint16_t Kind;
  uint32_t Flags;
  void *Address;
  char *SymbolName;
  uint64_t Size;
  uint64_t Data;
  void *AuxAddr;
} __tgt_offload_entry;

// ---------------------------------------------------------------------------

// A host global the image has no counterpart for.
int missing_global = 1;

__attribute__((weak, section("llvm_offload_entries"), aligned(8)))
const __tgt_offload_entry __offloading_entry[] = {{
    0ULL,                                // Reserved
    1,                                   // Version
    1,                                   // Kind (OpenMP)
    0,                                   // Flags
    &missing_global,                     // Address
    "__omp_offload_test_missing_global", // SymbolName
    sizeof(missing_global),              // Size (a global)
    0ULL,                                // Data
    NULL                                 // AuxAddr
}};

int main() {
#ifdef PROBE_FIRST
  omp_interop_t Obj = omp_interop_none;
#pragma omp interop init(target : Obj) device(omp_get_default_device())
  fprintf(stderr, "interop: %s\n",
          Obj == omp_interop_none ? "none" : "created");
#endif

  int x = 0;
#pragma omp target map(tofrom : x)
  x = 1;
  fprintf(stderr, "the target region ran: x = %d\n", x);
  return 0;
}

// CHECK: omptarget error: Failed to load symbol __omp_offload_test_missing_global
// PROBE: interop: none
// CHECK: omptarget fatal error {{[0-9]+}}: "the plugin backend is in an invalid or unsupported state" failed to load images on device '{{[0-9]+}}'
// CHECK-NOT: the target region ran
// `not --crash` names the signal: the abort above, not a fault.
// CHECK: error: Aborted
