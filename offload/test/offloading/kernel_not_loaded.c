// clang-format off
// RUN: %libomptarget-compile-generic
// RUN: %not --crash %libomptarget-run-generic 2>&1 \
// RUN:   | %fcheck-generic --implicit-check-not=error
// clang-format on

// A kernel that fails to load is refused at launch, not run.
//
// An offload entry whose kernel the image does not define fails in
// get_function, so the kernel never gets a device handle. Launching it must
// report that and abort under mandatory offloading, rather than hand the
// plugin the host entry's address in place of a kernel. Every error line is
// checked: nothing may report a different failure.
//
// The entry is written by hand, in the section clang places its entries in,
// so it is part of the image's entry table like any other.

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

typedef struct {
  uint32_t Version;
  uint32_t NumArgs;
  void **ArgBasePtrs;
  void **ArgPtrs;
  int64_t *ArgSizes;
  int64_t *ArgTypes;
  void **ArgNames;
  void **ArgMappers;
  uint64_t Tripcount;
  uint64_t Flags;
  uint32_t NumTeams[3];
  uint32_t ThreadLimit[3];
  uint32_t DynCGroupMem;
} KernelArgsTy;

int __tgt_target_kernel(void *Loc, int64_t DeviceId, int32_t NumTeams,
                        int32_t ThreadLimit, void *HostPtr, KernelArgsTy *Args);

// ---------------------------------------------------------------------------

// Stands in for the region ID clang would emit for a real kernel.
static char missing_region;

__attribute__((weak, section("llvm_offload_entries"), aligned(8)))
const __tgt_offload_entry __offloading_entry[] = {{
    0ULL,                              // Reserved
    1,                                 // Version
    1,                                 // Kind (OpenMP)
    0,                                 // Flags
    &missing_region,                   // Address
    "__omp_offloading_does_not_exist", // SymbolName
    0,                                 // Size (a kernel)
    0ULL,                              // Data
    NULL                               // AuxAddr
}};

int main() {
  // An ordinary kernel loads and runs.
  int x = 0;
#pragma omp target map(tofrom : x)
  x = 1;
  fprintf(stderr, "ordinary kernel: x = %d\n", x);

  KernelArgsTy Args = {0};
  Args.Version = 3;
  Args.NumTeams[0] = 1;
  Args.ThreadLimit[0] = 1;
  __tgt_target_kernel(NULL, omp_get_default_device(), 1, 1, &missing_region,
                      &Args);
  fprintf(stderr, "the missing kernel was launched\n");
  return 0;
}

// CHECK: "PluginInterface" error: Failure to init kernel: {{.*}}
// CHECK: omptarget error: Failed to load kernel __omp_offloading_does_not_exist
// CHECK: ordinary kernel: x = 1
// CHECK: omptarget error: Kernel __omp_offloading_does_not_exist was not loaded on device {{[0-9]+}}, abort target.
// CHECK: omptarget error: Consult https://openmp.llvm.org/design/Runtimes.html for debugging options.
// CHECK: omptarget error: Source location information not present. Compile with -g or -gline-tables-only.
// CHECK: omptarget fatal error 1: failure of target construct while offloading is mandatory
// CHECK-NOT: the missing kernel was launched
// `not --crash` names the signal: the abort above, not a fault in the launch.
// CHECK: error: Aborted
