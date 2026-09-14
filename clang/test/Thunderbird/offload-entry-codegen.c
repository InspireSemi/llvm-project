// Paired test for the offload-entry guards (audit S13c and S02).
//
// Both halves live here on purpose. Each guard was written for Thunderbird but
// originally keyed on !isGPU(), which is also true for upstream's own non-GPU
// offload targets and for any host compile. Narrowing them to the Inspire
// vendor fixed that -- and a test that checked only the non-Inspire half would
// stay green if the Inspire half stopped working, which is the failure this
// file exists to prevent.
//
// S13c: createOffloadEntry marks the entry "kernel" and forces external
//       linkage, because a Thunderbird kernel is reachable only by address
//       from the offload entry table and IPSCCP would otherwise wipe its body.
// S02:  the outlined function must NOT be alwaysinline on the Thunderbird
//       device: it runs on its own pthread and has to stay callable through
//       __kmpc_fork_call.
//
// The function below is deliberately not called "kernel": the attribute and a
// symbol containing that word are easy to confuse in a FileCheck pattern, and
// an earlier draft of this test did exactly that.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// RUN: %clang -fopenmp --offload-arch=thunderbird --offload-device-only \
// RUN:     -O2 -S -emit-llvm -o - %s | FileCheck --check-prefix=TBIRD %s
// RUN: %clang -fopenmp --offload-arch=thunderbird --offload-device-only \
// RUN:     -O2 -S -emit-llvm -o - %s | FileCheck --check-prefix=TBIRD-NEG %s

// RUN: %clang -fopenmp -fopenmp-targets=x86_64-unknown-linux-gnu \
// RUN:     --offload-device-only -O2 -S -emit-llvm -o - %s \
// RUN:   | FileCheck --check-prefix=OTHER-NEG %s

// The Inspire device entry keeps external linkage and carries the attribute.
// TBIRD: define protected void @__omp_offloading_
// TBIRD: {{^attributes .*"kernel"}}

// ...and its outlined function stays callable rather than being inlined away.
// TBIRD-NEG-NOT: alwaysinline

// Everyone else keeps upstream's shape: no "kernel" attribute at all.
// OTHER-NEG-NOT: {{^attributes .*"kernel"}}

void scale(const double *restrict in, double *restrict out, int n, double s) {
#pragma omp target teams distribute parallel for map(to : in[0 : n])           \
    map(from : out[0 : n])
  for (int i = 0; i < n; ++i)
    out[i] = in[i] * s + 1.0;
}
