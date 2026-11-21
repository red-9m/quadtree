# planBulk<int32>, staged GCC Release. objdump -d -Mintel --no-show-raw-insn.
  145f00:	movzx  esi,BYTE PTR [r9+rax*1]
  145f05:	mov    edi,DWORD PTR [r11+rax*4]
  145f09:	add    rax,0x1
  145f0d:	mov    ecx,DWORD PTR [rsp+rsi*4+0x120]
  145f14:	lea    r13d,[rcx+0x1]
  145f18:	mov    DWORD PTR [rbp+rcx*4+0x0],edi
  145f1c:	mov    DWORD PTR [rsp+rsi*4+0x120],r13d
  145f24:	cmp    r8,rax
  145f27:	jne    145f00 
