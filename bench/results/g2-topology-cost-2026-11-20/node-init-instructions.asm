; materializeBulk<int32> selected paths, GCC -O3. Control uses a stack Node then copies it.

; control
  14a410: sub    rsp,0x118
  14a544: movd   xmm0,DWORD PTR [r12]
  14a54a: movd   xmm2,DWORD PTR [r12+0x8]
  14a551: mov    ecx,r14d
  14a554: mov    QWORD PTR [rsp+0xb0],0x0
  14a560: movzx  edi,BYTE PTR [r12+0x10]
  14a566: movd   xmm3,DWORD PTR [rsp+0x38]
  14a56c: movaps XMMWORD PTR [rsp+0x50],xmm1
  14a571: movaps XMMWORD PTR [rsp+0x60],xmm1
  14a576: movaps XMMWORD PTR [rsp+0x70],xmm1
  14a57b: movaps XMMWORD PTR [rsp+0x80],xmm1
  14a583: movaps XMMWORD PTR [rsp+0x90],xmm1
  14a58b: movaps XMMWORD PTR [rsp+0xa0],xmm1
  14a593: test   r14b,0x2
  14a597: je     14a9e8
  14a59d: movdqa xmm4,xmm3
  14a5a1: movzx  r8d,BYTE PTR [r12+0x11]
  14a5a7: movd   xmm3,DWORD PTR [r12+0xc]
  14a5ae: lea    esi,[rbx+rcx*1]
  14a5b1: punpckldq xmm2,xmm3
  14a5b5: punpckldq xmm0,xmm4
  14a5b9: mov    BYTE PTR [rsp+0x50],dil
  14a5be: movzx  eax,BYTE PTR [r12+0x74]
  14a5c4: punpcklqdq xmm0,xmm2
  14a5c8: mov    BYTE PTR [rsp+0x51],r8b
  14a5cd: movaps XMMWORD PTR [rsp+0x40],xmm0
  14a5d2: mov    r9d,DWORD PTR [r15+r14*4+0x4]
  14a5d7: mov    r8d,DWORD PTR [r15+r14*4]
  14a5db: lea    edx,[rax+0x1]
  14a5de: mov    DWORD PTR [rsp+0x98],r13d
  14a5e6: mov    eax,DWORD PTR [rsp+0xc]
  14a5ea: mov    BYTE PTR [rsp+0xb4],dl
  14a5f1: mov    edx,esi
  14a5f3: imul   rdx,rdx,0x78
  14a5f7: add    rdx,QWORD PTR [rsp]
  14a5fb: bt     eax,ecx
  14a5fe: mov    QWORD PTR [rsp+0x9c],0xffffffffffffffff
  14a60a: movups XMMWORD PTR [rdx],xmm0
  14a60d: movdqa xmm0,XMMWORD PTR [rsp+0x50]
  14a613: setb   cl
  14a616: add    r14,0x1
  14a61a: mov    BYTE PTR [rsp+0xb5],0x1
  14a622: mov    rdi,QWORD PTR [rsp+0xaf]
  14a62a: movzx  ecx,cl
  14a62d: movups XMMWORD PTR [rdx+0x10],xmm0
  14a631: movdqa xmm0,XMMWORD PTR [rsp+0x60]
  14a637: movaps XMMWORD PTR [rsp+0x10],xmm1
  14a63c: movups XMMWORD PTR [rdx+0x20],xmm0
  14a640: movdqa xmm0,XMMWORD PTR [rsp+0x70]
  14a646: movups XMMWORD PTR [rdx+0x30],xmm0
  14a64a: movdqa xmm0,XMMWORD PTR [rsp+0x80]
  14a653: movups XMMWORD PTR [rdx+0x40],xmm0
  14a657: movdqa xmm0,XMMWORD PTR [rsp+0x90]
  14a660: movups XMMWORD PTR [rdx+0x50],xmm0
  14a664: movdqa xmm0,XMMWORD PTR [rsp+0xa0]
  14a66d: movups XMMWORD PTR [rdx+0x60],xmm0
  14a671: mov    QWORD PTR [rdx+0x6f],rdi
  14a675: mov    rdx,QWORD PTR [rsp+0x20]
  14a67a: mov    rdi,rbp
  14a67d: call   14a410
  14a682: cmp    r14,0x4
  14a686: movdqa xmm1,XMMWORD PTR [rsp+0x10]
  14a68c: jne    14a544

