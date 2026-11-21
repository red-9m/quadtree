   11c3a:	48 89 c2             	mov    rdx,rax
   11c3d:	48 c1 e2 04          	shl    rdx,0x4
   11c41:	48 03 57 70          	add    rdx,QWORD PTR [rdi+0x70]
   11c45:	0f 18 0a             	prefetcht0 BYTE PTR [rdx]
   11c48:	c3                   	ret
   11c3a:	48 89 c2             	mov    rdx,rax
   11c3d:	48 c1 e2 04          	shl    rdx,0x4
   11c41:	48 03 57 70          	add    rdx,QWORD PTR [rdi+0x70]
   11c45:	0f 1f 00             	nop    DWORD PTR [rax]
   11c48:	c3                   	ret
