# Candidate: int32 bulk commit with the bounded prefetch path
   e32e5:	42 8b 94 10 14 03 00 00 	mov    edx,DWORD PTR [rax+r10*1+0x314]
   e32ed:	48 6b d2 38          	imul   rdx,rdx,0x38
   e32f1:	4c 01 fa             	add    rdx,r15
   e32f4:	0f 18 0a             	prefetcht0 BYTE PTR [rdx]
   e32f7:	0f 18 4a 30          	prefetcht0 BYTE PTR [rdx+0x30]
   e32fb:	42 8b 94 10 94 01 00 00 	mov    edx,DWORD PTR [rax+r10*1+0x194]
   e3303:	48 6b d2 38          	imul   rdx,rdx,0x38
   e3307:	49 8b 54 17 30       	mov    rdx,QWORD PTR [r15+rdx*1+0x30]
   e330c:	0f 18 0a             	prefetcht0 BYTE PTR [rdx]
   e330f:	4c 01 d0             	add    rax,r10
   e3312:	44 89 ce             	mov    esi,r9d
   e3315:	4c 89 44 24 50       	mov    QWORD PTR [rsp+0x50],r8
   e331a:	8b 78 14             	mov    edi,DWORD PTR [rax+0x14]
   e331d:	4c 89 54 24 30       	mov    QWORD PTR [rsp+0x30],r10
   e3322:	48 89 44 24 20       	mov    QWORD PTR [rsp+0x20],rax
   e3327:	48 6b d7 38          	imul   rdx,rdi,0x38
   e332b:	49 89 fe             	mov    r14,rdi
   e332e:	48 89 df             	mov    rdi,rbx
   e3331:	44 89 4c 24 10       	mov    DWORD PTR [rsp+0x10],r9d
   e3336:	4c 01 fa             	add    rdx,r15
   e3339:	0f b6 4a 28          	movzx  ecx,BYTE PTR [rdx+0x28]
   e333d:	48 89 14 24          	mov    QWORD PTR [rsp],rdx
   e3341:	88 4c 24 18          	mov    BYTE PTR [rsp+0x18],cl
   e3345:	e8 b6 9f 05 00       	call   13d300
   e334a:	83 83 c8 00 00 00 01 	add    DWORD PTR [rbx+0xc8],0x1
   e3351:	44 8b 4c 24 10       	mov    r9d,DWORD PTR [rsp+0x10]
   e3356:	83 83 d4 00 00 00 01 	add    DWORD PTR [rbx+0xd4],0x1
   e335d:	80 7c 24 18 00       	cmp    BYTE PTR [rsp+0x18],0x0
   e3362:	48 8b 14 24          	mov    rdx,QWORD PTR [rsp]
   e3366:	48 8b 44 24 20       	mov    rax,QWORD PTR [rsp+0x20]
   e336b:	4c 8b 54 24 30       	mov    r10,QWORD PTR [rsp+0x30]
   e3370:	4c 8b 44 24 50       	mov    r8,QWORD PTR [rsp+0x50]
   e3375:	0f 85 b5 01 00 00    	jne    e3530
   e337b:	41 f6 c4 01          	test   r12b,0x1
   e337f:	74 1f                	je     e33a0
   e3381:	48 8b 7c 24 58       	mov    rdi,QWORD PTR [rsp+0x58]
   e3386:	4c 89 f2             	mov    rdx,r14
   e3389:	48 c1 e2 04          	shl    rdx,0x4
   e338d:	49 89 7c 15 00       	mov    QWORD PTR [r13+rdx*1+0x0],rdi
   e3392:	45 89 4c 15 08       	mov    DWORD PTR [r13+rdx*1+0x8],r9d
   e3397:	41 c7 44 15 0c 00 00 00 00 	mov    DWORD PTR [r13+rdx*1+0xc],0x0
   e33a0:	44 89 48 14          	mov    DWORD PTR [rax+0x14],r9d
   e33a4:	49 83 c2 18          	add    r10,0x18
   e33a8:	49 83 e8 01          	sub    r8,0x1
   e33ac:	0f 84 63 05 00 00    	je     e3915
   e33b2:	48 8b 44 24 40       	mov    rax,QWORD PTR [rsp+0x40]
   e33b7:	41 89 e9             	mov    r9d,ebp
   e33ba:	45 29 c1             	sub    r9d,r8d
   e33bd:	48 8b 80 a0 00 00 00 	mov    rax,QWORD PTR [rax+0xa0]
   e33c4:	49 83 f8 20          	cmp    r8,0x20
   e33c8:	0f 87 17 ff ff ff    	ja     e32e5
   e33ce:	49 83 f8 10          	cmp    r8,0x10
   e33d2:	0f 86 37 ff ff ff    	jbe    e330f
   e33d8:	e9 1e ff ff ff       	jmp    e32fb
   e33dd:	0f 1f 00             	nop    DWORD PTR [rax]
   e33e0:	8b 58 08             	mov    ebx,DWORD PTR [rax+0x8]
