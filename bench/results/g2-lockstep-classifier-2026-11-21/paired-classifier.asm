; GCC 16.2.1 Release planBulk<int32>, excerpt from paired classification.
; Upper histogram update at 146d35, lower at 146cc2; loop exits at 146d40.
  146cc2:	add    DWORD PTR [rsp+rax*4+0x120],0x1
  146cca:	mov    eax,r8d
  146ccd:	sub    eax,ecx
  146ccf:	mov    BYTE PTR [r10],sil
  146cd2:	cmp    eax,0x10
  146cd5:	jbe    146ce5
  146cd7:	lea    eax,[rcx+0x10]
  146cda:	mov    eax,DWORD PTR [rdi+rax*4]
  146cdd:	shl    rax,0x4
  146ce1:	prefetcht0 BYTE PTR [rbx+rax*1]
  146ce5:	mov    r11d,ecx
  146ce8:	mov    eax,DWORD PTR [rdi+r11*4]
  146cec:	shl    rax,0x4
  146cf0:	add    rax,rbx
  146cf3:	mov    ebp,DWORD PTR [rax]
  146cf5:	mov    esi,DWORD PTR [rax+0x8]
  146cf8:	mov    r12d,DWORD PTR [rax+0xc]
  146cfc:	sub    r12d,DWORD PTR [rax+0x4]
  146d00:	sub    esi,ebp
  146d02:	cmp    edx,r12d
  146d05:	jb     146dd8
  146d0b:	cmp    r14d,esi
  146d0e:	jb     146dd8
  146d14:	cmp    BYTE PTR [rsp+0x28],0x0
  146d19:	jne    146de8
  146d1f:	xor    eax,eax
  146d21:	add    eax,0x1
  146d24:	mov    esi,eax
  146d26:	add    ecx,0x1
  146d29:	mov    BYTE PTR [r15+r11*1],sil
  146d2d:	add    r10,0x1
  146d31:	add    r9,0x4
  146d35:	add    DWORD PTR [rsp+rax*4+0x100],0x1
  146d3d:	cmp    r13d,ecx
  146d40:	je     146e18
  146d46:	mov    eax,DWORD PTR [rsp+0x40]
  146d4a:	sub    eax,ecx
  146d4c:	cmp    eax,0x10
  146d4f:	jbe    146d64
  146d51:	mov    eax,DWORD PTR [rsp+0x48]
  146d55:	add    eax,ecx
  146d57:	mov    eax,eax
  146d59:	mov    eax,DWORD PTR [rdi+rax*4]
  146d5c:	shl    rax,0x4
  146d60:	prefetcht0 BYTE PTR [rbx+rax*1]
  146d64:	mov    eax,DWORD PTR [r9]
  146d67:	shl    rax,0x4
  146d6b:	add    rax,rbx
  146d6e:	mov    r11d,DWORD PTR [rax]
  146d71:	mov    esi,DWORD PTR [rax+0x8]
  146d74:	mov    ebp,DWORD PTR [rax+0xc]
  146d77:	sub    ebp,DWORD PTR [rax+0x4]
  146d7a:	sub    esi,r11d
  146d7d:	cmp    edx,ebp
  146d7f:	jb     146d8a
  146d81:	cmp    r14d,esi
  146d84:	.byte 0xf
