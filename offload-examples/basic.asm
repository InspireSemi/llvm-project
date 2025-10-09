
basic.elf:	file format elf64-x86-64

Disassembly of section .init:

0000000000001000 <_init>:
    1000: f3 0f 1e fa                  	endbr64
    1004: 48 83 ec 08                  	subq	$0x8, %rsp
    1008: 48 8b 05 c1 2f 00 00         	movq	0x2fc1(%rip), %rax      # 0x3fd0 <printf@GLIBC_2.2.5+0x3fd0>
    100f: 48 85 c0                     	testq	%rax, %rax
    1012: 74 02                        	je	0x1016 <_init+0x16>
    1014: ff d0                        	callq	*%rax
    1016: 48 83 c4 08                  	addq	$0x8, %rsp
    101a: c3                           	retq

Disassembly of section .plt:

0000000000001020 <.plt>:
    1020: ff 35 ca 2f 00 00            	pushq	0x2fca(%rip)            # 0x3ff0 <_GLOBAL_OFFSET_TABLE_+0x8>
    1026: ff 25 cc 2f 00 00            	jmpq	*0x2fcc(%rip)           # 0x3ff8 <_GLOBAL_OFFSET_TABLE_+0x10>
    102c: 0f 1f 40 00                  	nopl	(%rax)

0000000000001030 <printf@plt>:
    1030: ff 25 ca 2f 00 00            	jmpq	*0x2fca(%rip)           # 0x4000 <_GLOBAL_OFFSET_TABLE_+0x18>
    1036: 68 00 00 00 00               	pushq	$0x0
    103b: e9 e0 ff ff ff               	jmp	0x1020 <.plt>

0000000000001040 <__tgt_target_kernel@plt>:
    1040: ff 25 c2 2f 00 00            	jmpq	*0x2fc2(%rip)           # 0x4008 <_GLOBAL_OFFSET_TABLE_+0x20>
    1046: 68 01 00 00 00               	pushq	$0x1
    104b: e9 d0 ff ff ff               	jmp	0x1020 <.plt>

0000000000001050 <__cxa_atexit@plt>:
    1050: ff 25 ba 2f 00 00            	jmpq	*0x2fba(%rip)           # 0x4010 <_GLOBAL_OFFSET_TABLE_+0x28>
    1056: 68 02 00 00 00               	pushq	$0x2
    105b: e9 c0 ff ff ff               	jmp	0x1020 <.plt>

0000000000001060 <__tgt_unregister_lib@plt>:
    1060: ff 25 b2 2f 00 00            	jmpq	*0x2fb2(%rip)           # 0x4018 <_GLOBAL_OFFSET_TABLE_+0x30>
    1066: 68 03 00 00 00               	pushq	$0x3
    106b: e9 b0 ff ff ff               	jmp	0x1020 <.plt>

0000000000001070 <__tgt_register_lib@plt>:
    1070: ff 25 aa 2f 00 00            	jmpq	*0x2faa(%rip)           # 0x4020 <_GLOBAL_OFFSET_TABLE_+0x38>
    1076: 68 04 00 00 00               	pushq	$0x4
    107b: e9 a0 ff ff ff               	jmp	0x1020 <.plt>

Disassembly of section .plt.got:

0000000000001080 <__cxa_finalize@plt>:
    1080: ff 25 32 2f 00 00            	jmpq	*0x2f32(%rip)           # 0x3fb8 <printf@GLIBC_2.2.5+0x3fb8>
    1086: 66 90                        	nop

Disassembly of section .text:

0000000000001090 <.omp_offloading.descriptor_reg>:
    1090: 50                           	pushq	%rax
    1091: 48 8d 3d e8 2c 00 00         	leaq	0x2ce8(%rip), %rdi      # 0x3d80 <.omp_offloading.descriptor>
    1098: e8 d3 ff ff ff               	callq	0x1070 <__tgt_register_lib@plt>
    109d: 48 8d 3d 0c 00 00 00         	leaq	0xc(%rip), %rdi         # 0x10b0 <.omp_offloading.descriptor_unreg>
    10a4: e8 37 02 00 00               	callq	0x12e0 <atexit>
    10a9: 58                           	popq	%rax
    10aa: c3                           	retq
    10ab: 0f 1f 44 00 00               	nopl	(%rax,%rax)

