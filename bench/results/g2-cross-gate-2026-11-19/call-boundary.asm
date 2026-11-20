# Candidate: unordered int32 crossPairsOverlapping body, child entry gate
   9d9d8:	8b 45 64             	mov    0x64(%rbp),%eax
   9d9db:	85 c0                	test   %eax,%eax
   9d9dd:	74 51                	je     9da30
   9d9df:	89 d8                	mov    %ebx,%eax
   9d9e1:	48 6b c0 78          	imul   $0x78,%rax,%rax
   9d9e5:	4c 01 e0             	add    %r12,%rax
   9d9e8:	8b 50 64             	mov    0x64(%rax),%edx
   9d9eb:	85 d2                	test   %edx,%edx
   9d9ed:	74 41                	je     9da30
   9d9ef:	49 8b 57 38          	mov    0x38(%r15),%rdx
   9d9f3:	48 85 d2             	test   %rdx,%rdx
   9d9f6:	0f 84 bc 01 00 00    	je     9dbb8
   9d9fc:	48 83 fa ff          	cmp    $0xffffffffffffffff,%rdx
   9da00:	74 08                	je     9da0a
   9da02:	48 83 ea 01          	sub    $0x1,%rdx
   9da06:	49 89 57 38          	mov    %rdx,0x38(%r15)
   9da0a:	49 8b 57 08          	mov    0x8(%r15),%rdx
   9da0e:	48 83 fa ff          	cmp    $0xffffffffffffffff,%rdx
   9da12:	74 08                	je     9da1c
   9da14:	48 83 c2 01          	add    $0x1,%rdx
   9da18:	49 89 57 08          	mov    %rdx,0x8(%r15)
   9da1c:	8b 50 1c             	mov    0x1c(%rax),%edx
   9da1f:	39 55 14             	cmp    %edx,0x14(%rbp)
   9da22:	0f 8e 1e 01 00 00    	jle    9db46
   9da28:	0f 1f 84 00 00 00 00 00 	nopl   0x0(%rax,%rax,1)
   9da30:	83 c3 01             	add    $0x1,%ebx
   9da33:	41 39 de             	cmp    %ebx,%r14d
   9da36:	74 06                	je     9da3e
   9da38:	41 80 3f 00          	cmpb   $0x0,(%r15)
   9da3c:	74 9a                	je     9d9d8
   9db46:	8b 55 1c             	mov    0x1c(%rbp),%edx
   9db49:	39 50 14             	cmp    %edx,0x14(%rax)
   9db4c:	0f 8f de fe ff ff    	jg     9da30
   9db52:	8b 50 20             	mov    0x20(%rax),%edx
   9db55:	39 55 18             	cmp    %edx,0x18(%rbp)
   9db58:	0f 8f d2 fe ff ff    	jg     9da30
   9db5e:	8b 55 20             	mov    0x20(%rbp),%edx
   9db61:	39 50 18             	cmp    %edx,0x18(%rax)
   9db64:	0f 8f c6 fe ff ff    	jg     9da30
   9db6a:	48 8b 50 40          	mov    0x40(%rax),%rdx
   9db6e:	48 85 55 38          	test   %rdx,0x38(%rbp)
   9db72:	74 4d                	je     9dbc1
   9db74:	48 8b 40 38          	mov    0x38(%rax),%rax
   9db78:	48 85 45 40          	test   %rax,0x40(%rbp)
   9db7c:	74 43                	je     9dbc1
   9db7e:	4c 8b 4c 24 50       	mov    0x50(%rsp),%r9
   9db83:	4c 8b 44 24 20       	mov    0x20(%rsp),%r8
   9db88:	4c 89 f9             	mov    %r15,%rcx
   9db8b:	89 da                	mov    %ebx,%edx
   9db8d:	44 89 54 24 1c       	mov    %r10d,0x1c(%rsp)
   9db92:	89 74 24 10          	mov    %esi,0x10(%rsp)
   9db96:	48 89 7c 24 08       	mov    %rdi,0x8(%rsp)
   9db9b:	e8 70 fa ff ff       	call   9d610
   9dba0:	48 8b 7c 24 08       	mov    0x8(%rsp),%rdi
   9dba5:	8b 74 24 10          	mov    0x10(%rsp),%esi
   9dba9:	44 8b 54 24 1c       	mov    0x1c(%rsp),%r10d
   9dbae:	e9 7d fe ff ff       	jmp    9da30
   9dbb3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
   9dbb8:	41 c6 07 0b          	movb   $0xb,(%r15)
   9dbbc:	e9 6f fe ff ff       	jmp    9da30
   9dbc1:	49 8b 47 18          	mov    0x18(%r15),%rax
   9dbc5:	48 83 f8 ff          	cmp    $0xffffffffffffffff,%rax
   9dbc9:	0f 84 61 fe ff ff    	je     9da30
   9dbcf:	48 83 c0 01          	add    $0x1,%rax
   9dbd3:	49 89 47 18          	mov    %rax,0x18(%r15)
   9dbd7:	e9 54 fe ff ff       	jmp    9da30

