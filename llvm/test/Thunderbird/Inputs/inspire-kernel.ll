; An Inspire device module: the kernel carries the attribute but uses the C
; calling convention, so only the vendor-gated admission finds it.
target triple = "riscv64-inspire-linux-gnu"

define void @tbird_kernel() #0 {
entry:
  ret void
}

attributes #0 = { "kernel" }

!llvm.module.flags = !{!0, !1}
!0 = !{i32 7, !"openmp", i32 51}
!1 = !{i32 7, !"openmp-device", i32 51}
