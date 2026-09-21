; An Inspire device module with one __kmpc_fork_call whose outlined callee is
; readnone and willreturn -- the exact shape deleteParallelRegions() erases.
; Only the vendor gate keeps it.
target triple = "riscv64-inspire-linux-gnu"

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
