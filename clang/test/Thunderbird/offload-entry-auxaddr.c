// Paired test for C1: kernel-argument C types ride on the offload entry.
//
// The Thunderbird device dispatches kernels through libffi and needs each
// parameter's C type at run time. That array used to travel as a field spliced
// into the middle of KernelArgsTy -- a struct the AMDGPU, CUDA and host plugins
// and five libomptarget files all read, and whose layout is versioned but was
// not re-versioned. C1 moved it onto EntryTy::AuxAddr, the spare pointer
// upstream already provides on every offload entry, and deleted the struct
// field.
//
// Both halves live here on purpose. After C1 the entry is the ONLY thing
// carrying type information to the device, so a test that checked only the
// non-Inspire half would stay green while Thunderbird silently lost its types
// and mis-marshalled every argument -- a runtime fault a compiler test would
// never see. The negative half is what proves the positive half is a real
// vendor gate rather than something everyone gets.
//
// The function below is deliberately not called "kernel": the entry symbol
// embeds the enclosing function name, and a bare "kernel" in a FileCheck
// pattern is easy to confuse with the "kernel" attribute tested next door in
// offload-entry-codegen.c.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// RUN: %clang -fopenmp --offload-arch=thunderbird -O2 -S -emit-llvm -o - %s \
// RUN:   | FileCheck --check-prefix=TBIRD %s

// RUN: %clang -fopenmp -fopenmp-targets=x86_64-unknown-linux-gnu \
// RUN:     -O2 -S -emit-llvm -o - %s | FileCheck --check-prefix=OTHER %s
// RUN: %clang -fopenmp -fopenmp-targets=x86_64-unknown-linux-gnu \
// RUN:     -O2 -S -emit-llvm -o - %s | FileCheck --check-prefix=OTHER-NEG %s

// The types themselves: int, double*, double*, double -> INT32, POINTER,
// POINTER, DOUBLE. Spelled out because the values are a wire format the device
// decodes into ffi_type, and signedness governs RISC-V argument extension.
// TBIRD: @.offload_ctypes = private unnamed_addr constant [4 x i8] c"\05\0B\0B\0A"

// The entry's ninth field (AuxAddr) points at that array.
// TBIRD: @.offloading.entry.{{.*}} = {{.*}}%struct.__tgt_offload_entry { {{.*}}ptr @.offload_ctypes }

// Everyone else gets upstream's shape: the AuxAddr slot stays null.
// OTHER: @.offloading.entry.{{.*}} = {{.*}}%struct.__tgt_offload_entry { {{.*}}ptr null }

// ...and the array is not even emitted for them. Nothing references it once
// the kernel-arguments field is gone, so it is dead and dropped -- which is
// what returns KernelArgsTy to upstream's layout for the plugins that share
// it. This needs its own FileCheck run: the global precedes the entry in the
// module, so a -NOT beside the check above would scan past it.
// OTHER-NEG-NOT: @.offload_ctypes

void scale(const double *restrict in, double *restrict out, int n, double s) {
#pragma omp target teams distribute parallel for map(to : in[0 : n])           \
    map(from : out[0 : n])
  for (int i = 0; i < n; ++i)
    out[i] = in[i] * s + 1.0;
}
