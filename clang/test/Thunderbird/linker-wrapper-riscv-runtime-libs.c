// Paired test for the Inspire device runtime libraries (audit S07b, with S07a
// behind it).
//
// riscv64::link builds the device image as a shared object loaded by the
// device server at run time. Its OpenMP runtime lives in libomp.so on the
// device image rather than being linked into the image, so the link must name
// -lomp for the __kmpc_* references to have declarations -- the -Wl,--no-undefined
// a few arguments earlier then still rejects a genuinely missing symbol, while
// the runtime symbols resolve through DT_NEEDED at dlopen. -lm and -latomic
// ride in the same block, bracketed by --as-needed so they are dropped when
// unused.
//
// S07a is the dispatch one level up: riscv64 reaches riscv64::link at all only
// for the Inspire vendor. Both halves are here because they must stay narrowed
// together -- routing a generic riscv64 target into an Inspire-only link would
// trade an accurate early error for a confusing late one.
//
// No device link runs. --dry-run makes clang-linker-wrapper print the commands
// it would have issued and return without spawning them, findProgram
// short-circuits, and the sysroot below is named but never opened -- so no
// device toolchain, no riscv lld and no Inspire runtime is required here. The
// packager and -cc1 steps above are real; only the link is not.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// The image payload is not a real device object and does not need to be: the
// wrapper dispatches on the triple recorded by clang-offload-packager, not on
// the contents. Upstream's own linker-wrapper tests hand an x86-64 object to
// nvptx and amdgcn images for the same reason.
// RUN: %clang -cc1 %s -triple x86_64-unknown-linux-gnu -emit-obj -o %t.elf.o
// RUN: clang-offload-packager -o %t.inspire.out \
// RUN:     --image=file=%t.elf.o,kind=openmp,triple=riscv64-inspire-linux-gnu,arch=thunderbird
// RUN: %clang -cc1 %s -triple x86_64-unknown-linux-gnu -emit-obj -o %t.inspire.o \
// RUN:     -fembed-offload-object=%t.inspire.out

// The whole block lands on the device clang command line, so it is asserted
// with -SAME against the --target that identifies that command. Checking the
// libraries alone would also match the host link line further down.
// RUN: clang-linker-wrapper --host-triple=x86_64-unknown-linux-gnu --dry-run \
// RUN:     --linker-path=/usr/bin/ld --device-sysroot=%t.sysroot \
// RUN:     %t.inspire.o -o %t.inspire.exe 2>&1 \
// RUN:   | FileCheck --check-prefix=TBIRD %s
// TBIRD: --target=riscv64-inspire-linux-gnu
// TBIRD-SAME: -lomp
// TBIRD-SAME: -Wl,--as-needed
// TBIRD-SAME: -lm
// TBIRD-SAME: -latomic
// TBIRD-SAME: -Wl,--no-as-needed

// A generic riscv64 offload target never reaches riscv64::link, so it cannot
// pick up the Inspire runtime libraries. It gets the fork base's diagnostic,
// which is the behaviour S07a was narrowed to preserve.
// RUN: clang-offload-packager -o %t.generic.out \
// RUN:     --image=file=%t.elf.o,kind=openmp,triple=riscv64-unknown-linux-gnu,arch=generic
// RUN: %clang -cc1 %s -triple x86_64-unknown-linux-gnu -emit-obj -o %t.generic.o \
// RUN:     -fembed-offload-object=%t.generic.out
// RUN: not clang-linker-wrapper --host-triple=x86_64-unknown-linux-gnu --dry-run \
// RUN:     --linker-path=/usr/bin/ld --device-sysroot=%t.sysroot \
// RUN:     %t.generic.o -o %t.generic.exe 2>&1 \
// RUN:   | FileCheck --check-prefix=GENERIC %s
// GENERIC: error: riscv64 linking is not supported

// The sysroot is mandatory and refused early rather than producing a link that
// silently searches the host's libraries.
// RUN: not clang-linker-wrapper --host-triple=x86_64-unknown-linux-gnu --dry-run \
// RUN:     --linker-path=/usr/bin/ld \
// RUN:     %t.inspire.o -o %t.nosysroot.exe 2>&1 \
// RUN:   | FileCheck --check-prefix=NOSYSROOT %s
// NOSYSROOT: error: RISC-V offload linking requires --device-sysroot

void f(void) {
#pragma omp target
  {}
}
