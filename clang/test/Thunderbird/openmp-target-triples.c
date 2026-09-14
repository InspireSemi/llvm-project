// Paired test for -fopenmp-targets acceptance (audit S06, with S07a behind it).
//
// S06 added TT.isRISCV() to the set of accepted OpenMP target triples. Ungated,
// that also accepted riscv64-unknown-linux-gnu, which the fork base rejects.
// S07a then routed such a target into riscv64::link -- an Inspire-specific
// invocation with a mandatory --device-sysroot and Inspire-only runtime
// libraries -- so the failure surfaced late and as the wrong diagnostic.
//
// The two must stay narrowed together. Accepting the triple here while
// refusing it at the link step would trade an early accurate error for a late
// confusing one, which is worse than either end state.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// The check lives in CompilerInvocation, so the compilation has to actually
// run: -### only prints the driver's jobs and never reaches the front end,
// which means it reports success for both triples and proves nothing here.
//
// Ours is accepted.
// RUN: %clang -fopenmp -fopenmp-targets=riscv64-inspire-linux-gnu \
// RUN:     -fsyntax-only %s 2>&1 | FileCheck --check-prefix=INSPIRE \
// RUN:       --allow-empty %s
// INSPIRE-NOT: OpenMP target is invalid

// A generic riscv64 offload target is rejected, exactly as the fork base
// rejects it.
// RUN: not %clang -fopenmp -fopenmp-targets=riscv64-unknown-linux-gnu \
// RUN:     -fsyntax-only %s 2>&1 | FileCheck --check-prefix=GENERIC %s
// GENERIC: error: OpenMP target is invalid: 'riscv64-unknown-linux-gnu'

void f(void) {
#pragma omp target
  {}
}
