	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_a2p1_c2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0"
	.file	"not-basic.c"
	.text
	.protected	__omp_offloading_802_5ae8f81_main_l9 # -- Begin function __omp_offloading_802_5ae8f81_main_l9
	.weak	__omp_offloading_802_5ae8f81_main_l9
	.p2align	1
	.type	__omp_offloading_802_5ae8f81_main_l9,@function
__omp_offloading_802_5ae8f81_main_l9:   # @__omp_offloading_802_5ae8f81_main_l9
# %bb.0:                                # %entry
	addi	sp, sp, -48
	sd	ra, 40(sp)                      # 8-byte Folded Spill
	sd	s0, 32(sp)                      # 8-byte Folded Spill
	addi	s0, sp, 48
	sd	a0, -24(s0)
	sd	a1, -32(s0)
	sd	a2, -40(s0)
	ld	a4, -40(s0)
	lw	a0, -32(s0)
	sw	a0, -48(s0)
	ld	a3, -48(s0)
.Lpcrel_hi0:
	auipc	a0, %pcrel_hi(.L__unnamed_1)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi0)
.Lpcrel_hi1:
	auipc	a1, %pcrel_hi(__omp_offloading_802_5ae8f81_main_l9.omp_outlined)
	addi	a2, a1, %pcrel_lo(.Lpcrel_hi1)
	li	a1, 2
	call	__kmpc_fork_teams
	ld	ra, 40(sp)                      # 8-byte Folded Reload
	ld	s0, 32(sp)                      # 8-byte Folded Reload
	addi	sp, sp, 48
	ret
.Lfunc_end0:
	.size	__omp_offloading_802_5ae8f81_main_l9, .Lfunc_end0-__omp_offloading_802_5ae8f81_main_l9
                                        # -- End function
	.p2align	1                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined
# %bb.0:                                # %entry
	addi	sp, sp, -192
	sd	ra, 184(sp)                     # 8-byte Folded Spill
	sd	s0, 176(sp)                     # 8-byte Folded Spill
	addi	s0, sp, 192
	sd	a0, -24(s0)
	sd	a1, -32(s0)
	sd	a2, -40(s0)
	sd	a3, -48(s0)
	ld	a0, -48(s0)
	sd	a0, -128(s0)                    # 8-byte Folded Spill
	li	a0, 0
	sd	a0, -56(s0)
	sw	a0, -68(s0)
	lui	a1, 256
	sd	a1, -112(s0)                    # 8-byte Folded Spill
	addi	a1, a1, -1
	sw	a1, -72(s0)
	li	a7, 1
	sw	a7, -76(s0)
	sw	a0, -80(s0)
	ld	a0, -24(s0)
	lw	a1, 0(a0)
	sd	a1, -120(s0)                    # 8-byte Folded Spill
	mv	a0, sp
	sd	a7, 0(a0)
.Lpcrel_hi2:
	auipc	a0, %pcrel_hi(.L__unnamed_2)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi2)
	li	a2, 92
	addi	a3, s0, -80
	addi	a4, s0, -68
	addi	a5, s0, -72
	addi	a6, s0, -76
	call	__kmpc_for_static_init_4
	ld	a1, -112(s0)                    # 8-byte Folded Reload
	lw	a0, -72(s0)
	blt	a0, a1, .LBB1_2
	j	.LBB1_1
.LBB1_1:                                # %cond.true
	lui	a0, 256
	addi	a0, a0, -1
	sd	a0, -136(s0)                    # 8-byte Folded Spill
	j	.LBB1_3
.LBB1_2:                                # %cond.false
	lw	a0, -72(s0)
	sd	a0, -136(s0)                    # 8-byte Folded Spill
	j	.LBB1_3
.LBB1_3:                                # %cond.end
	ld	a0, -136(s0)                    # 8-byte Folded Reload
	sw	a0, -72(s0)
	lw	a0, -68(s0)
	sw	a0, -60(s0)
	j	.LBB1_4
.LBB1_4:                                # %omp.inner.for.cond
                                        # =>This Inner Loop Header: Depth=1
	lw	a1, -60(s0)
	lw	a0, -72(s0)
	blt	a0, a1, .LBB1_7
	j	.LBB1_5
.LBB1_5:                                # %omp.inner.for.body
                                        #   in Loop: Header=BB1_4 Depth=1
	lwu	a3, -68(s0)
	lwu	a4, -72(s0)
	lw	a0, -40(s0)
	sw	a0, -96(s0)
	ld	a5, -96(s0)
