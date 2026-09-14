// Paired test for the RISC-V driver guards (audit S04a and S05a).
//
// Both guards originally keyed on Triple.isRISCV(), which also claims
// riscv64-unknown-linux-gnu -- a legitimate third-party OpenMP offload triple
// that wants none of this. They are now gated on the Inspire vendor.
//
// S04a: getRISCVTargetFeatures overrode -march to rv64gc whenever -fopenmp and
//       --offload-arch were both present. Ungated, that rewrote the
//       architecture of a RISC-V *host* compile that merely asked for
//       Thunderbird offload.
// S05a: the device compile disables PIE and forces global-dynamic TLS, because
//       the device image is loaded with dlopen and local-exec TPREL
//       relocations are incompatible with -shared.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// The Inspire device job: rv64gc features and the TLS model.
// RUN: %clang -fopenmp --offload-arch=thunderbird -### -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=TBIRD %s
// Both must be on the same job line -- it is the device compilation that gets
// the TLS model, and asserting them separately would pass if the model landed
// on the host job instead.
// TBIRD: "-triple" "riscv64-inspire-linux-gnu"
// TBIRD-SAME: "-ftls-model=global-dynamic"

// A RISC-V host compile that asks for Thunderbird offload keeps its own -march.
// Before S04a was narrowed this was silently rewritten to rv64gc, which would
// show up as +f and +d below.
// --offload-host-only matters: without it the Thunderbird device job is in the
// same output, it legitimately carries +d, and a whole-output CHECK-NOT would
// match that instead of saying anything about the host.
// RUN: %clang --target=riscv64-unknown-linux-gnu -march=rv64imac -fopenmp \
// RUN:     --offload-arch=thunderbird --offload-host-only -### -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=RVHOST %s
// RVHOST: "-triple" "riscv64-unknown-linux-gnu"
// RVHOST-SAME: "-target-feature" "+m"
// RVHOST-NOT: "-target-feature" "+d"

void f(void) {
#pragma omp target
  {}
}