00000000000010b0 <.omp_offloading.descriptor_unreg>:
    10b0: 50                           	pushq	%rax
    10b1: 48 8d 3d c8 2c 00 00         	leaq	0x2cc8(%rip), %rdi      # 0x3d80 <.omp_offloading.descriptor>
    10b8: e8 a3 ff ff ff               	callq	0x1060 <__tgt_unregister_lib@plt>
    10bd: 58                           	popq	%rax
    10be: c3                           	retq
    10bf: 90                           	nop

00000000000010c0 <_start>:
    10c0: f3 0f 1e fa                  	endbr64
    10c4: 31 ed                        	xorl	%ebp, %ebp
    10c6: 49 89 d1                     	movq	%rdx, %r9
    10c9: 5e                           	popq	%rsi
    10ca: 48 89 e2                     	movq	%rsp, %rdx
    10cd: 48 83 e4 f0                  	andq	$-0x10, %rsp
    10d1: 50                           	pushq	%rax
    10d2: 54                           	pushq	%rsp
    10d3: 45 31 c0                     	xorl	%r8d, %r8d
    10d6: 31 c9                        	xorl	%ecx, %ecx
    10d8: 48 8d 3d d1 00 00 00         	leaq	0xd1(%rip), %rdi        # 0x11b0 <main>
    10df: ff 15 db 2e 00 00            	callq	*0x2edb(%rip)           # 0x3fc0 <printf@GLIBC_2.2.5+0x3fc0>
    10e5: f4                           	hlt
    10e6: 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)

00000000000010f0 <deregister_tm_clones>:
    10f0: 48 8d 3d 79 2f 00 00         	leaq	0x2f79(%rip), %rdi      # 0x4070 <completed.0>
    10f7: 48 8d 05 72 2f 00 00         	leaq	0x2f72(%rip), %rax      # 0x4070 <completed.0>
    10fe: 48 39 f8                     	cmpq	%rdi, %rax
    1101: 74 15                        	je	0x1118 <deregister_tm_clones+0x28>
    1103: 48 8b 05 be 2e 00 00         	movq	0x2ebe(%rip), %rax      # 0x3fc8 <printf@GLIBC_2.2.5+0x3fc8>
    110a: 48 85 c0                     	testq	%rax, %rax
    110d: 74 09                        	je	0x1118 <deregister_tm_clones+0x28>
    110f: ff e0                        	jmpq	*%rax
    1111: 0f 1f 80 00 00 00 00         	nopl	(%rax)
    1118: c3                           	retq
    1119: 0f 1f 80 00 00 00 00         	nopl	(%rax)

0000000000001120 <register_tm_clones>:
    1120: 48 8d 3d 49 2f 00 00         	leaq	0x2f49(%rip), %rdi      # 0x4070 <completed.0>
    1127: 48 8d 35 42 2f 00 00         	leaq	0x2f42(%rip), %rsi      # 0x4070 <completed.0>
    112e: 48 29 fe                     	subq	%rdi, %rsi
    1131: 48 89 f0                     	movq	%rsi, %rax
    1134: 48 c1 ee 3f                  	shrq	$0x3f, %rsi
    1138: 48 c1 f8 03                  	sarq	$0x3, %rax
    113c: 48 01 c6                     	addq	%rax, %rsi
    113f: 48 d1 fe                     	sarq	%rsi
    1142: 74 14                        	je	0x1158 <register_tm_clones+0x38>
    1144: 48 8b 05 8d 2e 00 00         	movq	0x2e8d(%rip), %rax      # 0x3fd8 <printf@GLIBC_2.2.5+0x3fd8>
    114b: 48 85 c0                     	testq	%rax, %rax
    114e: 74 08                        	je	0x1158 <register_tm_clones+0x38>
    1150: ff e0                        	jmpq	*%rax
    1152: 66 0f 1f 44 00 00            	nopw	(%rax,%rax)
    1158: c3                           	retq
    1159: 0f 1f 80 00 00 00 00         	nopl	(%rax)