.Lpcrel_hi3:
	auipc	a0, %pcrel_hi(.L__unnamed_1)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi3)
.Lpcrel_hi4:
	auipc	a1, %pcrel_hi(__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined)
	addi	a2, a1, %pcrel_lo(.Lpcrel_hi4)
	li	a1, 4
	addi	a6, s0, -56
	call	__kmpc_fork_call
	j	.LBB1_6
.LBB1_6:                                # %omp.inner.for.inc
                                        #   in Loop: Header=BB1_4 Depth=1
	lw	a0, -60(s0)
	lw	a1, -76(s0)
	addw	a0, a0, a1
	sw	a0, -60(s0)
	j	.LBB1_4
.LBB1_7:                                # %omp.inner.for.end
	j	.LBB1_8
.LBB1_8:                                # %omp.loop.exit
	ld	a0, -120(s0)                    # 8-byte Folded Reload
	sext.w	a1, a0
	sd	a1, -160(s0)                    # 8-byte Folded Spill
.Lpcrel_hi5:
	auipc	a0, %pcrel_hi(.L__unnamed_2)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi5)
	call	__kmpc_for_static_fini
	ld	a1, -160(s0)                    # 8-byte Folded Reload
	addi	a0, s0, -56
	sd	a0, -104(s0)
.Lpcrel_hi6:
	auipc	a0, %pcrel_hi(.L__unnamed_3)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi6)
.Lpcrel_hi7:
	auipc	a2, %pcrel_hi(__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func)
	addi	a5, a2, %pcrel_lo(.Lpcrel_hi7)
.Lpcrel_hi8:
	auipc	a2, %got_pcrel_hi(.gomp_critical_user_.reduction.var)
	ld	a6, %pcrel_lo(.Lpcrel_hi8)(a2)
	li	a2, 1
	sd	a2, -152(s0)                    # 8-byte Folded Spill
	li	a3, 8
	addi	a4, s0, -104
	call	__kmpc_reduce_nowait
	ld	a1, -152(s0)                    # 8-byte Folded Reload
	mv	a2, a0
	sd	a2, -144(s0)                    # 8-byte Folded Spill
	beq	a0, a1, .LBB1_10
	j	.LBB1_9
.LBB1_9:                                # %omp.loop.exit
	ld	a0, -144(s0)                    # 8-byte Folded Reload
	sext.w	a0, a0
	li	a1, 2
	beq	a0, a1, .LBB1_11
	j	.LBB1_14
.LBB1_10:                               # %.omp.reduction.case1
	ld	a0, -128(s0)                    # 8-byte Folded Reload
	ld	a0, 0(a0)
	ld	a1, -56(s0)
	call	__adddf3
	ld	a2, -128(s0)                    # 8-byte Folded Reload
	mv	a1, a0
	ld	a0, -120(s0)                    # 8-byte Folded Reload
	sd	a1, 0(a2)
	sext.w	a1, a0
.Lpcrel_hi9:
	auipc	a0, %pcrel_hi(.L__unnamed_3)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi9)
.Lpcrel_hi10:
	auipc	a2, %got_pcrel_hi(.gomp_critical_user_.reduction.var)
	ld	a2, %pcrel_lo(.Lpcrel_hi10)(a2)
	call	__kmpc_end_reduce_nowait
	j	.LBB1_14
.LBB1_11:                               # %.omp.reduction.case2
	ld	a0, -128(s0)                    # 8-byte Folded Reload
	ld	a1, -56(s0)
	sd	a1, -176(s0)                    # 8-byte Folded Spill
	ld	a0, 0(a0)
	sd	a0, -168(s0)                    # 8-byte Folded Spill
	j	.LBB1_12
.LBB1_12:                               # %atomicrmw.start
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB1_15 Depth 2
	ld	a0, -168(s0)                    # 8-byte Folded Reload
	ld	a1, -176(s0)                    # 8-byte Folded Reload
	sd	a0, -184(s0)                    # 8-byte Folded Spill
	call	__adddf3
	ld	a3, -128(s0)                    # 8-byte Folded Reload
	ld	a1, -184(s0)                    # 8-byte Folded Reload
	mv	a4, a0
.LBB1_15:                               # %atomicrmw.start
                                        #   Parent Loop BB1_12 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	lr.d	a0, (a3)
	bne	a0, a1, .LBB1_17
# %bb.16:                               # %atomicrmw.start
                                        #   in Loop: Header=BB1_15 Depth=2
	sc.d	a2, a4, (a3)
	bnez	a2, .LBB1_15
