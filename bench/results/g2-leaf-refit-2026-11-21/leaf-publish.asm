; GCC 16.2.1 Release, materializeBulk<int32>.
; Control: leaf builds its own items, then branches to shared refit tail.
  14b912:	jmp    1302e0
  14b917:	nop    WORD PTR [rax+rax*1+0x0]
  14b920:	mov    ecx,r9d
  14b923:	mov    edx,r8d
  14b926:	mov    esi,r14d
  14b929:	call   14b060
  14b92e:	add    rbp,QWORD PTR [r12+0xf0]
  14b936:	mov    eax,DWORD PTR [rbp+0x68]
  14b939:	cmp    DWORD PTR [r12+0x1c],eax
  14b93e:	jae    14b8da
  14b940:	shr    eax,1
  14b942:	mov    edx,0x1
  14b947:	cmove  eax,edx
  14b94a:	mov    DWORD PTR [rbp+0x6c],eax
  14b94d:	jmp    14b8da

; Candidate: parent still jumps to refit; leaf publishes and returns.
  14b392:	jmp    12fd60
  14b397:	nop    WORD PTR [rax+rax*1+0x0]
  14b3a0:	mov    ecx,r9d
  14b3a3:	mov    edx,r8d
  14b3a6:	mov    esi,r14d
  14b3a9:	call   14aae0
  14b3ae:	add    rbp,QWORD PTR [r12+0xf0]
  14b3b6:	mov    eax,DWORD PTR [rbp+0x68]
  14b3b9:	cmp    DWORD PTR [r12+0x1c],eax
  14b3be:	jae    14b3cf
  14b3c0:	mov    edx,eax
  14b3c2:	mov    ecx,0x1
  14b3c7:	shr    edx,1
  14b3c9:	cmove  edx,ecx
  14b3cc:	mov    DWORD PTR [rbp+0x6c],edx
  14b3cf:	movdqu xmm0,XMMWORD PTR [rbp+0x24]
  14b3d4:	mov    DWORD PTR [rbp+0x64],eax
  14b3d7:	movups XMMWORD PTR [rbp+0x14],xmm0
  14b3db:	movdqu xmm0,XMMWORD PTR [rbp+0x48]
  14b3e0:	movups XMMWORD PTR [rbp+0x38],xmm0
  14b3e4:	mov    rax,QWORD PTR [rsp+0x58]
  14b3e9:	sub    rax,QWORD PTR fs:0x28
  14b3f2:	jne    14b5fc
  14b3f8:	mov    rbp,QWORD PTR [rsp+0x70]
  14b3fd:	mov    r12,QWORD PTR [rsp+0x78]
  14b402:	mov    r14,QWORD PTR [rsp+0x88]
  14b40a:	add    rsp,0x98
  14b411:	ret
