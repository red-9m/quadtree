; GCC x86-64 Release candidate; objdump -d -Mintel --no-show-raw-insn.

; prepareHome: mix future rank, mask and prefetch into L1
  11be0: mov    rax,QWORD PTR [rdx]
  11be3: mov    rdx,QWORD PTR [rsi]
  11be6: mov    eax,DWORD PTR [rax+rcx*4]
  11be9: lea    rax,[rax+rax*4]
  11bed: lea    rax,[rdx+rax*8]
  11bf1: mov    rax,QWORD PTR [rax+0x10]
  11bf5: mov    rdx,rax
  11bf8: shr    rdx,0x1e
  11bfc: xor    rdx,rax
  11bff: movabs rax,0xbf58476d1ce4e5b9
  11c09: imul   rdx,rax
  11c0d: mov    rax,rdx
  11c10: shr    rax,0x1b
  11c14: xor    rdx,rax
  11c17: movabs rax,0x94d049bb133111eb
  11c21: imul   rdx,rax
  11c25: mov    rax,QWORD PTR [rdi+0x78]
  11c29: sub    rax,0x1
  11c2d: mov    rcx,rdx
  11c30: shr    rcx,0x1f
  11c34: xor    rdx,rcx
  11c37: and    rax,rdx
  11c3a: mov    rdx,rax
  11c3d: shl    rdx,0x4
  11c41: add    rdx,QWORD PTR [rdi+0x70]
  11c45: prefetcht0 BYTE PTR [rdx]
  11c48: ret

; Consume the current queued home; refresh only when another input exists
  d3175: mov    r8,r15
  d3178: mov    rax,r14
  d317b: and    r8d,0x1f
  d317f: sub    rax,r15
  d3182: mov    r9,QWORD PTR [rsp+r8*8+0x190]
  d318a: cmp    rax,0x20
  d318e: ja     d32d7

; Probe from the queued home (no second mix), publish bucket
  d3257: mov    rax,r9
  d325a: shl    rax,0x4
  d325e: add    rax,r11
  d3261: cmp    DWORD PTR [rax+0x8],edi
  d3264: je     d3240
  d3266: mov    rcx,QWORD PTR [rcx+0x10]
  d326a: mov    QWORD PTR [r10+0xa0],rdx
  d3271: mov    DWORD PTR [rax+0x8],edi
  d3274: mov    QWORD PTR [rax],rcx
  d3277: mov    DWORD PTR [rax+0xc],0x0

; Refresh one ring slot for i + 32
  d32d7: lea    rcx,[r15+0x20]
  d32db: lea    rdx,[rsp+0xc8]
  d32e3: mov    rdi,r10
  d32e6: lea    rsi,[rsp+0xc0]
  d32ee: call   11be0
  d32f3: mov    QWORD PTR [rsp+r8*8+0x190],rax
  d32fb: jmp    d3194