.LBB1_17:                               # %atomicrmw.start
                                        #   in Loop: Header=BB1_12 Depth=1
	mv	a2, a0
	sd	a2, -168(s0)                    # 8-byte Folded Spill
	bne	a0, a1, .LBB1_12
	j	.LBB1_13
.LBB1_13:                               # %atomicrmw.end
	j	.LBB1_14
.LBB1_14:                               # %.omp.reduction.default
	ld	ra, 184(sp)                     # 8-byte Folded Reload
	ld	s0, 176(sp)                     # 8-byte Folded Reload
	addi	sp, sp, 192
	ret
.Lfunc_end1:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined, .Lfunc_end1-__omp_offloading_802_5ae8f81_main_l9.omp_outlined
                                        # -- End function
	.p2align	1                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined
# %bb.0:                                # %entry
	addi	sp, sp, -208
	sd	ra, 200(sp)                     # 8-byte Folded Spill
	sd	s0, 192(sp)                     # 8-byte Folded Spill
	addi	s0, sp, 208
	sd	a0, -24(s0)
	sd	a1, -32(s0)
	sd	a2, -40(s0)
	sd	a3, -48(s0)
	sd	a4, -56(s0)
	sd	a5, -64(s0)
	ld	a0, -64(s0)
	sd	a0, -136(s0)                    # 8-byte Folded Spill
	li	a0, 0
	sw	a0, -76(s0)
	lui	a1, 256
	sd	a1, -120(s0)                    # 8-byte Folded Spill
	addi	a1, a1, -1
	sw	a1, -80(s0)
	ld	a2, -40(s0)
	ld	a1, -48(s0)
	sw	a2, -76(s0)
	sw	a1, -80(s0)
	li	a7, 1
	sw	a7, -84(s0)
	sw	a0, -88(s0)
	sd	a0, -96(s0)
	ld	a0, -24(s0)
	lw	a1, 0(a0)
	sd	a1, -128(s0)                    # 8-byte Folded Spill
	mv	a0, sp
	sd	a7, 0(a0)
.Lpcrel_hi11:
	auipc	a0, %pcrel_hi(.L__unnamed_4)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi11)
	li	a2, 34
	addi	a3, s0, -88
	addi	a4, s0, -76
	addi	a5, s0, -80
	addi	a6, s0, -84
	call	__kmpc_for_static_init_4
	ld	a1, -120(s0)                    # 8-byte Folded Reload
	lw	a0, -80(s0)
	blt	a0, a1, .LBB2_2
	j	.LBB2_1
.LBB2_1:                                # %cond.true
	lui	a0, 256
	addi	a0, a0, -1
	sd	a0, -144(s0)                    # 8-byte Folded Spill
	j	.LBB2_3
.LBB2_2:                                # %cond.false
	lw	a0, -80(s0)
	sd	a0, -144(s0)                    # 8-byte Folded Spill
	j	.LBB2_3
.LBB2_3:                                # %cond.end
	ld	a0, -144(s0)                    # 8-byte Folded Reload
	sw	a0, -80(s0)
	lw	a0, -76(s0)
	sw	a0, -68(s0)
	j	.LBB2_4
.LBB2_4:                                # %omp.inner.for.cond
                                        # =>This Inner Loop Header: Depth=1
	lw	a1, -68(s0)
	lw	a0, -80(s0)
	blt	a0, a1, .LBB2_8
	j	.LBB2_5
.LBB2_5:                                # %omp.inner.for.body
                                        #   in Loop: Header=BB2_4 Depth=1
	lw	a0, -68(s0)
	sw	a0, -100(s0)
	lw	a0, -100(s0)
	call	__floatsidf
	li	a1, 1023
	slli	a1, a1, 52
	sd	a1, -152(s0)                    # 8-byte Folded Spill
	call	__adddf3
	mv	a1, a0
	ld	a0, -152(s0)                    # 8-byte Folded Reload
	call	__divdf3
	mv	a1, a0
	ld	a0, -96(s0)
	call	__adddf3
	sd	a0, -96(s0)
	j	.LBB2_6
.LBB2_6:                                # %omp.body.continue
                                        #   in Loop: Header=BB2_4 Depth=1
	j	.LBB2_7
.LBB2_7:                                # %omp.inner.for.inc
                                        #   in Loop: Header=BB2_4 Depth=1
	lw	a0, -68(s0)
	addiw	a0, a0, 1
	sw	a0, -68(s0)
	j	.LBB2_4
