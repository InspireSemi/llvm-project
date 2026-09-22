// Paired test for the RISC-V device job of --offload-arch=thunderbird.
//
// The bound offload architecture reaches the device toolchain through
// Generic_GCC::TranslateArgs. On RISC-V, -march takes an ISA string and the
// architecture names a processor, so it is passed as -mcpu -- the routing ARM,
// PowerPC and AArch64 offload architectures already get. The device job
// therefore carries -target-cpu thunderbird and that processor's ISA.
//
// The device compile also disables PIE and forces global-dynamic TLS: the
// device image is loaded with dlopen, and local-exec TPREL relocations are
// incompatible with -shared.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// The Inspire device job. Everything is asserted on the same job line -- it is
// the device compilation that gets the processor and the TLS model, and
// asserting them separately would pass if either landed on the host job.
// RUN: %clang -fopenmp --offload-arch=thunderbird -### -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=TBIRD %s
// TBIRD: "-triple" "riscv64-inspire-linux-gnu"
// TBIRD-SAME: "-ftls-model=global-dynamic"
// TBIRD-SAME: "-target-cpu" "thunderbird"
// TBIRD-SAME: "-target-feature" "+d"

// A RISC-V host compile that asks for Thunderbird offload keeps its own -march;
// the bound architecture is translated for the device toolchain only.
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
