
broken-args:	file format elf64-x86-64

Disassembly of section .init:

0000000000001000 <_init>:
    1000: f3 0f 1e fa                  	endbr64
    1004: 48 83 ec 08                  	subq	$0x8, %rsp
    1008: 48 8b 05 b9 2f 00 00         	movq	0x2fb9(%rip), %rax      # 0x3fc8 <printf@GLIBC_2.2.5+0x3fc8>
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

0000000000001040 <__tgt_register_lib@plt>:
    1040: ff 25 c2 2f 00 00            	jmpq	*0x2fc2(%rip)           # 0x4008 <_GLOBAL_OFFSET_TABLE_+0x20>
    1046: 68 01 00 00 00               	pushq	$0x1
    104b: e9 d0 ff ff ff               	jmp	0x1020 <.plt>

0000000000001050 <__tgt_target_kernel@plt>:
    1050: ff 25 ba 2f 00 00            	jmpq	*0x2fba(%rip)           # 0x4010 <_GLOBAL_OFFSET_TABLE_+0x28>
    1056: 68 02 00 00 00               	pushq	$0x2
    105b: e9 c0 ff ff ff               	jmp	0x1020 <.plt>

0000000000001060 <__tgt_unregister_lib@plt>:
    1060: ff 25 b2 2f 00 00            	jmpq	*0x2fb2(%rip)           # 0x4018 <_GLOBAL_OFFSET_TABLE_+0x30>
    1066: 68 03 00 00 00               	pushq	$0x3
    106b: e9 b0 ff ff ff               	jmp	0x1020 <.plt>

0000000000001070 <__cxa_atexit@plt>:
    1070: ff 25 aa 2f 00 00            	jmpq	*0x2faa(%rip)           # 0x4020 <_GLOBAL_OFFSET_TABLE_+0x38>
    1076: 68 04 00 00 00               	pushq	$0x4
    107b: e9 a0 ff ff ff               	jmp	0x1020 <.plt>

Disassembly of section .plt.got:

0000000000001080 <__cxa_finalize@plt>:
    1080: ff 25 3a 2f 00 00            	jmpq	*0x2f3a(%rip)           # 0x3fc0 <printf@GLIBC_2.2.5+0x3fc0>
    1086: 66 90                        	nop

Disassembly of section .text:

0000000000001090 <.omp_offloading.descriptor_reg>:
    1090: 50                           	pushq	%rax
    1091: 48 8d 3d e8 2c 00 00         	leaq	0x2ce8(%rip), %rdi      # 0x3d80 <.omp_offloading.descriptor>
    1098: e8 a3 ff ff ff               	callq	0x1040 <__tgt_register_lib@plt>
    109d: 48 8d 3d 0c 00 00 00         	leaq	0xc(%rip), %rdi         # 0x10b0 <.omp_offloading.descriptor_unreg>
    10a4: e8 c7 02 00 00               	callq	0x1370 <atexit>
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
    10df: ff 15 f3 2e 00 00            	callq	*0x2ef3(%rip)           # 0x3fd8 <printf@GLIBC_2.2.5+0x3fd8>
    10e5: f4                           	hlt
    10e6: 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)

