// Kernel-argument C types ride on the offload entry for Inspire targets.
//
// The Thunderbird device dispatches kernels through libffi and needs each
// parameter's C type at run time. The array travels on EntryTy::AuxAddr, the
// spare pointer every offload entry already carries, rather than in the
// kernel-arguments struct that the AMDGPU, CUDA and host plugins also read.
//
// Both halves live here on purpose. The entry is the only thing carrying type
// information to the device, so a test that checked only the non-Inspire half
// would stay green while Thunderbird silently lost its types and
// mis-marshalled every argument -- a runtime fault a compiler test would
// never see. The negative half is what proves the positive half is a real
// vendor gate rather than something every target gets.
//
// The function below is deliberately not called "kernel": the entry symbol
// embeds the enclosing function name, and a bare "kernel" in a FileCheck
// pattern is easy to confuse with the function attribute of the same name.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// RUN: %clang_cc1 -fopenmp -x c -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=TBIRD %s

// RUN: %clang_cc1 -fopenmp -x c -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=x86_64-unknown-linux-gnu -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=OTHER %s
// RUN: %clang_cc1 -fopenmp -x c -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=x86_64-unknown-linux-gnu -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=OTHER-NEG %s

// The types themselves: int, double*, double*, double -> INT32, POINTER,
// POINTER, DOUBLE. Spelled out because the values are a wire format the device
// decodes into ffi_type, and signedness governs RISC-V argument extension.
// TBIRD: @.offload_ctypes = private unnamed_addr constant [4 x i8] c"\05\0B\0B\0A"

// The entry's ninth field (AuxAddr) points at that array.
// TBIRD: @.offloading.entry.{{.*}} = {{.*}}%struct.__tgt_offload_entry { {{.*}}ptr @.offload_ctypes }

// Everyone else gets upstream's shape: the AuxAddr slot stays null.
// OTHER: @.offloading.entry.{{.*}} = {{.*}}%struct.__tgt_offload_entry { {{.*}}ptr null }

// ...and the array is not emitted for them at all. This needs its own
// FileCheck run: the global precedes the entry in the module, so a -NOT beside
// the check above would scan past it.
// OTHER-NEG-NOT: @.offload_ctypes

void scale(const double *restrict in, double *restrict out, int n, double s) {
#pragma omp target teams distribute parallel for map(to : in[0 : n])           \
    map(from : out[0 : n])
  for (int i = 0; i < n; ++i)
    out[i] = in[i] * s + 1.0;
}