0000000000001160 <__do_global_dtors_aux>:
    1160: f3 0f 1e fa                  	endbr64
    1164: 80 3d 05 2f 00 00 00         	cmpb	$0x0, 0x2f05(%rip)      # 0x4070 <completed.0>
    116b: 75 2b                        	jne	0x1198 <__do_global_dtors_aux+0x38>
    116d: 55                           	pushq	%rbp
    116e: 48 83 3d 42 2e 00 00 00      	cmpq	$0x0, 0x2e42(%rip)      # 0x3fb8 <printf@GLIBC_2.2.5+0x3fb8>
    1176: 48 89 e5                     	movq	%rsp, %rbp
    1179: 74 0c                        	je	0x1187 <__do_global_dtors_aux+0x27>
    117b: 48 8b 3d ae 2e 00 00         	movq	0x2eae(%rip), %rdi      # 0x4030 <__dso_handle>
    1182: e8 f9 fe ff ff               	callq	0x1080 <__cxa_finalize@plt>
    1187: e8 64 ff ff ff               	callq	0x10f0 <deregister_tm_clones>
    118c: c6 05 dd 2e 00 00 01         	movb	$0x1, 0x2edd(%rip)      # 0x4070 <completed.0>
    1193: 5d                           	popq	%rbp
    1194: c3                           	retq
    1195: 0f 1f 00                     	nopl	(%rax)
    1198: c3                           	retq
    1199: 0f 1f 80 00 00 00 00         	nopl	(%rax)

00000000000011a0 <frame_dummy>:
    11a0: f3 0f 1e fa                  	endbr64
    11a4: e9 77 ff ff ff               	jmp	0x1120 <register_tm_clones>
    11a9: 0f 1f 80 00 00 00 00         	nopl	(%rax)