; direct initialization
  14a410: sub    rsp,0x98
  14a501: pxor   xmm0,xmm0
  14a505: mov    edx,DWORD PTR [rsp+0x10]
  14a509: mov    DWORD PTR [rsp+0xc],eax
  14a50d: movzx  eax,BYTE PTR [rsp+0x3d]
  14a512: mov    BYTE PTR [rsp+0x20],al
  14a516: jne    14a730
  14a51c: mov    QWORD PTR [rsp+0x20],r12
  14a521: mov    r15,rbp
  14a524: lea    r12,[rsp+0x40]
  14a529: mov    ebp,edx
  14a52b: test   al,al
  14a52d: je     14a690
  14a533: movups XMMWORD PTR [rbx],xmm0
  14a536: movd   xmm3,DWORD PTR [rsp+0x38]
  14a53c: mov    esi,r13d
  14a53f: mov    QWORD PTR [rbx+0x70],0x0
  14a547: movups XMMWORD PTR [rbx+0x10],xmm0
  14a54b: movups XMMWORD PTR [rbx+0x20],xmm0
  14a54f: movups XMMWORD PTR [rbx+0x30],xmm0
  14a553: movups XMMWORD PTR [rbx+0x40],xmm0
  14a557: movups XMMWORD PTR [rbx+0x50],xmm0
  14a55b: movups XMMWORD PTR [rbx+0x60],xmm0
  14a55f: movd   xmm1,DWORD PTR [r15]
  14a564: movd   xmm2,DWORD PTR [r15+0x8]
  14a56a: movzx  edx,BYTE PTR [r15+0x10]
  14a56f: test   r13b,0x2
  14a573: je     14a850
  14a579: movdqa xmm4,xmm3
  14a57d: movzx  ecx,BYTE PTR [r15+0x11]
  14a582: movd   xmm3,DWORD PTR [r15+0xc]
  14a588: punpckldq xmm2,xmm3
  14a58c: punpckldq xmm1,xmm4
  14a590: mov    BYTE PTR [rbx+0x10],dl
  14a593: mov    r9d,DWORD PTR [r12+r13*4+0x4]
  14a598: punpcklqdq xmm1,xmm2
  14a59c: mov    BYTE PTR [rbx+0x11],cl
  14a59f: xor    ecx,ecx
  14a5a1: mov    r8d,DWORD PTR [r12+r13*4]
  14a5a5: mov    DWORD PTR [rbx+0x58],r14d
  14a5a9: mov    rdi,QWORD PTR [rsp+0x20]
  14a5ae: mov    DWORD PTR [rbx+0x5c],0xffffffff
  14a5b5: mov    DWORD PTR [rbx+0x60],0xffffffff
  14a5bc: movups XMMWORD PTR [rbx],xmm1
  14a5bf: movzx  eax,BYTE PTR [r15+0x74]
  14a5c4: mov    BYTE PTR [rbx+0x75],0x1
  14a5c8: lea    edx,[rax+0x1]
  14a5cb: mov    eax,DWORD PTR [rsp+0xc]
  14a5cf: movaps XMMWORD PTR [rsp+0x10],xmm0
  14a5d4: mov    BYTE PTR [rbx+0x74],dl
  14a5d7: mov    rdx,QWORD PTR [rsp]
  14a5db: bt     eax,esi
  14a5de: setb   cl
  14a5e1: add    esi,ebp
  14a5e3: add    r13,0x1
  14a5e7: add    rbx,0x78
  14a5eb: call   14a410
  14a5f0: cmp    r13,0x4
  14a5f4: movdqa xmm0,XMMWORD PTR [rsp+0x10]
