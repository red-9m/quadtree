; GCC 16.2.1 Release, int32 ordered snapshot; objdump -d -Mintel --no-show-raw-insn.

; Source sort: n >= 4096, then both capacity checks, then radixRankOrder.
   f2180:	cmp    rsi,0xfff
   f2187:	jbe    f2277
   f218d:	cmp    QWORD PTR [r15+0x248],rsi
   f2194:	jb     f2281
   f219a:	movabs rax,0xaaaaaaaaaaaaaaab
   f21a4:	mul    QWORD PTR [r15+0x260]
   f21ab:	shr    rdx,0x4
   f21af:	cmp    rdx,rsi
   f21b2:	jb     f2281
   f21b8:	mov    rdi,QWORD PTR [r15+0x68]
   f21bc:	mov    rdx,QWORD PTR [rsp+0x30]
   f21c1:	mov    rsi,QWORD PTR [rsp+0x28]
   f21c6:	call   126e0
   f21cb:	jmp    f22ab

; Partner row: same helper, guarded by the separate row capacity checks.
   f245f:	cmp    DWORD PTR [rdi+0x248],ebp
   f2465:	jb     f27c3
   f246b:	movabs rax,0xaaaaaaaaaaaaaaab
   f2475:	mul    QWORD PTR [r15+0x260]
   f247c:	shr    rdx,0x4
   f2480:	cmp    rdx,rbp
   f2483:	jb     f27c3
   f2489:	mov    rdi,QWORD PTR [r15+0x68]
   f248d:	mov    rsi,QWORD PTR [rsp+0x28]
   f2492:	mov    ecx,ebp
   f2494:	mov    rdx,rbx
   f2497:	call   126e0
   f249c:	mov    rbp,QWORD PTR [rsp+0x48]
