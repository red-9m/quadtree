; GCC x86-64 staged control, objdump -dl -Mintel --no-show-raw-insn (selected windows).

; First classifier loop: lookahead ID / prefetch, demand ID, four Box<int32> edges
  145c7e: cmp    esi,0x10
  145c81: jbe    145c93
  145c83: mov    eax,ebp
  145c85: sub    eax,esi
  145c87: mov    eax,DWORD PTR [rdx+rax*4]
  145c8a: shl    rax,0x4
  145c8e: prefetcht0 BYTE PTR [r14+rax*1]
  145c93: mov    eax,DWORD PTR [rdi]
  145c95: shl    rax,0x4
  145c99: add    rax,r14
  145c9c: mov    r11d,DWORD PTR [rax]
  145c9f: mov    ecx,DWORD PTR [rax+0x8]
  145ca2: mov    r10d,DWORD PTR [rax+0x4]
  145ca6: mov    eax,DWORD PTR [rax+0xc]

; Second classifier loop: same operands; SUB reads the fourth edge
  145d70: mov    eax,DWORD PTR [rsp+0x20]
  145d74: mov    rdx,QWORD PTR [rsp+0x10]
  145d79: sub    eax,esi
  145d7b: mov    eax,DWORD PTR [rdx+rax*4]
  145d7e: shl    rax,0x4
  145d82: prefetcht0 BYTE PTR [r14+rax*1]
  145d87: mov    eax,DWORD PTR [r10+rdi*4]
  145d8b: shl    rax,0x4
  145d8f: add    rax,r14
  145d92: mov    r9d,DWORD PTR [rax]
  145d95: mov    ecx,DWORD PTR [rax+0x8]
  145d98: mov    edx,DWORD PTR [rax+0xc]
  145d9b: sub    edx,DWORD PTR [rax+0x4]

; Stable scatter: tag and source ID loads, cursor update, destination ID store
  145f00: movzx  esi,BYTE PTR [r9+rax*1]
  145f05: mov    edi,DWORD PTR [r11+rax*4]
  145f09: add    rax,0x1
  145f0d: mov    ecx,DWORD PTR [rsp+rsi*4+0x120]
  145f14: lea    r13d,[rcx+0x1]
  145f18: mov    DWORD PTR [rbp+rcx*4+0x0],edi
  145f1c: mov    DWORD PTR [rsp+rsi*4+0x120],r13d
  145f24: cmp    r8,rax
  145f27: jne    145f00

; Finish copy sites
  14597c: call   73b0 <memmove@plt>

; Finish copy site (inlined)
  145af9: call   73b0 <memmove@plt>

; Own-range copy site
  145f5e: call   73b0 <memmove@plt>