00000000000010f0 <deregister_tm_clones>:
    10f0: 48 8d 3d 79 2f 00 00         	leaq	0x2f79(%rip), %rdi      # 0x4070 <completed.0>
    10f7: 48 8d 05 72 2f 00 00         	leaq	0x2f72(%rip), %rax      # 0x4070 <completed.0>
    10fe: 48 39 f8                     	cmpq	%rdi, %rax
    1101: 74 15                        	je	0x1118 <deregister_tm_clones+0x28>
    1103: 48 8b 05 ae 2e 00 00         	movq	0x2eae(%rip), %rax      # 0x3fb8 <printf@GLIBC_2.2.5+0x3fb8>
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
    1144: 48 8b 05 85 2e 00 00         	movq	0x2e85(%rip), %rax      # 0x3fd0 <printf@GLIBC_2.2.5+0x3fd0>
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
    116e: 48 83 3d 4a 2e 00 00 00      	cmpq	$0x0, 0x2e4a(%rip)      # 0x3fc0 <printf@GLIBC_2.2.5+0x3fc0>
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
    11b4: 48 81 ec d0 00 00 00         	subq	$0xd0, %rsp
    11bb: c7 45 fc 00 00 00 00         	movl	$0x0, -0x4(%rbp)
    11c2: c7 45 f8 0b 00 00 00         	movl	$0xb, -0x8(%rbp)
    11c9: c6 45 f7 31                  	movb	$0x31, -0x9(%rbp)
    11cd: 48 c7 45 e8 ff ff ff ff      	movq	$-0x1, -0x18(%rbp)
    11d5: 48 8d 45 f8                  	leaq	-0x8(%rbp), %rax
    11d9: 48 89 45 d0                  	movq	%rax, -0x30(%rbp)
    11dd: 48 89 45 b8                  	movq	%rax, -0x48(%rbp)
    11e1: 48 c7 45 a0 00 00 00 00      	movq	$0x0, -0x60(%rbp)
    11e9: 48 8d 45 f7                  	leaq	-0x9(%rbp), %rax
    11ed: 48 89 45 d8                  	movq	%rax, -0x28(%rbp)
    11f1: 48 89 45 c0                  	movq	%rax, -0x40(%rbp)
    11f5: 48 c7 45 a8 00 00 00 00      	movq	$0x0, -0x58(%rbp)
    11fd: 48 8d 45 e8                  	leaq	-0x18(%rbp), %rax
    1201: 48 89 45 e0                  	movq	%rax, -0x20(%rbp)
    1205: 48 89 45 c8                  	movq	%rax, -0x38(%rbp)
    1209: 48 c7 45 b0 00 00 00 00      	movq	$0x0, -0x50(%rbp)
    1211: c7 85 38 ff ff ff 03 00 00 00	movl	$0x3, -0xc8(%rbp)
    121b: c7 85 3c ff ff ff 03 00 00 00	movl	$0x3, -0xc4(%rbp)
    1225: 48 8d 45 d0                  	leaq	-0x30(%rbp), %rax
    1229: 48 89 85 40 ff ff ff         	movq	%rax, -0xc0(%rbp)
    1230: 48 8d 45 b8                  	leaq	-0x48(%rbp), %rax
    1234: 48 89 85 48 ff ff ff         	movq	%rax, -0xb8(%rbp)
    123b: 48 8d 05 de 0d 00 00         	leaq	0xdde(%rip), %rax       # 0x2020 <.__omp_offloading_802_5ae9003_main_l9.region_id+0x10>
    1242: 48 89 85 50 ff ff ff         	movq	%rax, -0xb0(%rbp)
    1249: 48 8d 05 f0 0d 00 00         	leaq	0xdf0(%rip), %rax       # 0x2040 <.__omp_offloading_802_5ae9003_main_l9.region_id+0x30>
    1250: 48 89 85 58 ff ff ff         	movq	%rax, -0xa8(%rbp)
    1257: 48 c7 85 60 ff ff ff 00 00 00 00     	movq	$0x0, -0xa0(%rbp)
    1262: 48 c7 85 68 ff ff ff 00 00 00 00     	movq	$0x0, -0x98(%rbp)
    126d: 48 c7 85 70 ff ff ff 00 00 00 00     	movq	$0x0, -0x90(%rbp)
    1278: 48 c7 85 78 ff ff ff 00 00 00 00     	movq	$0x0, -0x88(%rbp)
    1283: c7 45 88 00 00 00 00         	movl	$0x0, -0x78(%rbp)
    128a: c7 45 84 00 00 00 00         	movl	$0x0, -0x7c(%rbp)
    1291: c7 45 80 ff ff ff ff         	movl	$0xffffffff, -0x80(%rbp) # imm = 0xFFFFFFFF
    1298: c7 45 94 00 00 00 00         	movl	$0x0, -0x6c(%rbp)
    129f: c7 45 90 00 00 00 00         	movl	$0x0, -0x70(%rbp)
    12a6: c7 45 8c 00 00 00 00         	movl	$0x0, -0x74(%rbp)
    12ad: c7 45 98 00 00 00 00         	movl	$0x0, -0x68(%rbp)
    12b4: 48 8d 3d e5 2a 00 00         	leaq	0x2ae5(%rip), %rdi      # 0x3da0 <.omp_offloading.descriptor+0x20>
    12bb: 48 c7 c6 ff ff ff ff         	movq	$-0x1, %rsi
    12c2: ba ff ff ff ff               	movl	$0xffffffff, %edx       # imm = 0xFFFFFFFF
    12c7: 31 c9                        	xorl	%ecx, %ecx
    12c9: 4c 8d 05 40 0d 00 00         	leaq	0xd40(%rip), %r8        # 0x2010 <.__omp_offloading_802_5ae9003_main_l9.region_id>
    12d0: 4c 8d 8d 38 ff ff ff         	leaq	-0xc8(%rbp), %r9
    12d7: e8 74 fd ff ff               	callq	0x1050 <__tgt_target_kernel@plt>
    12dc: 83 f8 00                     	cmpl	$0x0, %eax
    12df: 74 11                        	je	0x12f2 <main+0x142>
    12e1: 48 8d 7d f8                  	leaq	-0x8(%rbp), %rdi
    12e5: 48 8d 75 f7                  	leaq	-0x9(%rbp), %rsi
    12e9: 48 8d 55 e8                  	leaq	-0x18(%rbp), %rdx
    12ed: e8 4e 00 00 00               	callq	0x1340 <__omp_offloading_802_5ae9003_main_l9>
    12f2: 83 7d f8 00                  	cmpl	$0x0, -0x8(%rbp)
    12f6: 73 02                        	jae	0x12fa <main+0x14a>
    12f8: eb 00                        	jmp	0x12fa <main+0x14a>
    12fa: 8b 75 f8                     	movl	-0x8(%rbp), %esi
    12fd: 48 8d 3d 6b 0d 00 00         	leaq	0xd6b(%rip), %rdi       # 0x206f <.__omp_offloading_802_5ae9003_main_l9.region_id+0x5f>
    1304: b0 00                        	movb	$0x0, %al
    1306: e8 25 fd ff ff               	callq	0x1030 <printf@plt>
    130b: 8b 4d f8                     	movl	-0x8(%rbp), %ecx
    130e: 48 8d 35 95 0d 00 00         	leaq	0xd95(%rip), %rsi       # 0x20aa <.__omp_offloading_802_5ae9003_main_l9.region_id+0x9a>
    1315: 48 8d 05 89 0d 00 00         	leaq	0xd89(%rip), %rax       # 0x20a5 <.__omp_offloading_802_5ae9003_main_l9.region_id+0x95>
    131c: 83 f9 00                     	cmpl	$0x0, %ecx
    131f: 48 0f 45 f0                  	cmovneq	%rax, %rsi
    1323: 48 8d 3d 59 0d 00 00         	leaq	0xd59(%rip), %rdi       # 0x2083 <.__omp_offloading_802_5ae9003_main_l9.region_id+0x73>
    132a: b0 00                        	movb	$0x0, %al
    132c: e8 ff fc ff ff               	callq	0x1030 <printf@plt>
    1331: 8b 45 f8                     	movl	-0x8(%rbp), %eax
    1334: 48 81 c4 d0 00 00 00         	addq	$0xd0, %rsp
    133b: 5d                           	popq	%rbp
    133c: c3                           	retq
    133d: 0f 1f 00                     	nopl	(%rax)

