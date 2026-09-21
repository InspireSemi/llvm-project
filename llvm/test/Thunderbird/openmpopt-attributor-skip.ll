; Paired test for the Attributor skip (audit S18b).
;
; runAttributor() registers and runs the OpenMP abstract attributes --
; AAKernelInfo, AAExecutionDomain, AAHeapToShared, AAIsDead and the rest --
; whose transformations assume GPU execution semantics: warp-level
; synchronisation for SPMD-isation, worker-thread dispatch for state-machine
; rewriting, thread divergence for dead-block analysis. Thunderbird runs the
; device runtime on pthreads, so those assumptions do not hold and the
; transformations remove or rewrite code that is live. The whole run is skipped
; for Inspire device modules.
;
; This is the broadest behavioural guard in the delta, and like S18a it fails
; silently: the output still compiles and links, with work removed.
;
; The guard must cover BOTH Attributor entry points. The module pass seeds
; AAKernelInfo and the runtime-call folders; the CGSCC pass reaches the
; per-function AAs through registerAAsForFunction(). One RUN line per pipeline
; over the same module is what pins that, and it is the reason this file does
; not simply reuse the S18a shape -- a single pipeline would leave half the
; guard unmeasured.
;
; Barrier elimination is the channel because it is visible in the IR, needs no
; backend and no target intrinsics, and is driven by an AA that both entry
; points reach. -stats also reports it, but a statistic that is ABSENT when the
; guard fires makes for a weaker assertion than a call that is present.
;
; RUN: opt -S -passes=openmp-opt %s 2>&1 \
; RUN:   | FileCheck --check-prefix=OTHER %s
; RUN: opt -S -passes=openmp-opt-cgscc %s 2>&1 \
; RUN:   | FileCheck --check-prefix=OTHER %s
; RUN: opt -S -passes=openmp-opt %S/Inputs/inspire-aligned-barrier.ll 2>&1 \
; RUN:   | FileCheck --check-prefix=INSPIRE %s
; RUN: opt -S -passes=openmp-opt-cgscc %S/Inputs/inspire-aligned-barrier.ll 2>&1 \
; RUN:   | FileCheck --check-prefix=INSPIRE %s

; The control is a GPU device module of the same shape, keeping both openmp
; module flags for the same reason as in the S18a test: dropping openmp-device
; would stop the Attributor by failing isOpenMPDevice() instead of the vendor
; test, measuring the wrong half of the predicate.
; OTHER-NOT: call void @aligned_barrier

target triple = "nvptx64-nvidia-cuda"

declare void @aligned_barrier() "llvm.assume"="ompx_aligned_barrier"

define void @kern() "kernel" {
  call void @aligned_barrier()
  ret void
}

!llvm.module.flags = !{!0, !1}
!0 = !{i32 7, !"openmp", i32 50}
!1 = !{i32 7, !"openmp-device", i32 50}

; ...while the Inspire module keeps its barrier under both pipelines.
; INSPIRE: call void @aligned_barrier
