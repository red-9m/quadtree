; GCC x86-64 staged control; objdump -d -Mintel --no-show-raw-insn.
; crossPairs :: Node.count +100 bytes from its node
  9adc6: mov    r11d,DWORD PTR [rax+0x64]
  9addd: mov    r11d,DWORD PTR [rdx+0x64]
; crossPairs :: Node.bounds +20 bytes from its node
  9ae1b: cmp    DWORD PTR [rax+0x14],r11d
  9ae30: cmp    DWORD PTR [rdx+0x14],ebx
; crossPairs :: Node.bounds +24 bytes from its node
  9ae3c: cmp    DWORD PTR [rax+0x18],ebx
  9ae48: cmp    DWORD PTR [rdx+0x18],ebx
; crossPairs :: Node.bounds +28 bytes from its node
  9ae17: mov    r11d,DWORD PTR [rdx+0x1c]
  9ae2d: mov    ebx,DWORD PTR [rax+0x1c]
; crossPairs :: Node.bounds +32 bytes from its node
  9ae39: mov    ebx,DWORD PTR [rdx+0x20]
  9ae45: mov    ebx,DWORD PTR [rax+0x20]
; crossPairs :: Node.firstChild +92 bytes from its node
  9afa1: mov    r11d,DWORD PTR [rdx+0x5c]
  9b010: mov    esi,DWORD PTR [rax+0x5c]
; crossPairs :: Node.summary +56 bytes from its node
  9ae51: mov    r11,QWORD PTR [rax+0x38]
  9ae63: test   QWORD PTR [rdx+0x38],r11
; crossPairs :: Node.summary +64 bytes from its node
  9ae55: test   QWORD PTR [rdx+0x40],r11
  9ae5f: mov    r11,QWORD PTR [rax+0x40]
; crossPairs :: Node.head +96 bytes from its node
  9ae6d: mov    ebx,DWORD PTR [rax+0x60]
  9aed0: mov    esi,DWORD PTR [rbp+0x60]
; crossPairs :: Node.ownCount +104 bytes from its node
  9af40: add    eax,DWORD PTR [rbp+0x68]
  9b24b: add    eax,DWORD PTR [rbx+0x68]
; crossPairs :: Node.linear +117 bytes from its node
  9af32: cmp    BYTE PTR [rbp+0x75],0x0
  9af50: cmp    BYTE PTR [rax+0x75],0x0
; walkOrdered :: Node.count +100 bytes from its node
  1e7b7: mov    r13d,DWORD PTR [r8+0x64]
  1e800: mov    r9d,DWORD PTR [r14+0xdc]
  1e850: mov    r8d,DWORD PTR [r14+0x154]
  1e8a9: mov    edi,DWORD PTR [r14+0x1cc]
  1e908: mov    ecx,DWORD PTR [r14+0x1cc]
  1e9c0: mov    esi,DWORD PTR [r14+0x1cc]
; walkOrdered :: Node.bounds +20 bytes from its node
  1e7d0: mov    eax,DWORD PTR [r8+0x14]
  1e81c: mov    eax,DWORD PTR [r14+0x8c]
  1e87a: cmp    eax,DWORD PTR [r14+0x104]
  1e8cc: cmp    eax,DWORD PTR [r14+0x17c]
  1ea0b: cmp    DWORD PTR [rax+0x14],edi
; walkOrdered :: Node.bounds +24 bytes from its node
  1e7f1: mov    eax,DWORD PTR [r8+0x18]
  1e839: mov    edi,DWORD PTR [r14+0x90]
  1e898: mov    edi,DWORD PTR [r14+0x108]
  1ea25: cmp    DWORD PTR [rax+0x18],edi
; walkOrdered :: Node.bounds +28 bytes from its node
  1e7c6: cmp    DWORD PTR [r8+0x1c],eax
  1e813: cmp    DWORD PTR [r14+0x94],eax
  1e869: cmp    DWORD PTR [r14+0x10c],eax
  1e8c3: cmp    DWORD PTR [r14+0x184],edi
  1e9f7: mov    edi,DWORD PTR [rax+0x1c]
; walkOrdered :: Node.bounds +32 bytes from its node
  1e7e4: cmp    DWORD PTR [r8+0x20],eax
  1e82d: cmp    DWORD PTR [r14+0x98],eax
  1e88b: cmp    DWORD PTR [r14+0x110],edi
  1ea14: mov    edi,DWORD PTR [rax+0x20]
; walkOrdered :: Node.firstChild +92 bytes from its node
  1e78e: mov    eax,DWORD PTR [r14+0x5c]
; walkOrdered :: Node.summary +56 bytes from its node
  1e678: test   QWORD PTR [rsi+0x38],r8
; walkOrdered :: Node.summary +64 bytes from its node
  1e66e: test   QWORD PTR [rsi+0x40],rcx
; walkOrdered :: Node.head +96 bytes from its node
  1e687: mov    r13d,DWORD PTR [rsi+0x60]
  1e77c: add    eax,DWORD PTR [r14+0x60]
; walkOrdered :: Node.ownCount +104 bytes from its node
  1e774: mov    eax,DWORD PTR [r14+0x68]
; walkOrdered :: Node.linear +117 bytes from its node
  1e769: cmp    BYTE PTR [r14+0x75],0x0
