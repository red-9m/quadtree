; Control Release default-predicate exact cross helper
   11ca0:	mov    edx,edx
   11ca2:	mov    esi,esi
   11ca4:	mov    rax,QWORD PTR [rdi+0x60]
   11ca8:	mov    r9,rcx
   11cab:	mov    r8,rsi
   11cae:	mov    rcx,rdx
   11cb1:	shl    r8,0x5
   11cb5:	shl    rcx,0x5
   11cb9:	add    rcx,rax
   11cbc:	add    rax,r8
   11cbf:	mov    r8,QWORD PTR [rax]
   11cc2:	test   QWORD PTR [rcx+0x8],r8
   11cc6:	je     11d60
   11ccc:	mov    rax,QWORD PTR [rax+0x8]
   11cd0:	test   QWORD PTR [rcx],rax
   11cd3:	je     11d60
   11cd9:	mov    rax,QWORD PTR [r9+0x30]
   11cdd:	test   rax,rax
   11ce0:	je     11d78
   11ce6:	cmp    rax,0xffffffffffffffff
   11cea:	je     11cf4
   11cec:	sub    rax,0x1
   11cf0:	mov    QWORD PTR [r9+0x30],rax
   11cf4:	mov    rax,QWORD PTR [r9+0x10]
   11cf8:	cmp    rax,0xffffffffffffffff
   11cfc:	je     11d06
   11cfe:	add    rax,0x1
   11d02:	mov    QWORD PTR [r9+0x10],rax
   11d06:	mov    rax,QWORD PTR [rdi+0x50]
   11d0a:	shl    rsi,0x4
   11d0e:	shl    rdx,0x4
   11d12:	add    rdx,rax
   11d15:	add    rax,rsi
   11d18:	mov    r8d,DWORD PTR [rax+0x4]
   11d1c:	mov    r10d,DWORD PTR [rax+0xc]
   11d20:	mov    ecx,DWORD PTR [rax]
   11d22:	mov    esi,DWORD PTR [rax+0x8]
   11d25:	cmp    r8d,r10d
   11d28:	mov    r9d,DWORD PTR [rdx+0x8]
   11d2c:	sete   al
   11d2f:	cmp    ecx,esi
   11d31:	sete   r11b
   11d35:	or     al,r11b
   11d38:	jne    11d40
   11d3a:	cmp    BYTE PTR [rdi+0x19],0x1
   11d3e:	jne    11d80
   11d40:	cmp    ecx,r9d
   11d43:	jg     11d72
   11d45:	cmp    esi,DWORD PTR [rdx]
   11d47:	jl     11d72
   11d49:	cmp    r8d,DWORD PTR [rdx+0xc]
   11d4d:	jg     11d72
   11d4f:	cmp    r10d,DWORD PTR [rdx+0x4]
   11d53:	setge  al
   11d56:	ret
   11d57:	nop    WORD PTR [rax+rax*1+0x0]
   11d60:	mov    rax,QWORD PTR [r9+0x18]
   11d64:	cmp    rax,0xffffffffffffffff
   11d68:	je     11d72
   11d6a:	add    rax,0x1
   11d6e:	mov    QWORD PTR [r9+0x18],rax
   11d72:	xor    eax,eax
   11d74:	ret
   11d75:	nop    DWORD PTR [rax]
   11d78:	mov    BYTE PTR [r9],0xb
   11d7c:	jmp    11d72
   11d7e:	xchg   ax,ax
   11d80:	mov    eax,DWORD PTR [rdx]
   11d82:	cmp    eax,r9d
   11d85:	je     11d40
   11d87:	mov    edi,DWORD PTR [rdx+0x4]
   11d8a:	mov    r11d,DWORD PTR [rdx+0xc]
   11d8e:	cmp    edi,r11d
   11d91:	je     11d40
   11d93:	cmp    esi,eax
   11d95:	setg   al
   11d98:	cmp    ecx,r9d
   11d9b:	setl   dl
   11d9e:	and    eax,edx
   11da0:	cmp    r8d,r11d
   11da3:	setl   dl
   11da6:	and    eax,edx
   11da8:	cmp    r10d,edi
   11dab:	setg   dl
   11dae:	and    eax,edx
   11db0:	ret
