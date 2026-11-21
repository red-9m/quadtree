; GCC 16.2.1 Release, staged int32 bulk commit.
; Dependent payload load is at 147839; most payload block periods land at 14783c.
  147830:	mov    rax,QWORD PTR [rcx+0x20]
  147834:	mov    r9,rdx
  147837:	mov    edx,esi
  147839:	mov    r10d,DWORD PTR [rax]
  14783c:	mov    rax,QWORD PTR [rdi+0x58]
  147840:	mov    DWORD PTR [rax+rdx*4],r10d
  147844:	mov    rax,rdx
  147847:	movdqu xmm0,XMMWORD PTR [r9]
  14784c:	xor    r9d,r9d
  14784f:	shl    rax,0x4
  147853:	add    rax,QWORD PTR [rdi+0x50]
  147857:	movups XMMWORD PTR [rax],xmm0
  14785a:	movdqu xmm0,XMMWORD PTR [rcx]
  14785e:	test   r8b,r8b
  147861:	je     147867
  147863:	mov    r9,QWORD PTR [rcx+0x10]
  147867:	mov    rax,rdx
  14786a:	shl    rax,0x5
  14786e:	add    rax,QWORD PTR [rdi+0x60]
  147872:	mov    QWORD PTR [rax+0x10],r9
  147876:	mov    DWORD PTR [rax+0x18],esi
  147879:	mov    BYTE PTR [rax+0x1c],r8b
  14787d:	movups XMMWORD PTR [rax],xmm0
  147880:	mov    rax,QWORD PTR [rdi+0x68]
  147884:	mov    DWORD PTR [rax+rdx*8],0x0
  14788b:	mov    DWORD PTR [rax+rdx*8+0x4],esi
  14788f:	ret

; Existing lookahead followed by current index/address formation.
   d32d4:	mov    rsi,QWORD PTR [rsp+0x18]
   d32d9:	test   rsi,rsi
   d32dc:	je     d3998
   d32e2:	mov    edx,DWORD PTR [rsi+rbx*4+0x80]
   d32e9:	mov    rcx,rdx
   d32ec:	lea    rdx,[rdx+rdx*4]
   d32f0:	lea    rdx,[r13+rdx*8+0x0]
   d32f5:	shl    rcx,0x4
   d32f9:	prefetcht0 BYTE PTR [r12+rcx*1]
   d32fe:	prefetcht0 BYTE PTR [rdx]
   d3301:	prefetcht0 BYTE PTR [rdx+0x20]
   d3305:	mov    rsi,QWORD PTR [rsp+0x18]
   d330a:	mov    edx,DWORD PTR [rsi+rbx*4+0x40]
   d330e:	lea    rdx,[rdx+rdx*4]
   d3312:	mov    rdx,QWORD PTR [r13+rdx*8+0x20]
   d3317:	prefetcht0 BYTE PTR [rdx]
   d331a:	mov    rsi,QWORD PTR [rsp+0x18]
   d331f:	mov    ebp,DWORD PTR [rsi+rbx*4]
   d3322:	lea    rdx,[rbp+rbp*4+0x0]
   d3327:	shl    rbp,0x4
   d332b:	mov    rdi,rax
   d332e:	mov    esi,r14d
   d3331:	lea    rcx,[r13+rdx*8+0x0]
   d3336:	lea    rdx,[r12+rbp*1]
   d333a:	mov    QWORD PTR [rsp+0x20],rax
   d333f:	movzx  r8d,BYTE PTR [rcx+0x18]
   d3344:	mov    QWORD PTR [rsp+0x40],rcx
   d3349:	mov    BYTE PTR [rsp+0x30],r8b
   d334e:	call   147830
