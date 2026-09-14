; Paired test for OpenMPOpt kernel selection (audit S18c).
;
; getDeviceKernels selects by kernel calling convention upstream. Thunderbird
; kernels use the C calling convention, so they were invisible to it and were
; internalized and dropped by GlobalDCE at -O1+. The fix admits them by the
; "kernel" attribute, but only for Inspire device modules -- an ungated
; relaxation also stopped counting CUDA kernels linked into OpenMP modules,
; which is what the second half below pins down.
;
; REQUIRES: asserts
;
; RUN: opt -passes=openmp-opt -stats -disable-output %s 2>&1 \
; RUN:   | FileCheck --check-prefix=NVPTX %s
; RUN: opt -passes=openmp-opt -stats -disable-output %S/Inputs/inspire-kernel.ll 2>&1 \
; RUN:   | FileCheck --check-prefix=INSPIRE %s

; A ptx_kernel without the attribute is a CUDA kernel linked alongside OpenMP.
; It must be counted as a non-OpenMP kernel, not selected and not ignored.
; NVPTX: Number of non-OpenMP target region kernels identified
; NVPTX: Number of OpenMP target region entry points

target triple = "nvptx64-nvidia-cuda"

define ptx_kernel void @cuda_kernel() {
entry:
  ret void
}

define ptx_kernel void @omp_kernel() #0 {
entry:
  ret void
}

attributes #0 = { "kernel" }

!llvm.module.flags = !{!0, !1}
!0 = !{i32 7, !"openmp", i32 51}
!1 = !{i32 7, !"openmp-device", i32 51}
; INSPIRE: Number of OpenMP target region entry points
