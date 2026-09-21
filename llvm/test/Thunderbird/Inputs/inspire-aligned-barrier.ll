; An Inspire device module with a redundant aligned barrier -- the shape
; AAExecutionDomain removes once the Attributor runs. Only the vendor gate
; keeps it, and it must be kept at both Attributor entry points.
target triple = "riscv64-inspire-linux-gnu"

declare void @aligned_barrier() "llvm.assume"="ompx_aligned_barrier"

define void @kern() "kernel" {
  call void @aligned_barrier()
  ret void
}

!llvm.module.flags = !{!0, !1}
!0 = !{i32 7, !"openmp", i32 50}
!1 = !{i32 7, !"openmp-device", i32 50}
