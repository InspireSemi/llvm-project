; Paired test for parallel-region deletion (audit S18a).
;
; deleteParallelRegions() erases a __kmpc_fork_call whose outlined callee only
; reads memory and will return, on the reasoning that the region has no
; observable effect. On a GPU that holds. On Thunderbird it does not:
; __kmpc_fork_call is what actually dispatches the work onto pthreads, so
; erasing it silently drops every parallel region. The readnone inference that
; triggers the deletion is itself unreliable here, because FunctionAttrs cannot
; always track writes through the captured-variable struct passed via varargs.
;
; Both halves live here on purpose. The deletion is silent -- no diagnostic, no
; link error, correct-looking output with the work removed -- so if this guard
; stopped firing nothing else in the harness would notice. The other half is
; what proves the guard is a vendor gate and not a blanket disabling of an
; upstream optimisation.
;
; Two things about the mechanics are easy to get wrong, and both were checked
; rather than assumed. The guard suppresses eraseFromParent() on the CALL, not
; on any function: @.omp_outlined. keeps its define either way, so a test
; asserting the callee still exists would pass with the guard removed and prove
; nothing. And deleteParallelRegions() is reached only from the CGSCC arm of
; OpenMPOpt::run -- under -passes=openmp-opt the fork call survives in every
; build, so that pipeline would also pass vacuously.
;
; RUN: opt -S -passes=openmp-opt-cgscc %s 2>&1 \
; RUN:   | FileCheck --check-prefix=OTHER %s
; RUN: opt -S -passes=openmp-opt-cgscc %S/Inputs/inspire-parallel-region.ll 2>&1 \
; RUN:   | FileCheck --check-prefix=INSPIRE %s

; The control below is an ordinary GPU device module carrying the same shape.
; It keeps both openmp module flags on purpose: dropping openmp-device would
; also stop the deletion, but by failing isOpenMPDevice() rather than the
; vendor test, which is a different half of the predicate and would leave the
; vendor gate itself unmeasured.
; OTHER-NOT: call void (ptr, i32, ptr, ...) @__kmpc_fork_call

target triple = "nvptx64-nvidia-cuda"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
@0 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 0, ptr null }

define void @foo() {
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr @0, i32 0, ptr @.omp_outlined.)
  ret void
}

define internal void @.omp_outlined.(ptr noalias %tid, ptr noalias %bid) memory(none) willreturn nounwind {
  ret void
}

declare !callback !2 void @__kmpc_fork_call(ptr, i32, ptr, ...)

!llvm.module.flags = !{!0, !1}
!0 = !{i32 7, !"openmp", i32 51}
!1 = !{i32 7, !"openmp-device", i32 51}
!2 = !{!3}
!3 = !{i64 2, i64 -1, i64 -1, i1 true}

; ...while the Inspire module keeps its dispatch.
; INSPIRE: call void (ptr, i32, ptr, ...) @__kmpc_fork_call