00000000000011b0 <main>:
    11b0: 55                           	pushq	%rbp
    11b1: 48 89 e5                     	movq	%rsp, %rbp
    11b4: 48 81 ec 90 00 00 00         	subq	$0x90, %rsp
    11bb: c7 45 fc 00 00 00 00         	movl	$0x0, -0x4(%rbp)
    11c2: c7 45 f8 00 00 00 00         	movl	$0x0, -0x8(%rbp)
    11c9: 48 8d 45 f8                  	leaq	-0x8(%rbp), %rax
    11cd: 48 89 45 f0                  	movq	%rax, -0x10(%rbp)
    11d1: 48 89 45 e8                  	movq	%rax, -0x18(%rbp)
    11d5: 48 c7 45 e0 00 00 00 00      	movq	$0x0, -0x20(%rbp)
    11dd: c7 85 78 ff ff ff 03 00 00 00	movl	$0x3, -0x88(%rbp)
    11e7: c7 85 7c ff ff ff 01 00 00 00	movl	$0x1, -0x84(%rbp)
    11f1: 48 8d 45 f0                  	leaq	-0x10(%rbp), %rax
    11f5: 48 89 45 80                  	movq	%rax, -0x80(%rbp)
    11f9: 48 8d 45 e8                  	leaq	-0x18(%rbp), %rax
    11fd: 48 89 45 88                  	movq	%rax, -0x78(%rbp)
    1201: 48 8d 05 00 0e 00 00         	leaq	0xe00(%rip), %rax       # 0x2008 <.__omp_offloading_802_5ae8e7c_main_l7.region_id+0x4>
    1208: 48 89 45 90                  	movq	%rax, -0x70(%rbp)
    120c: 48 8d 05 fd 0d 00 00         	leaq	0xdfd(%rip), %rax       # 0x2010 <.__omp_offloading_802_5ae8e7c_main_l7.region_id+0xc>
    1213: 48 89 45 98                  	movq	%rax, -0x68(%rbp)
    1217: 48 c7 45 a0 00 00 00 00      	movq	$0x0, -0x60(%rbp)
    121f: 48 c7 45 a8 00 00 00 00      	movq	$0x0, -0x58(%rbp)
    1227: 48 c7 45 b0 00 00 00 00      	movq	$0x0, -0x50(%rbp)
    122f: 48 c7 45 b8 00 00 00 00      	movq	$0x0, -0x48(%rbp)
    1237: c7 45 c8 00 00 00 00         	movl	$0x0, -0x38(%rbp)
    123e: c7 45 c4 00 00 00 00         	movl	$0x0, -0x3c(%rbp)
    1245: c7 45 c0 ff ff ff ff         	movl	$0xffffffff, -0x40(%rbp) # imm = 0xFFFFFFFF
    124c: c7 45 d4 00 00 00 00         	movl	$0x0, -0x2c(%rbp)
    1253: c7 45 d0 00 00 00 00         	movl	$0x0, -0x30(%rbp)
    125a: c7 45 cc 00 00 00 00         	movl	$0x0, -0x34(%rbp)
    1261: c7 45 d8 00 00 00 00         	movl	$0x0, -0x28(%rbp)
    1268: 48 8d 3d 31 2b 00 00         	leaq	0x2b31(%rip), %rdi      # 0x3da0 <.omp_offloading.descriptor+0x20>
    126f: 48 c7 c6 ff ff ff ff         	movq	$-0x1, %rsi
    1276: ba ff ff ff ff               	movl	$0xffffffff, %edx       # imm = 0xFFFFFFFF
    127b: 31 c9                        	xorl	%ecx, %ecx
    127d: 4c 8d 05 80 0d 00 00         	leaq	0xd80(%rip), %r8        # 0x2004 <.__omp_offloading_802_5ae8e7c_main_l7.region_id>
    1284: 4c 8d 8d 78 ff ff ff         	leaq	-0x88(%rbp), %r9
    128b: e8 b0 fd ff ff               	callq	0x1040 <__tgt_target_kernel@plt>
    1290: 83 f8 00                     	cmpl	$0x0, %eax
    1293: 74 09                        	je	0x129e <main+0xee>
    1295: 48 8d 7d f8                  	leaq	-0x8(%rbp), %rdi
    1299: e8 22 00 00 00               	callq	0x12c0 <__omp_offloading_802_5ae8e7c_main_l7>
    129e: 8b 75 f8                     	movl	-0x8(%rbp), %esi
    12a1: 48 8d 3d 87 0d 00 00         	leaq	0xd87(%rip), %rdi       # 0x202f <.__omp_offloading_802_5ae8e7c_main_l7.region_id+0x2b>
    12a8: b0 00                        	movb	$0x0, %al
    12aa: e8 81 fd ff ff               	callq	0x1030 <printf@plt>
    12af: 31 c0                        	xorl	%eax, %eax
    12b1: 48 81 c4 90 00 00 00         	addq	$0x90, %rsp
    12b8: 5d                           	popq	%rbp
    12b9: c3                           	retq
    12ba: 66 0f 1f 44 00 00            	nopw	(%rax,%rax)

00000000000012c0 <__omp_offloading_802_5ae8e7c_main_l7>:
    12c0: 55                           	pushq	%rbp
    12c1: 48 89 e5                     	movq	%rsp, %rbp
    12c4: 48 89 7d f8                  	movq	%rdi, -0x8(%rbp)
    12c8: 48 8b 45 f8                  	movq	-0x8(%rbp), %rax
    12cc: c7 00 01 00 00 00            	movl	$0x1, (%rax)
    12d2: 5d                           	popq	%rbp
    12d3: c3                           	retq
    12d4: 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
    12de: 66 90                        	nop

00000000000012e0 <atexit>:
    12e0: f3 0f 1e fa                  	endbr64
    12e4: 48 8b 15 45 2d 00 00         	movq	0x2d45(%rip), %rdx      # 0x4030 <__dso_handle>
    12eb: 31 f6                        	xorl	%esi, %esi
    12ed: e9 5e fd ff ff               	jmp	0x1050 <__cxa_atexit@plt>

Disassembly of section .fini:

00000000000012f4 <_fini>:
    12f4: f3 0f 1e fa                  	endbr64
    12f8: 48 83 ec 08                  	subq	$0x8, %rsp
    12fc: 48 83 c4 08                  	addq	$0x8, %rsp
    1300: c3                           	retq
