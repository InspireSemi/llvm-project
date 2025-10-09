	.file	"not-basic.c"
	.text
	.globl	main                            # -- Begin function main
	.p2align	4
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$208, %rsp
	movl	$0, -4(%rbp)
	movl	$1048576, -8(%rbp)              # imm = 0x100000
	movq	$0, -16(%rbp)
	movl	-8(%rbp), %eax
	movl	%eax, -24(%rbp)
	movq	-24(%rbp), %rax
	movq	%rax, -200(%rbp)                # 8-byte Spill
	movq	%rax, -40(%rbp)
	movq	%rax, -56(%rbp)
	movq	$0, -72(%rbp)
	leaq	-16(%rbp), %rax
	movq	%rax, -32(%rbp)
	movq	%rax, -48(%rbp)
	movq	$0, -64(%rbp)
	movl	$3, -184(%rbp)
	movl	$2, -180(%rbp)
	leaq	-40(%rbp), %rax
	movq	%rax, -176(%rbp)
	leaq	-56(%rbp), %rax
	movq	%rax, -168(%rbp)
	leaq	.L.offload_sizes(%rip), %rax
	movq	%rax, -160(%rbp)
	leaq	.L.offload_maptypes(%rip), %rax
	movq	%rax, -152(%rbp)
	movq	$0, -144(%rbp)
	movq	$0, -136(%rbp)
	movq	$1048576, -128(%rbp)            # imm = 0x100000
	movq	$0, -120(%rbp)
	movl	$0, -104(%rbp)
	movl	$0, -108(%rbp)
	movl	$0, -112(%rbp)
	movl	$0, -92(%rbp)
	movl	$0, -96(%rbp)
	movl	$0, -100(%rbp)
	movl	$0, -88(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movq	$-1, %rsi
	xorl	%ecx, %ecx
	movq	.__omp_offloading_802_5ae8f81_main_l9.region_id@GOTPCREL(%rip), %r8
	leaq	-184(%rbp), %r9
	movl	%ecx, %edx
	callq	__tgt_target_kernel@PLT
	cmpl	$0, %eax
	je	.LBB0_2
# %bb.1:                                # %omp_offload.failed
	movq	-200(%rbp), %rdi                # 8-byte Reload
	leaq	-16(%rbp), %rsi
	callq	__omp_offloading_802_5ae8f81_main_l9
.LBB0_2:                                # %omp_offload.cont
	movl	$0, -188(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$1, %esi
	leaq	main.omp_outlined(%rip), %rdx
	leaq	-188(%rbp), %rcx
	movb	$0, %al
	callq	__kmpc_fork_call@PLT
	movsd	-16(%rbp), %xmm0                # xmm0 = mem[0],zero
	movl	-188(%rbp), %edx
	leaq	.L.str(%rip), %rdi
	movl	$1048576, %esi                  # imm = 0x100000
	movb	$1, %al
	callq	printf@PLT
	xorl	%eax, %eax
	addq	$208, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9
	.type	__omp_offloading_802_5ae8f81_main_l9,@function
__omp_offloading_802_5ae8f81_main_l9:   # @__omp_offloading_802_5ae8f81_main_l9
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-16(%rbp), %r8
	movl	-8(%rbp), %eax
	movl	%eax, -24(%rbp)
	movq	-24(%rbp), %rcx
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$2, %esi
	leaq	__omp_offloading_802_5ae8f81_main_l9.omp_outlined(%rip), %rdx
	movb	$0, %al
	callq	__kmpc_fork_teams@PLT
	addq	$32, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	__omp_offloading_802_5ae8f81_main_l9, .Lfunc_end1-__omp_offloading_802_5ae8f81_main_l9
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$160, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	%rcx, -32(%rbp)
	movq	-32(%rbp), %rax
	movq	%rax, -104(%rbp)                # 8-byte Spill
	xorps	%xmm0, %xmm0
	movsd	%xmm0, -40(%rbp)
	movl	$0, -52(%rbp)
	movl	$1048575, -56(%rbp)             # imm = 0xFFFFF
	movl	$1, -60(%rbp)
	movl	$0, -64(%rbp)
	movq	-8(%rbp), %rax
	movl	(%rax), %esi
	movl	%esi, -92(%rbp)                 # 4-byte Spill
	leaq	.L__unnamed_2(%rip), %rdi
	movl	$92, %edx
	leaq	-64(%rbp), %rcx
	leaq	-52(%rbp), %r8
	leaq	-56(%rbp), %r9
	leaq	-60(%rbp), %rax
	movq	%rax, (%rsp)
	movl	$1, 8(%rsp)
	movl	$1, 16(%rsp)
	callq	__kmpc_for_static_init_4@PLT
	cmpl	$1048575, -56(%rbp)             # imm = 0xFFFFF
	jle	.LBB2_2
# %bb.1:                                # %cond.true
	movl	$1048575, %eax                  # imm = 0xFFFFF
	movl	%eax, -108(%rbp)                # 4-byte Spill
	jmp	.LBB2_3
.LBB2_2:                                # %cond.false
	movl	-56(%rbp), %eax
	movl	%eax, -108(%rbp)                # 4-byte Spill
.LBB2_3:                                # %cond.end
	movl	-108(%rbp), %eax                # 4-byte Reload
	movl	%eax, -56(%rbp)
	movl	-52(%rbp), %eax
	movl	%eax, -44(%rbp)
.LBB2_4:                                # %omp.inner.for.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	-44(%rbp), %eax
	cmpl	-56(%rbp), %eax
	jg	.LBB2_7
# %bb.5:                                # %omp.inner.for.body
                                        #   in Loop: Header=BB2_4 Depth=1
	movl	-52(%rbp), %eax
	movl	%eax, %ecx
	movl	-56(%rbp), %eax
	movl	%eax, %r8d
	movl	-24(%rbp), %eax
	movl	%eax, -80(%rbp)
	movq	-80(%rbp), %r9
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$4, %esi
	leaq	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined(%rip), %rdx
	leaq	-40(%rbp), %rax
	movq	%rax, (%rsp)
	movb	$0, %al
	callq	__kmpc_fork_call@PLT
# %bb.6:                                # %omp.inner.for.inc
                                        #   in Loop: Header=BB2_4 Depth=1
	movl	-44(%rbp), %eax
	addl	-60(%rbp), %eax
	movl	%eax, -44(%rbp)
	jmp	.LBB2_4
.LBB2_7:                                # %omp.inner.for.end
	jmp	.LBB2_8
.LBB2_8:                                # %omp.loop.exit
	movl	-92(%rbp), %esi                 # 4-byte Reload
	leaq	.L__unnamed_2(%rip), %rdi
	callq	__kmpc_for_static_fini@PLT
	movl	-92(%rbp), %esi                 # 4-byte Reload
	leaq	-40(%rbp), %rax
	movq	%rax, -88(%rbp)
	movq	.gomp_critical_user_.reduction.var@GOTPCREL(%rip), %rcx
	movq	%rsp, %rax
	movq	%rcx, (%rax)
	leaq	.L__unnamed_3(%rip), %rdi
	leaq	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func(%rip), %r9
	movl	$1, %edx
	movl	$8, %ecx
	leaq	-88(%rbp), %r8
	callq	__kmpc_reduce_nowait@PLT
	movl	%eax, %ecx
	movl	%ecx, -112(%rbp)                # 4-byte Spill
	subl	$1, %eax
	je	.LBB2_9
	jmp	.LBB2_14
.LBB2_14:                               # %omp.loop.exit
	movl	-112(%rbp), %eax                # 4-byte Reload
	subl	$2, %eax
	je	.LBB2_10
	jmp	.LBB2_13
.LBB2_9:                                # %.omp.reduction.case1
	movl	-92(%rbp), %esi                 # 4-byte Reload
	movq	-104(%rbp), %rax                # 8-byte Reload
	movsd	(%rax), %xmm0                   # xmm0 = mem[0],zero
	addsd	-40(%rbp), %xmm0
	movsd	%xmm0, (%rax)
	leaq	.L__unnamed_3(%rip), %rdi
	movq	.gomp_critical_user_.reduction.var@GOTPCREL(%rip), %rdx
	callq	__kmpc_end_reduce_nowait@PLT
	jmp	.LBB2_13
.LBB2_10:                               # %.omp.reduction.case2
	movq	-104(%rbp), %rax                # 8-byte Reload
	movsd	-40(%rbp), %xmm0                # xmm0 = mem[0],zero
	movsd	%xmm0, -128(%rbp)               # 8-byte Spill
	movsd	(%rax), %xmm0                   # xmm0 = mem[0],zero
	movsd	%xmm0, -120(%rbp)               # 8-byte Spill
.LBB2_11:                               # %atomicrmw.start
                                        # =>This Inner Loop Header: Depth=1
	movsd	-120(%rbp), %xmm0               # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movq	-104(%rbp), %rcx                # 8-byte Reload
	movsd	-128(%rbp), %xmm2               # 8-byte Reload
                                        # xmm2 = mem[0],zero
	movaps	%xmm0, %xmm1
	addsd	%xmm2, %xmm1
	movq	%xmm1, %rdx
	movq	%xmm0, %rax
	lock		cmpxchgq	%rdx, (%rcx)
	movq	%rax, %rcx
	sete	%al
	movq	%rcx, %xmm0
	testb	$1, %al
	movsd	%xmm0, -120(%rbp)               # 8-byte Spill
	jne	.LBB2_12
	jmp	.LBB2_11
.LBB2_12:                               # %atomicrmw.end
	jmp	.LBB2_13
.LBB2_13:                               # %.omp.reduction.default
	addq	$160, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end2:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined, .Lfunc_end2-__omp_offloading_802_5ae8f81_main_l9.omp_outlined
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3, 0x0                          # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined
.LCPI3_0:
	.quad	0x3ff0000000000000              # double 1
	.text
	.p2align	4
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$160, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	%rcx, -32(%rbp)
	movq	%r8, -40(%rbp)
	movq	%r9, -48(%rbp)
	movq	-48(%rbp), %rax
	movq	%rax, -112(%rbp)                # 8-byte Spill
	movl	$0, -60(%rbp)
	movl	$1048575, -64(%rbp)             # imm = 0xFFFFF
	movq	-24(%rbp), %rax
	movl	%eax, %ecx
	movq	-32(%rbp), %rax
                                        # kill: def $eax killed $eax killed $rax
	movl	%ecx, -60(%rbp)
	movl	%eax, -64(%rbp)
	movl	$1, -68(%rbp)
	movl	$0, -72(%rbp)
	xorps	%xmm0, %xmm0
	movsd	%xmm0, -80(%rbp)
	movq	-8(%rbp), %rax
	movl	(%rax), %esi
	movl	%esi, -100(%rbp)                # 4-byte Spill
	leaq	.L__unnamed_4(%rip), %rdi
	movl	$34, %edx
	leaq	-72(%rbp), %rcx
	leaq	-60(%rbp), %r8
	leaq	-64(%rbp), %r9
	leaq	-68(%rbp), %rax
	movq	%rax, (%rsp)
	movl	$1, 8(%rsp)
	movl	$1, 16(%rsp)
	callq	__kmpc_for_static_init_4@PLT
	cmpl	$1048575, -64(%rbp)             # imm = 0xFFFFF
	jle	.LBB3_2
# %bb.1:                                # %cond.true
	movl	$1048575, %eax                  # imm = 0xFFFFF
	movl	%eax, -116(%rbp)                # 4-byte Spill
	jmp	.LBB3_3
.LBB3_2:                                # %cond.false
	movl	-64(%rbp), %eax
	movl	%eax, -116(%rbp)                # 4-byte Spill
.LBB3_3:                                # %cond.end
	movl	-116(%rbp), %eax                # 4-byte Reload
	movl	%eax, -64(%rbp)
	movl	-60(%rbp), %eax
	movl	%eax, -52(%rbp)
.LBB3_4:                                # %omp.inner.for.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	-52(%rbp), %eax
	cmpl	-64(%rbp), %eax
	jg	.LBB3_8
# %bb.5:                                # %omp.inner.for.body
                                        #   in Loop: Header=BB3_4 Depth=1
	movl	-52(%rbp), %eax
	shll	$0, %eax
	addl	$0, %eax
	movl	%eax, -84(%rbp)
	cvtsi2sdl	-84(%rbp), %xmm1
	movsd	.LCPI3_0(%rip), %xmm0           # xmm0 = [1.0E+0,0.0E+0]
	addsd	%xmm0, %xmm1
	movsd	.LCPI3_0(%rip), %xmm0           # xmm0 = [1.0E+0,0.0E+0]
	divsd	%xmm1, %xmm0
	addsd	-80(%rbp), %xmm0
	movsd	%xmm0, -80(%rbp)
# %bb.6:                                # %omp.body.continue
                                        #   in Loop: Header=BB3_4 Depth=1
	jmp	.LBB3_7
.LBB3_7:                                # %omp.inner.for.inc
                                        #   in Loop: Header=BB3_4 Depth=1
	movl	-52(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -52(%rbp)
	jmp	.LBB3_4
.LBB3_8:                                # %omp.inner.for.end
	jmp	.LBB3_9
.LBB3_9:                                # %omp.loop.exit
	movl	-100(%rbp), %esi                # 4-byte Reload
	leaq	.L__unnamed_4(%rip), %rdi
	callq	__kmpc_for_static_fini@PLT
	movl	-100(%rbp), %esi                # 4-byte Reload
	leaq	-80(%rbp), %rax
	movq	%rax, -96(%rbp)
	movq	.gomp_critical_user_.reduction.var@GOTPCREL(%rip), %rcx
	movq	%rsp, %rax
	movq	%rcx, (%rax)
	leaq	.L__unnamed_3(%rip), %rdi
	leaq	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func(%rip), %r9
	movl	$1, %edx
	movl	$8, %ecx
	leaq	-96(%rbp), %r8
	callq	__kmpc_reduce_nowait@PLT
	movl	%eax, %ecx
	movl	%ecx, -120(%rbp)                # 4-byte Spill
	subl	$1, %eax
	je	.LBB3_10
	jmp	.LBB3_15
.LBB3_15:                               # %omp.loop.exit
	movl	-120(%rbp), %eax                # 4-byte Reload
	subl	$2, %eax
	je	.LBB3_11
	jmp	.LBB3_14
.LBB3_10:                               # %.omp.reduction.case1
	movl	-100(%rbp), %esi                # 4-byte Reload
	movq	-112(%rbp), %rax                # 8-byte Reload
	movsd	(%rax), %xmm0                   # xmm0 = mem[0],zero
	addsd	-80(%rbp), %xmm0
	movsd	%xmm0, (%rax)
	leaq	.L__unnamed_3(%rip), %rdi
	movq	.gomp_critical_user_.reduction.var@GOTPCREL(%rip), %rdx
	callq	__kmpc_end_reduce_nowait@PLT
	jmp	.LBB3_14
.LBB3_11:                               # %.omp.reduction.case2
	movq	-112(%rbp), %rax                # 8-byte Reload
	movsd	-80(%rbp), %xmm0                # xmm0 = mem[0],zero
	movsd	%xmm0, -136(%rbp)               # 8-byte Spill
	movsd	(%rax), %xmm0                   # xmm0 = mem[0],zero
	movsd	%xmm0, -128(%rbp)               # 8-byte Spill
.LBB3_12:                               # %atomicrmw.start
                                        # =>This Inner Loop Header: Depth=1
	movsd	-128(%rbp), %xmm0               # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movq	-112(%rbp), %rcx                # 8-byte Reload
	movsd	-136(%rbp), %xmm2               # 8-byte Reload
                                        # xmm2 = mem[0],zero
	movaps	%xmm0, %xmm1
	addsd	%xmm2, %xmm1
	movq	%xmm1, %rdx
	movq	%xmm0, %rax
	lock		cmpxchgq	%rdx, (%rcx)
	movq	%rax, %rcx
	sete	%al
	movq	%rcx, %xmm0
	testb	$1, %al
	movsd	%xmm0, -128(%rbp)               # 8-byte Spill
	jne	.LBB3_13
	jmp	.LBB3_12
.LBB3_13:                               # %atomicrmw.end
	jmp	.LBB3_14
.LBB3_14:                               # %.omp.reduction.default
	addq	$160, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end3:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined, .Lfunc_end3-__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rax
	movq	-16(%rbp), %rcx
	movq	(%rcx), %rcx
	movq	(%rax), %rax
	movsd	(%rax), %xmm0                   # xmm0 = mem[0],zero
	addsd	(%rcx), %xmm0
	movsd	%xmm0, (%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end4:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func, .Lfunc_end4-__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rax
	movq	-16(%rbp), %rcx
	movq	(%rcx), %rcx
	movq	(%rax), %rax
	movsd	(%rax), %xmm0                   # xmm0 = mem[0],zero
	addsd	(%rcx), %xmm0
	movsd	%xmm0, (%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end5:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func, .Lfunc_end5-__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function main.omp_outlined
	.type	main.omp_outlined,@function
main.omp_outlined:                      # @main.omp_outlined
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$48, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	-24(%rbp), %rax
	movq	%rax, -40(%rbp)                 # 8-byte Spill
	movq	-8(%rbp), %rax
	movl	(%rax), %esi
	movl	%esi, -28(%rbp)                 # 4-byte Spill
	leaq	.L__unnamed_1(%rip), %rdi
	callq	__kmpc_single@PLT
	cmpl	$0, %eax
	je	.LBB6_2
# %bb.1:                                # %omp_if.then
	callq	omp_get_num_threads@PLT
	movl	-28(%rbp), %esi                 # 4-byte Reload
	movl	%eax, %ecx
	movq	-40(%rbp), %rax                 # 8-byte Reload
	movl	%ecx, (%rax)
	leaq	.L__unnamed_1(%rip), %rdi
	callq	__kmpc_end_single@PLT
.LBB6_2:                                # %omp_if.end
	movl	-28(%rbp), %esi                 # 4-byte Reload
	leaq	.L__unnamed_5(%rip), %rdi
	callq	__kmpc_barrier@PLT
	addq	$48, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end6:
	.size	main.omp_outlined, .Lfunc_end6-main.omp_outlined
	.cfi_endproc
                                        # -- End function
	.type	.L__unnamed_6,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_6:
	.asciz	";unknown;unknown;0;0;;"
	.size	.L__unnamed_6, 23

	.type	.L__unnamed_2,@object           # @1
	.section	.data.rel.ro,"aw",@progbits
	.p2align	3, 0x0
.L__unnamed_2:
	.long	0                               # 0x0
	.long	2050                            # 0x802
	.long	0                               # 0x0
	.long	22                              # 0x16
	.quad	.L__unnamed_6
	.size	.L__unnamed_2, 24

	.type	.L__unnamed_4,@object           # @2
	.p2align	3, 0x0
.L__unnamed_4:
	.long	0                               # 0x0
	.long	514                             # 0x202
	.long	0                               # 0x0
	.long	22                              # 0x16
	.quad	.L__unnamed_6
	.size	.L__unnamed_4, 24

	.type	.gomp_critical_user_.reduction.var,@object # @.gomp_critical_user_.reduction.var
	.comm	.gomp_critical_user_.reduction.var,32,8
	.type	.L__unnamed_3,@object           # @3
	.p2align	3, 0x0
.L__unnamed_3:
	.long	0                               # 0x0
	.long	18                              # 0x12
	.long	0                               # 0x0
	.long	22                              # 0x16
	.quad	.L__unnamed_6
	.size	.L__unnamed_3, 24

	.type	.L__unnamed_1,@object           # @4
	.p2align	3, 0x0
.L__unnamed_1:
	.long	0                               # 0x0
	.long	2                               # 0x2
	.long	0                               # 0x0
	.long	22                              # 0x16
	.quad	.L__unnamed_6
	.size	.L__unnamed_1, 24

	.type	.__omp_offloading_802_5ae8f81_main_l9.region_id,@object # @.__omp_offloading_802_5ae8f81_main_l9.region_id
	.section	.rodata,"a",@progbits
	.weak	.__omp_offloading_802_5ae8f81_main_l9.region_id
.__omp_offloading_802_5ae8f81_main_l9.region_id:
	.byte	0                               # 0x0
	.size	.__omp_offloading_802_5ae8f81_main_l9.region_id, 1

	.type	.L.offload_sizes,@object        # @.offload_sizes
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	3, 0x0
.L.offload_sizes:
	.quad	4                               # 0x4
	.quad	8                               # 0x8
	.size	.L.offload_sizes, 16

	.type	.L.offload_maptypes,@object     # @.offload_maptypes
	.p2align	3, 0x0
.L.offload_maptypes:
	.quad	800                             # 0x320
	.quad	35                              # 0x23
	.size	.L.offload_maptypes, 16

	.type	.L__unnamed_5,@object           # @5
	.section	.data.rel.ro,"aw",@progbits
	.p2align	3, 0x0
.L__unnamed_5:
	.long	0                               # 0x0
	.long	322                             # 0x142
	.long	0                               # 0x0
	.long	22                              # 0x16
	.quad	.L__unnamed_6
	.size	.L__unnamed_5, 24

	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"sum=%f (N=%d) host_threads=%d\n"
	.size	.L.str, 31

	.type	.offloading.entry_name,@object  # @.offloading.entry_name
	.section	.llvm.rodata.offloading,"aMS",@progbits,1,unique,1
.offloading.entry_name:
	.asciz	"__omp_offloading_802_5ae8f81_main_l9"
	.size	.offloading.entry_name, 37

	.type	.offloading.entry.__omp_offloading_802_5ae8f81_main_l9,@object # @.offloading.entry.__omp_offloading_802_5ae8f81_main_l9
	.section	llvm_offload_entries,"aw",@progbits
	.weak	.offloading.entry.__omp_offloading_802_5ae8f81_main_l9
	.p2align	3, 0x0
.offloading.entry.__omp_offloading_802_5ae8f81_main_l9:
	.quad	0                               # 0x0
	.short	1                               # 0x1
	.short	1                               # 0x1
	.long	0                               # 0x0
	.quad	.__omp_offloading_802_5ae8f81_main_l9.region_id
	.quad	.offloading.entry_name
	.quad	0                               # 0x0
	.quad	0                               # 0x0
	.quad	0
	.size	.offloading.entry.__omp_offloading_802_5ae8f81_main_l9, 56

	.type	.Lllvm.embedded.object,@object  # @llvm.embedded.object
	.section	.llvm.offloading,"e",@llvm_offloading
	.p2align	3, 0x0
.Lllvm.embedded.object:
	.asciz	"\020\377\020\255\001\000\000\000\360 \000\000\000\000\000\000 \000\000\000\000\000\000\000(\000\000\000\000\000\000\000\001\000\001\000\000\000\000\000H\000\000\000\000\000\000\000\002\000\000\000\000\000\000\000\240\000\000\000\000\000\000\000P \000\000\000\000\000\000\212\000\000\000\000\000\000\000n\000\000\000\000\000\000\000i\000\000\000\000\000\000\000\221\000\000\000\000\000\000\000\000arch\000riscv64-inspire-unknown-elf\000triple\000generic\000\000\000\000\000\000\000\000\177ELF\002\001\001\000\000\000\000\000\000\000\000\000\001\000\363\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\220\034\000\000\000\000\000\000\001\000\000\000@\000\000\000\000\000@\000\017\000\001\000yq\006\364\"\360\000\030#4\244\376#0\264\376#<\304\374\0037\204\375\003%\004\376#(\244\374\2036\004\375\027\005\000\000\023\005\005\000\227\005\000\000\023\206\005\000\211E\227\000\000\000\347\200\000\000\242p\002tEa\202\2001q\006\375\"\371\200\001#4\244\376#0\264\376#<\304\374#8\324\374\0035\004\375#0\244\370\001E#4\244\374#.\244\372\267\005\020\000#8\264\370\375\025#,\264\372\205H#*\024\373#(\244\372\0035\204\376\fA#4\264\370\n\205#0\025\001\027\005\000\000\023\005\005\000\023\006\300\005\223\006\004\373\023\007\304\373\223\007\204\373\023\bD\373\227\000\000\000\347\200\000\000\2035\004\371\003%\204\373cI\265\000\t\2407\005\020\000}\025#<\244\3661\240\003%\204\373#<\244\366\t\240\0035\204\367#,\244\372\003%\304\373#\"\244\374\t\240\203%D\374\003%\204\373c@\265\000\t\240\203f\304\373\003g\204\373\003%\204\375# \244\372\2037\004\372\027\005\000\000\023\005\005\000\227\005\000\000\023\206\005\000\221E\023\b\204\374\227\000\000\000\347\200\000\000\t\240\003%D\374\203%D\373-\235#\"\244\374\001\240\t\240\0035\204\370\233\005\005\000#0\264\366\027\005\000\000\023\005\005\000\227\000\000\000\347\200\000\000\2035\004\366\023\005\204\374#<\244\370\027\005\000\000\023\005\005\000\027\006\000\000\223\007\006\000\027\006\000\000\0038\006\000\005F#4\304\366\241F\023\007\204\371\227\000\000\000\347\200\000\000\2035\204\366*\206#8\304\366c\n\265\000\t\240\0035\004\367\001%\211Ec\000\265\000\001\240\0035\004\370\ba\2035\204\374\227\000\000\000\347\200\000\000\0036\004\370\252\205\0035\204\370\f\342\233\005\005\000\027\005\000\000\023\005\005\000\027\006\000\000\0036\006\000\227\000\000\000\347\200\000\000\001\240\0035\004\370\2035\204\374#8\264\364\ba#<\244\364\t\240\0035\204\365\2035\004\365#4\244\364\227\000\000\000\347\200\000\000\2036\004\370\2035\204\364*\207/\265\006\020c\025\265\000/\266\346\030u\372*\206#<\304\364c\020\265\000\t\240\t\240\352pJt)a\202\200Uq\206\345\242\341\200\t#4\244\376#0\264\376#<\304\374#8\324\374#4\344\374#0\364\374\0035\004\374#<\244\366\001E#*\244\372\267\005\020\000#4\264\370\375\025#(\264\372\0036\204\375\2035\004\375#*\304\372#(\264\372\205H#&\024\373#$\244\372#0\244\372\0035\204\376\fA#0\264\370\n\205#0\025\001\027\005\000\000\023\005\005\000\023\006 \002\223\006\204\372\023\007D\373\223\007\004\373\023\b\304\372\227\000\000\000\347\200\000\000\2035\204\370\003%\004\373cI\265\000\t\2407\005\020\000}\025#8\244\3661\240\003%\004\373#8\244\366\t\240\0035\004\367#(\244\372\003%D\373#.\244\372\t\240\203%\304\373\003%\004\373c@\265\000\t\240\003%\304\373#.\244\370\003%\304\371\227\000\000\000\347\200\000\000\223\005\360?\322\025#4\264\366\227\000\000\000\347\200\000\000\252\205\0035\204\366\227\000\000\000\347\200\000\000\252\205\0035\004\372\227\000\000\000\347\200\000\000#0\244\372\t\240\t\240\003%\304\373\005%#.\244\372\001\240\t\240\0035\004\370\233\005\005\000#8\264\364\027\005\000\000\023\005\005\000\227\000\000\000\347\200\000\000\2035\004\365\023\005\004\372#8\244\370\027\005\000\000\023\005\005\000\027\006\000\000\223\007\006\000\027\006\000\000\0038\006\000\005F#<\304\364\241F\023\007\004\371\227\000\000\000\347\200\000\000\2035\204\365*\206#0\304\366c\n\265\000\t\240\0035\004\366\001%\211Ec\000\265\000\001\240\0035\204\367\ba\2035\004\372\227\000\000\000\347\200\000\000\0036\204\367\252\205\0035\004\370\f\342\233\005\005\000\027\005\000\000\023\005\005\000\027\006\000\000\0036\006\000\227\000\000\000\347\200\000\000\001\240\0035\204\367\2035\004\372#0\264\364\ba#4\244\364\t\240\0035\204\364\2035\004\364#<\244\362\227\000\000\000\347\200\000\000\2036\204\367\2035\204\363*\207/\265\006\020c\025\265\000/\266\346\030u\372*\206#4\304\364c\020\265\000\t\240\t\240\256`\016dia\202\200yq\006\364\"\360\000\030#4\244\376#0\264\376\0035\204\376\2035\004\376\214a\ba#<\244\374\ba\214a\227\000\000\000\347\200\000\000\2035\204\375\210\341\242p\002tEa\202\200yq\006\364\"\360\000\030#4\244\376#0\264\376\0035\204\376\2035\004\376\214a\ba#<\244\374\ba\214a\227\000\000\000\347\200\000\000\2035\204\375\210\341\242p\002tEa\202\200;unknown;unknown;0;0;;\000\000\000\000\000\000\000\000\000\000\002\b\000\000\000\000\000\000\026\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\002\000\000\000\000\000\000\026\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\022\000\000\000\000\000\000\000\026\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000\000\000\000\000\000\000\026\000\000\000\000\000\000\000\000\000\000\000__omp_offloading_802_5ae8f81_main_l9\000\000\000\000\000\000\000\000\000\000\000\000\001\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000clang version 21.0.0git (https://github.com/inspiresemi/llvm-project bd3b6daacb1382591fbeeef56a50c471709dbc2b)\000AM\000\000\000riscv\000\001C\000\000\000\005rv64i2p1_m2p0_a2p1_c2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0\000\004\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000Y\002\000\000\004\000\361\377\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000I\004\000\000\000\000\002\000$\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\026\004\000\000\001\000\005\000H\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000\n\004\000\000\000\000\002\000,\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000y\001\000\000\002\000\002\000F\000\000\000\000\000\000\000\004\002\000\000\000\000\000\000\321\003\000\000\000\000\002\000\230\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\335\003\000\000\001\000\005\000\000\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000q\003\000\000\000\000\002\000\362\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\330\002\000\000\000\000\002\000D\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\217\003\000\000\000\000\002\000\024\001\000\000\000\000\000\000\000\000\000\000\000\000\000\0006\003\000\000\000\000\002\000\034\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000:\001\000\000\002\000\002\000J\002\000\000\000\000\000\000.\002\000\000\000\000\000\000\006\003\000\000\000\000\002\000R\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\355\002\000\000\000\000\002\000n\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\255\003\000\000\001\000\005\0000\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000\314\002\000\000\000\000\002\000v\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\n\002\000\000\002\000\002\000\262\004\000\000\000\000\000\000:\000\000\000\000\000\000\000\270\002\000\000\000\000\002\000~\001\000\000\000\000\000\000\000\000\000\000\000\000\000\0001\004\000\000\000\000\002\000\364\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\206\003\000\000\000\000\002\000B\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\254\002\000\000\000\000\002\000\332\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000U\004\000\000\000\000\002\000\342\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\004\000\000\000\000\002\000\b\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000$\004\000\000\000\000\002\000\264\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000[\003\000\000\001\000\005\000\030\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000i\003\000\000\000\000\002\000\016\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\304\002\000\000\000\000\002\000r\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\353\003\000\000\000\000\002\000\200\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\273\003\000\000\000\000\002\000\234\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000y\003\000\000\000\000\002\000\244\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\256\001\000\000\002\000\002\000x\004\000\000\000\000\000\000:\000\000\000\000\000\000\000 \003\000\000\000\000\002\000\254\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\370\003\000\000\000\000\002\000\"\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000-\003\000\000\000\000\002\000p\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\371\002\000\000\000\000\002\000\b\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\340\002\000\000\000\000\002\000\020\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\310\003\000\000\000\000\002\0006\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\022\003\000\000\001\000\004\000\000\000\000\000\000\000\000\000\027\000\000\000\000\000\000\000\253\001\000\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\253\001\000\000\000\000\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000#\001\000\000\001\000\007\000\000\000\000\000\000\000\000\000%\000\000\000\000\000\000\000\253\001\000\000\000\000\007\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\253\001\000\000\000\000\b\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\253\001\000\000\000\000\n\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\253\001\000\000\000\000\f\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\207\002\000\000\"\003\002\000\000\000\000\000\000\000\000\000F\000\000\000\000\000\000\000F\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000B\003\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\271\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\332\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\204\000\000\000\021\000\362\377\b\000\000\000\000\000\000\000 \000\000\000\000\000\000\0001\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\244\003\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\030\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\027\001\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\233\003\000\000\020\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000u\002\000\000!\000\b\000\000\000\000\000\000\000\000\0008\000\000\000\000\000\000\000$\000\000\000\000\000\000\000\027\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000$\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000(\000\000\000\000\000\000\000\030\000\000\000\003\000\000\000\000\000\000\000\000\000\000\000(\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000,\000\000\000\000\000\000\000\027\000\000\000\006\000\000\000\000\000\000\000\000\000\000\000,\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\0000\000\000\000\000\000\000\000\030\000\000\000\005\000\000\000\000\000\000\000\000\000\000\0000\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\0006\000\000\000\000\000\000\000\023\000\000\0000\000\000\000\000\000\000\000\000\000\000\0006\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\230\000\000\000\000\000\000\000\027\000\000\000\b\000\000\000\000\000\000\000\000\000\000\000\230\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\234\000\000\000\000\000\000\000\030\000\000\000\007\000\000\000\000\000\000\000\000\000\000\000\234\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\264\000\000\000\000\000\000\000\023\000\000\0001\000\000\000\000\000\000\000\000\000\000\000\264\000\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\372\000\000\000\000\000\000\000\020\000\000\000\n\000\000\000\000\000\000\000\000\000\000\000\024\001\000\000\000\000\000\000\027\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\024\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\030\001\000\000\000\000\000\000\030\000\000\000\013\000\000\000\000\000\000\000\000\000\000\000\030\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\034\001\000\000\000\000\000\000\027\000\000\000\r\000\000\000\000\000\000\000\000\000\000\000\034\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000 \001\000\000\000\000\000\000\030\000\000\000\f\000\000\000\000\000\000\000\000\000\000\000 \001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000*\001\000\000\000\000\000\000\023\000\000\0002\000\000\000\000\000\000\000\000\000\000\000*\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000B\001\000\000\000\000\000\000-\000\000\000\t\000\000\000\000\000\000\000\000\000\000\000R\001\000\000\000\000\000\000\027\000\000\000\b\000\000\000\000\000\000\000\000\000\000\000R\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000V\001\000\000\000\000\000\000\030\000\000\000\016\000\000\000\000\000\000\000\000\000\000\000V\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000Z\001\000\000\000\000\000\000\023\000\000\0003\000\000\000\000\000\000\000\000\000\000\000Z\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000n\001\000\000\000\000\000\000\027\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000n\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000r\001\000\000\000\000\000\000\030\000\000\000\017\000\000\000\000\000\000\000\000\000\000\000r\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000v\001\000\000\000\000\000\000\027\000\000\000\022\000\000\000\000\000\000\000\000\000\000\000v\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000z\001\000\000\000\000\000\000\030\000\000\000\021\000\000\000\000\000\000\000\000\000\000\000z\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000~\001\000\000\000\000\000\000\024\000\000\0004\000\000\000\000\000\000\000\000\000\000\000\202\001\000\000\000\000\000\000\030\000\000\000\023\000\000\000\000\000\000\000\000\000\000\000\202\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\222\001\000\000\000\000\000\000\023\000\000\0005\000\000\000\000\000\000\000\000\000\000\000\222\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\262\001\000\000\000\000\000\000\020\000\000\000\024\000\000\000\000\000\000\000\000\000\000\000\266\001\000\000\000\000\000\000-\000\000\000\025\000\000\000\000\000\000\000\000\000\000\000\302\001\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000\302\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\332\001\000\000\000\000\000\000\027\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\332\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\336\001\000\000\000\000\000\000\030\000\000\000\026\000\000\000\000\000\000\000\000\000\000\000\336\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\342\001\000\000\000\000\000\000\024\000\000\0004\000\000\000\000\000\000\000\000\000\000\000\346\001\000\000\000\000\000\000\030\000\000\000\027\000\000\000\000\000\000\000\000\000\000\000\346\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\352\001\000\000\000\000\000\000\023\000\000\0007\000\000\000\000\000\000\000\000\000\000\000\352\001\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\362\001\000\000\000\000\000\000-\000\000\000\025\000\000\000\000\000\000\000\000\000\000\000\024\002\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000\024\002\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000:\002\000\000\000\000\000\000\020\000\000\000\030\000\000\000\000\000\000\000\000\000\000\000\264\002\000\000\000\000\000\000\027\000\000\000\032\000\000\000\000\000\000\000\000\000\000\000\264\002\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\270\002\000\000\000\000\000\000\030\000\000\000\031\000\000\000\000\000\000\000\000\000\000\000\270\002\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\320\002\000\000\000\000\000\000\023\000\000\0001\000\000\000\000\000\000\000\000\000\000\000\320\002\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\026\003\000\000\000\000\000\000\020\000\000\000\034\000\000\000\000\000\000\000\000\000\000\000(\003\000\000\000\000\000\000\023\000\000\0008\000\000\000\000\000\000\000\000\000\000\000(\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000:\003\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000:\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000H\003\000\000\000\000\000\000\023\000\000\0009\000\000\000\000\000\000\000\000\000\000\000H\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000V\003\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000V\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000p\003\000\000\000\000\000\000-\000\000\000\033\000\000\000\000\000\000\000\000\000\000\000\200\003\000\000\000\000\000\000\027\000\000\000\032\000\000\000\000\000\000\000\000\000\000\000\200\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\204\003\000\000\000\000\000\000\030\000\000\000\035\000\000\000\000\000\000\000\000\000\000\000\204\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\210\003\000\000\000\000\000\000\023\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\210\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\234\003\000\000\000\000\000\000\027\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\234\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\240\003\000\000\000\000\000\000\030\000\000\000\036\000\000\000\000\000\000\000\000\000\000\000\240\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\244\003\000\000\000\000\000\000\027\000\000\000 \000\000\000\000\000\000\000\000\000\000\000\244\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\250\003\000\000\000\000\000\000\030\000\000\000\037\000\000\000\000\000\000\000\000\000\000\000\250\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\254\003\000\000\000\000\000\000\024\000\000\0004\000\000\000\000\000\000\000\000\000\000\000\260\003\000\000\000\000\000\000\030\000\000\000!\000\000\000\000\000\000\000\000\000\000\000\260\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\300\003\000\000\000\000\000\000\023\000\000\0005\000\000\000\000\000\000\000\000\000\000\000\300\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\340\003\000\000\000\000\000\000\020\000\000\000\"\000\000\000\000\000\000\000\000\000\000\000\344\003\000\000\000\000\000\000-\000\000\000#\000\000\000\000\000\000\000\000\000\000\000\360\003\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000\360\003\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\b\004\000\000\000\000\000\000\027\000\000\000\020\000\000\000\000\000\000\000\000\000\000\000\b\004\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\f\004\000\000\000\000\000\000\030\000\000\000$\000\000\000\000\000\000\000\000\000\000\000\f\004\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\020\004\000\000\000\000\000\000\024\000\000\0004\000\000\000\000\000\000\000\000\000\000\000\024\004\000\000\000\000\000\000\030\000\000\000%\000\000\000\000\000\000\000\000\000\000\000\024\004\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\030\004\000\000\000\000\000\000\023\000\000\0007\000\000\000\000\000\000\000\000\000\000\000\030\004\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000 \004\000\000\000\000\000\000-\000\000\000#\000\000\000\000\000\000\000\000\000\000\000B\004\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000B\004\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000h\004\000\000\000\000\000\000\020\000\000\000&\000\000\000\000\000\000\000\000\000\000\000\234\004\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000\234\004\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\326\004\000\000\000\000\000\000\023\000\000\0006\000\000\000\000\000\000\000\000\000\000\000\326\004\000\000\000\000\000\0003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\020\000\000\000\000\000\000\000\002\000\000\000'\000\000\000\000\000\000\000\000\000\000\000(\000\000\000\000\000\000\000\002\000\000\000'\000\000\000\000\000\000\000\000\000\000\000@\000\000\000\000\000\000\000\002\000\000\000'\000\000\000\000\000\000\000\000\000\000\000X\000\000\000\000\000\000\000\002\000\000\000'\000\000\000\000\000\000\000\000\000\000\000\020\000\000\000\000\000\000\000\002\000\000\000/\000\000\000\000\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000\002\000\000\000*\000\000\000\000\000\000\000\000\000\000\000/\0061\r3 572\02204\000$x\000.rela.text\000.comment\000__kmpc_end_reduce_nowait\000__kmpc_reduce_nowait\000__kmpc_fork_teams\000.riscv.attributes\000.relallvm_offload_entries\000.gomp_critical_user_.reduction.var\000.rela.data.rel.ro\000__kmpc_fork_call\000.note.GNU-stack\000__kmpc_for_static_fini\000.llvm.rodata.offloading\000.llvm_addrsig\000__floatsidf\000.offloading.entry_name\000__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined\000__omp_offloading_802_5ae8f81_main_l9.omp_outlined\000$d\000__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func\000__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func\000not-basic.c\000.strtab\000.symtab\000.offloading.entry.__omp_offloading_802_5ae8f81_main_l9\000.Lpcrel_hi9\000.Lpcrel_hi8\000.LBB2_8\000.Lpcrel_hi7\000.LBB1_7\000.Lpcrel_hi17\000.Lpcrel_hi6\000.Lpcrel_hi16\000.Lpcrel_hi5\000.L__unnamed_5\000.Lpcrel_hi15\000.LBB2_15\000.Lpcrel_hi4\000__kmpc_for_static_init_4\000.L__unnamed_4\000.LBB2_4\000.LBB1_4\000.Lpcrel_hi14\000.LBB1_14\000.Lpcrel_hi3\000__divdf3\000__adddf3\000.L__unnamed_3\000.Lpcrel_hi13\000.LBB2_13\000.Lpcrel_hi2\000.L__unnamed_2\000.Lpcrel_hi12\000.LBB2_12\000.LBB1_12\000.Lpcrel_hi1\000.L__unnamed_1\000.Lpcrel_hi11\000.LBB1_11\000.rodata.str1.1\000.Lpcrel_hi0\000.Lpcrel_hi10\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000e\002\000\000\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000,\030\000\000\000\000\000\000b\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\t\000\000\000\001\000\000\000\006\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000@\000\000\000\000\000\000\000\354\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\004\000\000\000\004\000\000\000@\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000P\f\000\000\000\000\000\000@\013\000\000\000\000\000\000\016\000\000\000\002\000\000\000\b\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000:\004\000\000\001\000\000\0002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000,\005\000\000\000\000\000\000\027\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\254\000\000\000\001\000\000\000\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000H\005\000\000\000\000\000\000`\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\b\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\247\000\000\000\004\000\000\000@\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\220\027\000\000\000\000\000\000`\000\000\000\000\000\000\000\016\000\000\000\005\000\000\000\b\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000\361\000\000\000\001\000\000\0002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\250\005\000\000\000\000\000\000%\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000o\000\000\000\001\000\000\000\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\320\005\000\000\000\000\000\0008\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\b\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000j\000\000\000\004\000\000\000@\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\360\027\000\000\000\000\000\0000\000\000\000\000\000\000\000\016\000\000\000\b\000\000\000\b\000\000\000\000\000\000\000\030\000\000\000\000\000\000\000\017\000\000\000\001\000\000\0000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\b\006\000\000\000\000\000\000p\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\312\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000x\006\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000X\000\000\000\003\000\000p\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000x\006\000\000\000\000\000\000N\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\t\001\000\000\003L\377o\000\000\000\200\000\000\000\000\000\000\000\000\000\000\000\000 \030\000\000\000\000\000\000\f\000\000\000\000\000\000\000\016\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000m\002\000\000\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\310\006\000\000\000\000\000\000\210\005\000\000\000\000\000\000\001\000\000\000/\000\000\000\b\000\000\000\000\000\000\000\030\000\000\000\000\000\000"
	.size	.Lllvm.embedded.object, 8432

	.ident	"clang version 21.0.0git (https://github.com/inspiresemi/llvm-project bd3b6daacb1382591fbeeef56a50c471709dbc2b)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __omp_offloading_802_5ae8f81_main_l9
	.addrsig_sym __omp_offloading_802_5ae8f81_main_l9.omp_outlined
	.addrsig_sym __kmpc_for_static_init_4
	.addrsig_sym __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined
	.addrsig_sym __kmpc_for_static_fini
	.addrsig_sym __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func
	.addrsig_sym __kmpc_reduce_nowait
	.addrsig_sym __kmpc_end_reduce_nowait
	.addrsig_sym __kmpc_fork_call
	.addrsig_sym __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func
	.addrsig_sym __kmpc_fork_teams
	.addrsig_sym __tgt_target_kernel
	.addrsig_sym main.omp_outlined
	.addrsig_sym __kmpc_end_single
	.addrsig_sym __kmpc_single
	.addrsig_sym omp_get_num_threads
	.addrsig_sym __kmpc_barrier
	.addrsig_sym printf
	.addrsig_sym .gomp_critical_user_.reduction.var
	.addrsig_sym .__omp_offloading_802_5ae8f81_main_l9.region_id
