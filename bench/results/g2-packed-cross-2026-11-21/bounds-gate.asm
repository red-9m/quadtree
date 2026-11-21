; GCC 16.2.1 Release int32 unordered crossPairs, after occupancy and charge.
; Control: four short-circuit signed scalar bounds tests.
   9af17:	mov    r11d,DWORD PTR [rdx+0x1c]
   9af1b:	cmp    DWORD PTR [rax+0x14],r11d
   9af1f:	jg     9b200
   9af25:	mov    QWORD PTR [rsp+0xd8],rbx
   9af2d:	mov    ebx,DWORD PTR [rax+0x1c]
   9af30:	cmp    DWORD PTR [rdx+0x14],ebx
   9af33:	jg     9b240
   9af39:	mov    ebx,DWORD PTR [rdx+0x20]
   9af3c:	cmp    DWORD PTR [rax+0x18],ebx
   9af3f:	jg     9b240
   9af45:	mov    ebx,DWORD PTR [rax+0x20]
   9af48:	cmp    DWORD PTR [rdx+0x18],ebx
   9af4b:	jg     9b240

; Candidate: two corner loads, opposite-maximum pack, signed lane comparison and mask.
   9ad39:	movdqu xmm1,XMMWORD PTR [r13+0x14]
   9ad3f:	movdqu xmm0,XMMWORD PTR [r14+0x14]
   9ad45:	movdqa xmm2,xmm1
   9ad49:	punpckhqdq xmm2,xmm0
   9ad4d:	punpcklqdq xmm0,xmm1
   9ad51:	pcmpgtd xmm0,xmm2
   9ad55:	pmovmskb eax,xmm0
   9ad59:	test   eax,eax
   9ad5b:	jne    9afe2