.LBB2_8:                                # %omp.inner.for.end
	j	.LBB2_9
.LBB2_9:                                # %omp.loop.exit
	ld	a0, -128(s0)                    # 8-byte Folded Reload
	sext.w	a1, a0
	sd	a1, -176(s0)                    # 8-byte Folded Spill
.Lpcrel_hi12:
	auipc	a0, %pcrel_hi(.L__unnamed_4)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi12)
	call	__kmpc_for_static_fini
	ld	a1, -176(s0)                    # 8-byte Folded Reload
	addi	a0, s0, -96
	sd	a0, -112(s0)
.Lpcrel_hi13:
	auipc	a0, %pcrel_hi(.L__unnamed_3)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi13)
.Lpcrel_hi14:
	auipc	a2, %pcrel_hi(__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func)
	addi	a5, a2, %pcrel_lo(.Lpcrel_hi14)
.Lpcrel_hi15:
	auipc	a2, %got_pcrel_hi(.gomp_critical_user_.reduction.var)
	ld	a6, %pcrel_lo(.Lpcrel_hi15)(a2)
	li	a2, 1
	sd	a2, -168(s0)                    # 8-byte Folded Spill
	li	a3, 8
	addi	a4, s0, -112
	call	__kmpc_reduce_nowait
	ld	a1, -168(s0)                    # 8-byte Folded Reload
	mv	a2, a0
	sd	a2, -160(s0)                    # 8-byte Folded Spill
	beq	a0, a1, .LBB2_11
	j	.LBB2_10
.LBB2_10:                               # %omp.loop.exit
	ld	a0, -160(s0)                    # 8-byte Folded Reload
	sext.w	a0, a0
	li	a1, 2
	beq	a0, a1, .LBB2_12
	j	.LBB2_15
.LBB2_11:                               # %.omp.reduction.case1
	ld	a0, -136(s0)                    # 8-byte Folded Reload
	ld	a0, 0(a0)
	ld	a1, -96(s0)
	call	__adddf3
	ld	a2, -136(s0)                    # 8-byte Folded Reload
	mv	a1, a0
	ld	a0, -128(s0)                    # 8-byte Folded Reload
	sd	a1, 0(a2)
	sext.w	a1, a0
.Lpcrel_hi16:
	auipc	a0, %pcrel_hi(.L__unnamed_3)
	addi	a0, a0, %pcrel_lo(.Lpcrel_hi16)
.Lpcrel_hi17:
	auipc	a2, %got_pcrel_hi(.gomp_critical_user_.reduction.var)
	ld	a2, %pcrel_lo(.Lpcrel_hi17)(a2)
	call	__kmpc_end_reduce_nowait
	j	.LBB2_15
.LBB2_12:                               # %.omp.reduction.case2
	ld	a0, -136(s0)                    # 8-byte Folded Reload
	ld	a1, -96(s0)
	sd	a1, -192(s0)                    # 8-byte Folded Spill
	ld	a0, 0(a0)
	sd	a0, -184(s0)                    # 8-byte Folded Spill
	j	.LBB2_13
.LBB2_13:                               # %atomicrmw.start
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB2_16 Depth 2
	ld	a0, -184(s0)                    # 8-byte Folded Reload
	ld	a1, -192(s0)                    # 8-byte Folded Reload
	sd	a0, -200(s0)                    # 8-byte Folded Spill
	call	__adddf3
	ld	a3, -136(s0)                    # 8-byte Folded Reload
	ld	a1, -200(s0)                    # 8-byte Folded Reload
	mv	a4, a0
.LBB2_16:                               # %atomicrmw.start
                                        #   Parent Loop BB2_13 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	lr.d	a0, (a3)
	bne	a0, a1, .LBB2_18
# %bb.17:                               # %atomicrmw.start
                                        #   in Loop: Header=BB2_16 Depth=2
	sc.d	a2, a4, (a3)
	bnez	a2, .LBB2_16
.LBB2_18:                               # %atomicrmw.start
                                        #   in Loop: Header=BB2_13 Depth=1
	mv	a2, a0
	sd	a2, -184(s0)                    # 8-byte Folded Spill
	bne	a0, a1, .LBB2_13
	j	.LBB2_14
.LBB2_14:                               # %atomicrmw.end
	j	.LBB2_15
.LBB2_15:                               # %.omp.reduction.default
	ld	ra, 200(sp)                     # 8-byte Folded Reload
	ld	s0, 192(sp)                     # 8-byte Folded Reload
	addi	sp, sp, 208
	ret
