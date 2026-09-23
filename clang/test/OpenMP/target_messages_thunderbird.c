// -fopenmp-targets acceptance for RISC-V triples.
//
// The frontend accepts a riscv64 OpenMP target only for the Inspire vendor. A
// generic riscv64 target is rejected here, as upstream rejects it, so the
// failure surfaces early with the accurate diagnostic rather than later at the
// Inspire-specific device link, whose mandatory --device-sysroot and runtime
// libraries a generic target does not have.
//
// The check lives in CompilerInvocation, so the frontend has to actually run:
// a driver -### only prints jobs and never reaches it.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// RUN: %clang_cc1 -fopenmp -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -fsyntax-only %s 2>&1 \
// RUN:   | FileCheck --check-prefix=INSPIRE --allow-empty %s
// INSPIRE-NOT: OpenMP target is invalid

// RUN: not %clang_cc1 -fopenmp -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-unknown-linux-gnu -fsyntax-only %s 2>&1 \
// RUN:   | FileCheck --check-prefix=GENERIC %s
// GENERIC: error: OpenMP target is invalid: 'riscv64-unknown-linux-gnu'

void f(void) {
#pragma omp target
  {}
}