0000000000001340 <__omp_offloading_802_5ae9003_main_l9>:
    1340: 55                           	pushq	%rbp
    1341: 48 89 e5                     	movq	%rsp, %rbp
    1344: 48 89 7d f8                  	movq	%rdi, -0x8(%rbp)
    1348: 48 89 75 f0                  	movq	%rsi, -0x10(%rbp)
    134c: 48 89 55 e8                  	movq	%rdx, -0x18(%rbp)
    1350: 48 8b 55 f8                  	movq	-0x8(%rbp), %rdx
    1354: 48 8b 4d f0                  	movq	-0x10(%rbp), %rcx
    1358: 48 8b 45 e8                  	movq	-0x18(%rbp), %rax
    135c: c7 02 22 00 00 00            	movl	$0x22, (%rdx)
    1362: c6 01 27                     	movb	$0x27, (%rcx)
    1365: 48 c7 00 eb 00 00 00         	movq	$0xeb, (%rax)
    136c: 5d                           	popq	%rbp
    136d: c3                           	retq
    136e: 66 90                        	nop

0000000000001370 <atexit>:
    1370: f3 0f 1e fa                  	endbr64
    1374: 48 8b 15 b5 2c 00 00         	movq	0x2cb5(%rip), %rdx      # 0x4030 <__dso_handle>
    137b: 31 f6                        	xorl	%esi, %esi
    137d: e9 ee fc ff ff               	jmp	0x1070 <__cxa_atexit@plt>

Disassembly of section .fini:

0000000000001384 <_fini>:
    1384: f3 0f 1e fa                  	endbr64
    1388: 48 83 ec 08                  	subq	$0x8, %rsp
    138c: 48 83 c4 08                  	addq	$0x8, %rsp
    1390: c3                           	retq
