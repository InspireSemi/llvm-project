// Paired test for the device link line (audit S05b).
//
// Thunderbird device images are loaded with dlopen()/memfd, which honours
// PT_LOAD segment permissions. LLD's rosegment, on by default, splits R-- and
// R-X sections into separate segments; at -O2+ that puts .text into a
// non-executable segment and the device faults on the first instruction it
// runs. LinkerWrapper::ConstructJob therefore appends --no-rosegment to the
// device linker arguments, gated on an Inspire-vendor OpenMP toolchain.
//
// Both halves live here on purpose. This guard changes the link line and no
// generated object, so nothing the codegen axes compare can see it: if it
// stopped firing, every other check in the harness would stay green while the
// device faulted at run time. The negative half is what proves the positive
// half is a real vendor gate rather than something every offload target gets.
//
// -c must NOT appear in these RUN lines. It stops the driver at the compile
// phase, so no link action is created, LinkerWrapper::ConstructJob never runs,
// and the test would pass while asserting nothing. The sibling
// driver-arch-and-pie.c does use -c, because it checks -cc1 flags instead.
//
// REQUIRES: riscv-registered-target, x86-registered-target, amdgpu-registered-target

// The flag reaches clang-linker-wrapper as a triple-qualified --device-linker
// argument, which the wrapper re-splits into -Xlinker for the device clang.
// --target is pinned because the Thunderbird triple is derived from the host's
// pointer width.
// RUN: %clang -fopenmp --offload-arch=thunderbird \
// RUN:     --target=x86_64-unknown-linux-gnu -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=TBIRD %s
// TBIRD: "--device-linker=riscv64-inspire-linux-gnu=--no-rosegment"

// An AMDGPU offload link runs through the same loop -- it iterates every
// registered offload toolchain -- and must not pick the flag up.
// This half asserts the link step was reached before asserting the flag is
// absent from it. Without that anchor the CHECK-NOT below would also pass if
// no link job existed at all, which is precisely the vacuous result the -c
// note above warns about.
// RUN: %clang -fopenmp --offload-arch=gfx90a -nogpulib \
// RUN:     --target=x86_64-unknown-linux-gnu -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OTHER %s
// OTHER: clang-linker-wrapper

// ...and the flag itself appears nowhere in that output. Its own FileCheck run
// so the -NOT scans the whole output rather than only the text following the
// anchor above.
// RUN: %clang -fopenmp --offload-arch=gfx90a -nogpulib \
// RUN:     --target=x86_64-unknown-linux-gnu -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OTHER-NEG %s
// OTHER-NEG-NOT: --no-rosegment

void f(void) {
#pragma omp target
  {}
}
