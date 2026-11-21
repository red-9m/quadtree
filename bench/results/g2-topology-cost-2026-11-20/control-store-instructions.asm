; Staged control, GCC x86-64: objdump -dl -Mintel --no-show-raw-insn.

; node_temp_zero
  14a554: mov    QWORD PTR [rsp+0xb0],0x0
  14a56c: movaps XMMWORD PTR [rsp+0x50],xmm1
  14a571: movaps XMMWORD PTR [rsp+0x60],xmm1
  14a576: movaps XMMWORD PTR [rsp+0x70],xmm1
  14a57b: movaps XMMWORD PTR [rsp+0x80],xmm1
  14a583: movaps XMMWORD PTR [rsp+0x90],xmm1
  14a58b: movaps XMMWORD PTR [rsp+0xa0],xmm1
  14a740: movaps XMMWORD PTR [rsp+0x50],xmm0
  14a74d: mov    QWORD PTR [rsp+0xb0],0x0
  14a759: movaps XMMWORD PTR [rsp+0x60],xmm0
  14a75e: movaps XMMWORD PTR [rsp+0x70],xmm0
  14a763: movaps XMMWORD PTR [rsp+0x80],xmm0
  14a76b: movaps XMMWORD PTR [rsp+0x90],xmm0
  14a773: movaps XMMWORD PTR [rsp+0xa0],xmm0
  14a8eb: movaps XMMWORD PTR [rsp+0x50],xmm0
  14a904: mov    QWORD PTR [rsp+0xb0],0x0
  14a93b: movaps XMMWORD PTR [rsp+0x90],xmm0
  14a943: movaps XMMWORD PTR [rsp+0xa0],xmm0
  14a999: movaps XMMWORD PTR [rsp+0x60],xmm0
  14a9a8: movaps XMMWORD PTR [rsp+0x70],xmm0
  14a9b0: movaps XMMWORD PTR [rsp+0x80],xmm0

; node_pool_copy
  14a60a: movups XMMWORD PTR [rdx],xmm0
  14a62d: movups XMMWORD PTR [rdx+0x10],xmm0
  14a63c: movups XMMWORD PTR [rdx+0x20],xmm0
  14a646: movups XMMWORD PTR [rdx+0x30],xmm0
  14a653: movups XMMWORD PTR [rdx+0x40],xmm0
  14a660: movups XMMWORD PTR [rdx+0x50],xmm0
  14a66d: movups XMMWORD PTR [rdx+0x60],xmm0
  14a671: mov    QWORD PTR [rdx+0x6f],rdi
  14a7eb: movups XMMWORD PTR [rdx],xmm1
  14a804: movups XMMWORD PTR [rdx+0x10],xmm1
  14a816: movups XMMWORD PTR [rdx+0x20],xmm1
  14a827: movups XMMWORD PTR [rdx+0x30],xmm1
  14a844: movups XMMWORD PTR [rdx+0x40],xmm1
  14a856: movups XMMWORD PTR [rdx+0x50],xmm1
  14a863: movups XMMWORD PTR [rdx+0x60],xmm1
  14a867: mov    QWORD PTR [rdx+0x6f],rcx
  14a932: movups XMMWORD PTR [rdx],xmm1
  14a94b: movups XMMWORD PTR [rdx+0x10],xmm1
  14a97c: movups XMMWORD PTR [rdx+0x50],xmm1
  14a989: movups XMMWORD PTR [rdx+0x20],xmm0
  14a98d: movups XMMWORD PTR [rdx+0x30],xmm0
  14a991: movups XMMWORD PTR [rdx+0x40],xmm0
  14a995: movups XMMWORD PTR [rdx+0x60],xmm1
  14a99e: mov    QWORD PTR [rdx+0x6f],rcx

; reference_store
  149f80: movups XMMWORD PTR [rbx-0x30],xmm2
  149fa6: movups XMMWORD PTR [rbx-0x20],xmm13
  149fbd: movups XMMWORD PTR [rbx-0x10],xmm1
  14a155: mov    DWORD PTR [rbx],r15d
  14a15b: mov    DWORD PTR [rbx+0x4],r12d
  14a15f: mov    DWORD PTR [rbx+0x8],ebp
  14a1d3: mov    DWORD PTR [rbp+0x0],r15d
  14a1d7: mov    DWORD PTR [rbp+0x4],ebx
  14a1e4: mov    DWORD PTR [rbp+0x8],r12d
  14a264: mov    DWORD PTR [rbp+0x0],r15d
  14a271: mov    DWORD PTR [rbp+0x4],eax
  14a27b: mov    DWORD PTR [rbp+0x8],ebx
  14a390: mov    DWORD PTR [rbx],r15d
  14a39a: mov    DWORD PTR [rbx+0x4],r14d
  14a3ab: mov    DWORD PTR [rbx+0x8],r12d

; refit_initial_subtree_store
  12f5fa: mov    DWORD PTR [rax+0x64],edi
  12f615: movups XMMWORD PTR [rax+0x14],xmm0
  12f61e: movups XMMWORD PTR [rax+0x38],xmm0
