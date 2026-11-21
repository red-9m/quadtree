# staged single-stream loop
  145f00:	movzx  esi,BYTE PTR [r9+rax*1]
  145f05:	mov    edi,DWORD PTR [r11+rax*4]
  145f09:	add    rax,0x1
  145f0d:	mov    ecx,DWORD PTR [rsp+rsi*4+0x120]
  145f14:	lea    r13d,[rcx+0x1]
  145f18:	mov    DWORD PTR [rbp+rcx*4+0x0],edi
  145f1c:	mov    DWORD PTR [rsp+rsi*4+0x120],r13d
  145f24:	cmp    r8,rax
  145f27:	jne    145f00 

# candidate interleaved loop; lower cursors at rsp+0x160, upper at rsp+0x180
  145e20:	movzx  esi,BYTE PTR [rbp+rax*1+0x0]
  145e25:	lea    edx,[r13+rax*1+0x0]
  145e2a:	mov    ebx,DWORD PTR [r9+rdx*4]
  145e2e:	mov    ecx,DWORD PTR [rsp+rsi*4+0x160]
  145e35:	lea    r15d,[rcx+0x1]
  145e39:	mov    DWORD PTR [rsp+rsi*4+0x160],r15d
  145e41:	movzx  esi,BYTE PTR [r14+rdx*1]
  145e46:	mov    edx,DWORD PTR [rsp+rsi*4+0x180]
  145e4d:	lea    r15d,[rdx+0x1]
  145e51:	mov    DWORD PTR [rsp+rsi*4+0x180],r15d
  145e59:	mov    esi,DWORD PTR [r12+rax*4]
  145e5d:	add    rax,0x1
  145e61:	mov    DWORD PTR [rdi+rcx*4],esi
  145e64:	mov    DWORD PTR [rdi+rdx*4],ebx
  145e67:	cmp    rax,r11
  145e6a:	jne    145e20 
