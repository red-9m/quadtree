; int32 contiguous BuildEntry bulk commit, GCC 16.2.1 Release.
; Sized input stride is 0x38 bytes; payload is at offset 0x10.
   d3517:	prefetcht0 BYTE PTR [rax+rdx*1+0x10]
   d351c:	mov    rax,QWORD PTR [rsp+0x20]
   d3521:	mov    edx,DWORD PTR [rax+r12*4]
   d3525:	mov    rax,QWORD PTR [rsp+0x8]
   d352a:	mov    rbp,rdx
   d352d:	lea    rcx,[rdx+rdx*4]
   d3531:	mov    rdi,rbx
   d3534:	imul   rdx,rdx,0x38
   d3538:	shl    rbp,0x4
   d353c:	lea    rcx,[r13+rcx*8+0x0]
   d3541:	lea    rsi,[rax+rbp*1]
   d3545:	mov    rax,QWORD PTR [rsp+0x48]
   d354a:	movzx  r8d,BYTE PTR [rcx+0x18]
   d354f:	mov    QWORD PTR [rsp+0x10],rcx
   d3554:	mov    r9d,DWORD PTR [rax+rdx*1+0x10]
   d3559:	mov    rdx,rsi
   d355c:	mov    esi,r15d
   d355f:	mov    BYTE PTR [rsp+0x28],r8b
   d3564:	call   11620

; Constructor clone receives the payload value in r9d.
   11620:	mov    rax,QWORD PTR [rdi+0x58]
   11624:	mov    r11,rdx
   11627:	mov    edx,esi
   11629:	mov    r10,rcx
   1162c:	mov    DWORD PTR [rax+rdx*4],r9d
   11630:	mov    rax,rdx
   11633:	movdqu xmm0,XMMWORD PTR [r11]
   11638:	shl    rax,0x4
   1163c:	add    rax,QWORD PTR [rdi+0x50]
   11640:	movups XMMWORD PTR [rax],xmm0
   11643:	movdqu xmm0,XMMWORD PTR [rcx]
   11647:	xor    ecx,ecx
   11649:	test   r8b,r8b
   1164c:	je     11652
   1164e:	mov    rcx,QWORD PTR [r10+0x10]
   11652:	mov    rax,rdx
   11655:	shl    rax,0x5
   11659:	add    rax,QWORD PTR [rdi+0x60]
   1165d:	mov    QWORD PTR [rax+0x10],rcx
   11661:	mov    DWORD PTR [rax+0x18],esi
   11664:	mov    BYTE PTR [rax+0x1c],r8b
   11668:	movups XMMWORD PTR [rax],xmm0
   1166b:	mov    rax,QWORD PTR [rdi+0x68]
   1166f:	mov    DWORD PTR [rax+rdx*8],0x0
   11676:	mov    DWORD PTR [rax+rdx*8+0x4],esi
   1167a:	ret
