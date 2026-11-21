; GCC 16.2.1 Release, exact item gate inlined into unordered crossPairs<int32>.
; Separation returns to the loop before any boundary/degeneracy check.
   98c4c:	movdqu xmm2,XMMWORD PTR [rdi]
   98c50:	movdqu xmm0,XMMWORD PTR [r8]
   98c55:	movdqa xmm1,xmm2
   98c59:	punpcklqdq xmm1,xmm0
   98c5d:	punpckhqdq xmm0,xmm2
   98c61:	movdqa xmm2,xmm1
   98c65:	pcmpgtd xmm2,xmm0
   98c69:	pmovmskb r14d,xmm2
   98c6e:	test   r14d,r14d
   98c71:	jne    98cd0
   98c73:	pcmpeqd xmm1,xmm0
   98c77:	pmovmskb r14d,xmm1
   98c7c:	cmp    BYTE PTR [rsi+0x19],0x1
   98c80:	je     98f00
   98c86:	test   r14d,r14d
   98c89:	je     98f00
   98c8f:	mov    r14d,DWORD PTR [rdi+0x8]
   98c93:	cmp    DWORD PTR [rdi],r14d
   98c96:	je     98f00
   98c9c:	mov    r14d,DWORD PTR [rdi+0xc]
   98ca0:	cmp    DWORD PTR [rdi+0x4],r14d
   98ca4:	je     98f00
   98caa:	mov    edi,DWORD PTR [r8+0x8]
   98cae:	cmp    DWORD PTR [r8],edi
   98cb1:	je     98f00
   98cb7:	mov    edi,DWORD PTR [r8+0xc]
   98cbb:	cmp    DWORD PTR [r8+0x4],edi
   98cbf:	je     98f00
