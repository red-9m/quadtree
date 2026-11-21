; GCC x86-64 Release, int32 ordered build; objdump -d -Mintel --no-show-raw-insn.

; Prepare home: mix future rank, mask and prefetch
   11d60:	mov    rax,QWORD PTR [rdx]
   11d63:	mov    rdx,QWORD PTR [rsi]
   11d66:	mov    eax,DWORD PTR [rax+rcx*4]
   11d69:	lea    rax,[rax+rax*4]
   11d6d:	lea    rax,[rdx+rax*8]
   11d71:	mov    rax,QWORD PTR [rax+0x10]
   11d75:	mov    rdx,rax
   11d78:	shr    rdx,0x1e
   11d7c:	xor    rdx,rax
   11d7f:	movabs rax,0xbf58476d1ce4e5b9
   11d89:	imul   rdx,rax
   11d8d:	mov    rax,rdx
   11d90:	shr    rax,0x1b
   11d94:	xor    rdx,rax
   11d97:	movabs rax,0x94d049bb133111eb
   11da1:	imul   rdx,rax
   11da5:	mov    rax,QWORD PTR [rdi+0x78]
   11da9:	sub    rax,0x1
   11dad:	mov    rcx,rdx
   11db0:	shr    rcx,0x1f
   11db4:	xor    rdx,rcx
   11db7:	and    rax,rdx
   11dba:	mov    rdx,rax
   11dbd:	shl    rdx,0x4
   11dc1:	add    rdx,QWORD PTR [rdi+0x70]
   11dc5:	prefetcht0 BYTE PTR [rdx]
   11dc8:	ret

; Consume ring slot and check tail
   d2d75:	mov    r8,r15
   d2d78:	mov    rax,r14
   d2d7b:	and    r8d,0x1f
   d2d7f:	sub    rax,r15
   d2d82:	mov    r9,QWORD PTR [rsp+r8*8+0x190]
   d2d8a:	cmp    rax,0x20
   d2d8e:	ja     d2ed7

; Probe from queued home and publish
   d2e40:	add    r9,0x1
   d2e44:	lea    rax,[rdx+0x1]
   d2e48:	and    r9,r8
   d2e4b:	cmp    rax,rsi
   d2e4e:	je     d300b
   d2e54:	mov    rdx,rax
   d2e57:	mov    rax,r9
   d2e5a:	shl    rax,0x4
   d2e5e:	add    rax,r11
   d2e61:	cmp    DWORD PTR [rax+0x8],edi
   d2e64:	je     d2e40
   d2e66:	mov    rcx,QWORD PTR [rcx+0x10]
   d2e6a:	mov    QWORD PTR [r10+0xa0],rdx
   d2e71:	mov    DWORD PTR [rax+0x8],edi
   d2e74:	mov    QWORD PTR [rax],rcx
   d2e77:	mov    DWORD PTR [rax+0xc],0x0

; Refresh slot for i + 32
   d2ed7:	lea    rcx,[r15+0x20]
   d2edb:	lea    rdx,[rsp+0xc8]
   d2ee3:	mov    rdi,r10
   d2ee6:	lea    rsi,[rsp+0xc0]
   d2eee:	call   11d60
   d2ef3:	mov    QWORD PTR [rsp+r8*8+0x190],rax
   d2efb:	jmp    d2d94