# Candidate: unordered int32 selfPairs body, sibling entry gate
   9e0e8:	8b 79 64             	mov    0x64(%rcx),%edi
   9e0eb:	85 ff                	test   %edi,%edi
   9e0ed:	0f 84 5d 01 00 00    	je     9e250
   9e0f3:	48 8b 7b 38          	mov    0x38(%rbx),%rdi
   9e0f7:	48 85 ff             	test   %rdi,%rdi
   9e0fa:	0f 84 10 01 00 00    	je     9e210
   9e100:	48 83 ff ff          	cmp    $0xffffffffffffffff,%rdi
   9e104:	74 08                	je     9e10e
   9e106:	48 83 ef 01          	sub    $0x1,%rdi
   9e10a:	48 89 7b 38          	mov    %rdi,0x38(%rbx)
   9e10e:	48 8b 7b 08          	mov    0x8(%rbx),%rdi
   9e112:	48 83 ff ff          	cmp    $0xffffffffffffffff,%rdi
   9e116:	74 08                	je     9e120
   9e118:	48 83 c7 01          	add    $0x1,%rdi
   9e11c:	48 89 7b 08          	mov    %rdi,0x8(%rbx)
   9e120:	8b 79 1c             	mov    0x1c(%rcx),%edi
   9e123:	41 39 7e 14          	cmp    %edi,0x14(%r14)
   9e127:	0f 8f 23 01 00 00    	jg     9e250
   9e12d:	41 8b 7e 1c          	mov    0x1c(%r14),%edi
   9e131:	39 79 14             	cmp    %edi,0x14(%rcx)
   9e134:	0f 8f 16 01 00 00    	jg     9e250
   9e13a:	8b 79 20             	mov    0x20(%rcx),%edi
   9e13d:	41 39 7e 18          	cmp    %edi,0x18(%r14)
   9e141:	0f 8f 09 01 00 00    	jg     9e250
   9e147:	41 8b 7e 20          	mov    0x20(%r14),%edi
   9e14b:	39 79 18             	cmp    %edi,0x18(%rcx)
   9e14e:	0f 8f fc 00 00 00    	jg     9e250
   9e154:	49 8b 7e 38          	mov    0x38(%r14),%rdi
   9e158:	48 85 79 40          	test   %rdi,0x40(%rcx)
   9e15c:	0f 84 4c 01 00 00    	je     9e2ae
   9e162:	49 8b 7e 40          	mov    0x40(%r14),%rdi
   9e166:	48 85 79 38          	test   %rdi,0x38(%rcx)
   9e16a:	0f 84 3e 01 00 00    	je     9e2ae
   9e170:	4d 89 f8             	mov    %r15,%r8
   9e173:	48 89 d9             	mov    %rbx,%rcx
   9e176:	48 89 ef             	mov    %rbp,%rdi
   9e179:	41 83 c5 01          	add    $0x1,%r13d
   9e17d:	44 88 5c 24 30       	mov    %r11b,0x30(%rsp)
   9e182:	44 89 54 24 18       	mov    %r10d,0x18(%rsp)
   9e187:	89 44 24 10          	mov    %eax,0x10(%rsp)
   9e18b:	4c 89 4c 24 08       	mov    %r9,0x8(%rsp)
   9e190:	89 34 24             	mov    %esi,(%rsp)
   9e193:	e8 78 f4 ff ff       	call   9d610
   9e198:	41 83 fd 04          	cmp    $0x4,%r13d
