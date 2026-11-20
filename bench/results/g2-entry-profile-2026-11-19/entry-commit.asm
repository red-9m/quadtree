# Retained int32 spatial-order entry loop
   e375d:	4b 8d 04 40          	lea    rax,[r8+r8*2]
   e3761:	44 89 c6             	mov    esi,r8d
   e3764:	4c 89 df             	mov    rdi,r11
   e3767:	4c 89 44 24 20       	mov    QWORD PTR [rsp+0x20],r8
   e376c:	49 8b 91 a0 00 00 00 	mov    rdx,QWORD PTR [r9+0xa0]
   e3773:	4c 89 4c 24 10       	mov    QWORD PTR [rsp+0x10],r9
   e3778:	4c 89 1c 24          	mov    QWORD PTR [rsp],r11
   e377c:	4c 8d 14 c2          	lea    r10,[rdx+rax*8]
   e3780:	41 8b 42 14          	mov    eax,DWORD PTR [r10+0x14]
   e3784:	4c 89 54 24 50       	mov    QWORD PTR [rsp+0x50],r10
   e3789:	48 6b c0 38          	imul   rax,rax,0x38
   e378d:	49 8d 1c 07          	lea    rbx,[r15+rax*1]
   e3791:	0f b6 4b 28          	movzx  ecx,BYTE PTR [rbx+0x28]
   e3795:	48 89 da             	mov    rdx,rbx
   e3798:	88 4c 24 08          	mov    BYTE PTR [rsp+0x8],cl
   e379c:	e8 5f 98 05 00       	call   13d000
   e37a1:	4c 8b 1c 24          	mov    r11,QWORD PTR [rsp]
   e37a5:	4c 8b 54 24 50       	mov    r10,QWORD PTR [rsp+0x50]
   e37aa:	4c 8b 4c 24 10       	mov    r9,QWORD PTR [rsp+0x10]
   e37af:	4c 8b 44 24 20       	mov    r8,QWORD PTR [rsp+0x20]
   e37b4:	41 83 83 c8 00 00 00 01 	add    DWORD PTR [r11+0xc8],0x1
   e37bc:	41 83 83 d4 00 00 00 01 	add    DWORD PTR [r11+0xd4],0x1
   e37c4:	80 7c 24 08 00       	cmp    BYTE PTR [rsp+0x8],0x0
   e37c9:	0f 85 d7 00 00 00    	jne    e38a6
   e37cf:	45 89 42 14          	mov    DWORD PTR [r10+0x14],r8d
   e37d3:	49 83 c0 01          	add    r8,0x1
   e37d7:	4d 39 c6             	cmp    r14,r8
   e37da:	75 81                	jne    e375d

# constructBuildEntry<false>
  13d000:	48 89 d0             	mov    rax,rdx
  13d003:	41 89 c8             	mov    r8d,ecx
  13d006:	89 f2                	mov    edx,esi
  13d008:	48 8b 48 30          	mov    rcx,QWORD PTR [rax+0x30]
  13d00c:	44 8b 09             	mov    r9d,DWORD PTR [rcx]
  13d00f:	48 8b 4f 58          	mov    rcx,QWORD PTR [rdi+0x58]
  13d013:	44 89 0c 91          	mov    DWORD PTR [rcx+rdx*4],r9d
  13d017:	48 89 d1             	mov    rcx,rdx
  13d01a:	f3 0f 6f 00          	movdqu xmm0,XMMWORD PTR [rax]
  13d01e:	48 c1 e1 04          	shl    rcx,0x4
  13d022:	48 03 4f 50          	add    rcx,QWORD PTR [rdi+0x50]
  13d026:	0f 11 01             	movups XMMWORD PTR [rcx],xmm0
  13d029:	31 c9                	xor    ecx,ecx
  13d02b:	f3 0f 6f 40 10       	movdqu xmm0,XMMWORD PTR [rax+0x10]
  13d030:	45 84 c0             	test   r8b,r8b
  13d033:	74 04                	je     13d039
  13d035:	48 8b 48 20          	mov    rcx,QWORD PTR [rax+0x20]
  13d039:	48 89 d0             	mov    rax,rdx
  13d03c:	48 c1 e0 05          	shl    rax,0x5
  13d040:	48 03 47 60          	add    rax,QWORD PTR [rdi+0x60]
  13d044:	48 89 48 10          	mov    QWORD PTR [rax+0x10],rcx
  13d048:	89 70 18             	mov    DWORD PTR [rax+0x18],esi
  13d04b:	44 88 40 1c          	mov    BYTE PTR [rax+0x1c],r8b
  13d04f:	0f 11 00             	movups XMMWORD PTR [rax],xmm0
  13d052:	48 8b 47 68          	mov    rax,QWORD PTR [rdi+0x68]
  13d056:	c7 04 d0 00 00 00 00 	mov    DWORD PTR [rax+rdx*8],0x0
  13d05d:	89 74 d0 04          	mov    DWORD PTR [rax+rdx*8+0x4],esi
  13d061:	c3                   	ret