.Lfunc_end2:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined, .Lfunc_end2-__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined
                                        # -- End function
	.p2align	1                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func
# %bb.0:                                # %entry
	addi	sp, sp, -48
	sd	ra, 40(sp)                      # 8-byte Folded Spill
	sd	s0, 32(sp)                      # 8-byte Folded Spill
	addi	s0, sp, 48
	sd	a0, -24(s0)
	sd	a1, -32(s0)
	ld	a0, -24(s0)
	ld	a1, -32(s0)
	ld	a1, 0(a1)
	ld	a0, 0(a0)
	sd	a0, -40(s0)                     # 8-byte Folded Spill
	ld	a0, 0(a0)
	ld	a1, 0(a1)
	call	__adddf3
	ld	a1, -40(s0)                     # 8-byte Folded Reload
	sd	a0, 0(a1)
	ld	ra, 40(sp)                      # 8-byte Folded Reload
	ld	s0, 32(sp)                      # 8-byte Folded Reload
	addi	sp, sp, 48
	ret
.Lfunc_end3:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func, .Lfunc_end3-__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp_outlined.omp.reduction.reduction_func
                                        # -- End function
	.p2align	1                               # -- Begin function __omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func
	.type	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func,@function
__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func: # @__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func
# %bb.0:                                # %entry
	addi	sp, sp, -48
	sd	ra, 40(sp)                      # 8-byte Folded Spill
	sd	s0, 32(sp)                      # 8-byte Folded Spill
	addi	s0, sp, 48
	sd	a0, -24(s0)
	sd	a1, -32(s0)
	ld	a0, -24(s0)
	ld	a1, -32(s0)
	ld	a1, 0(a1)
	ld	a0, 0(a0)
	sd	a0, -40(s0)                     # 8-byte Folded Spill
	ld	a0, 0(a0)
	ld	a1, 0(a1)
	call	__adddf3
	ld	a1, -40(s0)                     # 8-byte Folded Reload
	sd	a0, 0(a1)
	ld	ra, 40(sp)                      # 8-byte Folded Reload
	ld	s0, 32(sp)                      # 8-byte Folded Reload
	addi	sp, sp, 48
	ret
.Lfunc_end4:
	.size	__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func, .Lfunc_end4-__omp_offloading_802_5ae8f81_main_l9.omp_outlined.omp.reduction.reduction_func
                                        # -- End function
	.type	.L__unnamed_5,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_5:
	.asciz	";unknown;unknown;0;0;;"
	.size	.L__unnamed_5, 23

	.type	.L__unnamed_2,@object           # @1
	.section	.data.rel.ro,"aw",@progbits
	.p2align	3, 0x0
.L__unnamed_2:
	.word	0                               # 0x0
	.word	2050                            # 0x802
	.word	0                               # 0x0
	.word	22                              # 0x16
	.quad	.L__unnamed_5
	.size	.L__unnamed_2, 24

	.type	.L__unnamed_4,@object           # @2
	.p2align	3, 0x0
.L__unnamed_4:
	.word	0                               # 0x0
	.word	514                             # 0x202
	.word	0                               # 0x0
	.word	22                              # 0x16
	.quad	.L__unnamed_5
	.size	.L__unnamed_4, 24

	.type	.gomp_critical_user_.reduction.var,@object # @.gomp_critical_user_.reduction.var
	.comm	.gomp_critical_user_.reduction.var,32,8
	.type	.L__unnamed_3,@object           # @3
	.p2align	3, 0x0
.L__unnamed_3:
	.word	0                               # 0x0
	.word	18                              # 0x12
	.word	0                               # 0x0
	.word	22                              # 0x16
	.quad	.L__unnamed_5
	.size	.L__unnamed_3, 24

	.type	.L__unnamed_1,@object           # @4
	.p2align	3, 0x0
.L__unnamed_1:
	.word	0                               # 0x0
	.word	2                               # 0x2
	.word	0                               # 0x0
	.word	22                              # 0x16
	.quad	.L__unnamed_5
	.size	.L__unnamed_1, 24

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
	.half	1                               # 0x1
	.half	1                               # 0x1
	.word	0                               # 0x0
	.quad	__omp_offloading_802_5ae8f81_main_l9
	.quad	.offloading.entry_name
	.quad	0                               # 0x0
	.quad	0                               # 0x0
	.quad	0
	.size	.offloading.entry.__omp_offloading_802_5ae8f81_main_l9, 56

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
	.addrsig_sym .gomp_critical_user_.reduction.var
