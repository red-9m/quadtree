# Control: runner prologue, warm build call, buildPrepared prologue
   e4e00:	48 81 ec d8 04 00 00 	sub    $0x4d8,%rsp
   e5631:	48 83 ec 20          	sub    $0x20,%rsp
   e5635:	48 8d 8b 88 01 00 00 	lea    0x188(%rbx),%rcx
   e563c:	48 8d b3 58 02 00 00 	lea    0x258(%rbx),%rsi
   e5643:	66 0f 6f 84 24 30 03 00 00 	movdqa 0x330(%rsp),%xmm0
   e564c:	48 8d 7b 08          	lea    0x8(%rbx),%rdi
   e5650:	48 c7 44 24 10 00 00 00 00 	movq   $0x0,0x10(%rsp)
   e5659:	0f 11 04 24          	movups %xmm0,(%rsp)
   e565d:	e8 6e d3 ff ff       	call   e29d0
   e5662:	48 83 c4 20          	add    $0x20,%rsp
   e5666:	84 c0                	test   %al,%al
   e29d0:	48 81 ec 58 01 00 00 	sub    $0x158,%rsp
   e29d7:	48 89 9c 24 28 01 00 00 	mov    %rbx,0x128(%rsp)
   e29df:	64 48 8b 04 25 28 00 00 00 	mov    %fs:0x28,%rax
   e29e8:	48 89 84 24 18 01 00 00 	mov    %rax,0x118(%rsp)

# Outlined: runner prologue, warm build call, singleton prologue and workspace entry
   e4f40:	48 81 ec d8 04 00 00 	sub    $0x4d8,%rsp
   e577f:	48 83 fb 01          	cmp    $0x1,%rbx
   e5783:	0f 85 3f 02 00 00    	jne    e59c8
   e5789:	48 89 ca             	mov    %rcx,%rdx
   e578c:	48 89 4c 24 50       	mov    %rcx,0x50(%rsp)
   e5791:	48 89 7c 24 40       	mov    %rdi,0x40(%rsp)
   e5796:	e8 a5 9d 05 00       	call   13f540
   e579b:	48 8b 4c 24 50       	mov    0x50(%rsp),%rcx
   e57a0:	89 c7                	mov    %eax,%edi
   e57a2:	66 c1 ef 08          	shr    $0x8,%di
   e57a6:	48 8b 7c 24 40       	mov    0x40(%rsp),%rdi
   e57ab:	0f 84 17 02 00 00    	je     e59c8
  13f569:	48 83 ba 90 00 00 00 00 	cmpq   $0x0,0x90(%rdx)
  13f571:	74 6d                	je     13f5e0
  13f573:	48 83 ba a8 00 00 00 00 	cmpq   $0x0,0xa8(%rdx)
  13f57b:	74 63                	je     13f5e0
  13f57d:	48 83 ba c0 00 00 00 00 	cmpq   $0x0,0xc0(%rdx)
  13f585:	74 59                	je     13f5e0
  13f587:	48 89 d1             	mov    %rdx,%rcx
  13f58a:	48 83 ec 38          	sub    $0x38,%rsp
  13f58e:	48 89 fa             	mov    %rdi,%rdx
  13f591:	80 b9 cc 00 00 00 00 	cmpb   $0x0,0xcc(%rcx)
  13f598:	0f 85 8c 01 00 00    	jne    13f72a
  13f59e:	c6 81 cc 00 00 00 01 	movb   $0x1,0xcc(%rcx)
  13f5a5:	80 7e 30 00          	cmpb   $0x0,0x30(%rsi)
  13f605:	f0 48 0f b1 3d 72 3c 02 00 	lock cmpxchg %rdi,0x23c72(%rip)        # 163280

# Inline: runner prologue, size branch, eligibility checks and workspace entry
   e5260:	48 81 ec 98 04 00 00 	sub    $0x498,%rsp
   e5a31:	48 83 fa 01          	cmp    $0x1,%rdx
   e5a35:	75 19                	jne    e5a50
   e5a37:	44 8b 5b 2c          	mov    0x2c(%rbx),%r11d
   e5a3b:	45 85 db             	test   %r11d,%r11d
   e5a3e:	0f 85 e1 05 00 00    	jne    e6025
   e6025:	44 8b 93 d4 00 00 00 	mov    0xd4(%rbx),%r10d
   e602c:	45 85 d2             	test   %r10d,%r10d
   e602f:	0f 84 1b fa ff ff    	je     e5a50
   e6035:	44 8b 8b d8 00 00 00 	mov    0xd8(%rbx),%r9d
   e603c:	45 85 c9             	test   %r9d,%r9d
   e603f:	0f 84 0b fa ff ff    	je     e5a50
   e6045:	80 7b 08 00          	cmpb   $0x0,0x8(%rbx)
   e6049:	0f 84 01 fa ff ff    	je     e5a50
   e604f:	48 83 bb 18 02 00 00 00 	cmpq   $0x0,0x218(%rbx)
   e6057:	0f 84 10 01 00 00    	je     e616d
   e605d:	48 83 bb 30 02 00 00 00 	cmpq   $0x0,0x230(%rbx)
   e6065:	0f 84 02 01 00 00    	je     e616d
   e606b:	48 83 bb 48 02 00 00 00 	cmpq   $0x0,0x248(%rbx)
   e6073:	0f 84 f4 00 00 00    	je     e616d
   e6079:	80 bb 54 02 00 00 00 	cmpb   $0x0,0x254(%rbx)
   e6080:	0f 85 31 02 00 00    	jne    e62b7
   e6086:	c6 83 54 02 00 00 01 	movb   $0x1,0x254(%rbx)
   e608d:	41 80 7c 24 30 00    	cmpb   $0x0,0x30(%r12)
   e6093:	0f 84 de 00 00 00    	je     e6177
   e6099:	c6 83 54 02 00 00 00 	movb   $0x0,0x254(%rbx)
   e60a0:	e9 ab f9 ff ff       	jmp    e5a50

# Inline: identity acquisition and successful commit, then branch directly to pairs
   e6177:	41 8b 44 24 08       	mov    0x8(%r12),%eax
   e617c:	41 39 04 24          	cmp    %eax,(%r12)
   e6180:	7f 0c                	jg     e618e
   e6182:	41 8b 44 24 0c       	mov    0xc(%r12),%eax
   e6187:	41 39 44 24 04       	cmp    %eax,0x4(%r12)
   e618c:	7e 11                	jle    e619f
   e618e:	c6 83 54 02 00 00 00 	movb   $0x0,0x254(%rbx)
   e6195:	ba 02 00 00 00       	mov    $0x2,%edx
   e619a:	e9 b6 fc ff ff       	jmp    e5e55
   e619f:	f3 41 0f 6f 04 24    	movdqu (%r12),%xmm0
   e61a5:	48 8b 05 d4 d0 07 00 	mov    0x7d0d4(%rip),%rax        # 163280
   e61ac:	48 83 f8 ff          	cmp    $0xffffffffffffffff,%rax
   e61b0:	0f 84 f0 00 00 00    	je     e62a6
   e61b6:	48 8d 50 01          	lea    0x1(%rax),%rdx
   e61ba:	f0 48 0f b1 15 bd d0 07 00 	lock cmpxchg %rdx,0x7d0bd(%rip)        # 163280
   e61c3:	75 e7                	jne    e61ac
   e61c5:	48 83 c0 01          	add    $0x1,%rax
   e61c9:	48 89 c5             	mov    %rax,%rbp
   e61cc:	0f 84 d4 00 00 00    	je     e62a6
   e61d2:	48 83 bb 80 00 00 00 00 	cmpq   $0x0,0x80(%rbx)
   e61da:	0f 85 af 00 00 00    	jne    e628f
   e61e0:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
   e61e5:	45 31 c0             	xor    %r8d,%r8d
   e61e8:	48 89 83 e0 00 00 00 	mov    %rax,0xe0(%rbx)
   e61ef:	44 89 83 e8 00 00 00 	mov    %r8d,0xe8(%rbx)
   e61f6:	83 bb 64 01 00 00 ff 	cmpl   $0xffffffff,0x164(%rbx)
   e61fd:	74 1a                	je     e6219
   e61ff:	31 ff                	xor    %edi,%edi
   e6201:	48 89 83 64 01 00 00 	mov    %rax,0x164(%rbx)
   e6208:	48 89 bb 70 01 00 00 	mov    %rdi,0x170(%rbx)
   e620f:	c7 83 78 01 00 00 ff ff ff ff 	movl   $0xffffffff,0x178(%rbx)
   e6219:	80 bb 60 01 00 00 00 	cmpb   $0x0,0x160(%rbx)
   e6220:	0f 11 83 50 01 00 00 	movups %xmm0,0x150(%rbx)
   e6227:	75 07                	jne    e6230
   e6229:	c6 83 60 01 00 00 01 	movb   $0x1,0x160(%rbx)
   e6230:	48 8b 43 58          	mov    0x58(%rbx),%rax
   e6234:	31 c9                	xor    %ecx,%ecx
   e6236:	31 f6                	xor    %esi,%esi
   e6238:	0f 11 00             	movups %xmm0,(%rax)
   e623b:	48 8b 43 60          	mov    0x60(%rbx),%rax
   e623f:	41 8b 54 24 10       	mov    0x10(%r12),%edx
   e6244:	89 10                	mov    %edx,(%rax)
   e6246:	48 8b 43 68          	mov    0x68(%rbx),%rax
   e624a:	31 d2                	xor    %edx,%edx
   e624c:	f3 41 0f 6f 44 24 18 	movdqu 0x18(%r12),%xmm0
   e6253:	48 89 50 10          	mov    %rdx,0x10(%rax)
   e6257:	0f 11 00             	movups %xmm0,(%rax)
   e625a:	89 48 18             	mov    %ecx,0x18(%rax)
   e625d:	c6 40 1c 00          	movb   $0x0,0x1c(%rax)
   e6261:	48 8b 43 70          	mov    0x70(%rbx),%rax
   e6265:	48 89 30             	mov    %rsi,(%rax)
   e6268:	48 89 ab c8 00 00 00 	mov    %rbp,0xc8(%rbx)
   e626f:	c7 83 d0 00 00 00 01 00 00 00 	movl   $0x1,0xd0(%rbx)
   e6279:	c7 83 dc 00 00 00 01 00 00 00 	movl   $0x1,0xdc(%rbx)
   e6283:	c6 83 54 02 00 00 00 	movb   $0x0,0x254(%rbx)
   e628a:	e9 02 f8 ff ff       	jmp    e5a91
   e5a91:	48 83 7c 24 40 18    	cmpq   $0x18,0x40(%rsp)
   e5a97:	0f 84 cb 01 00 00    	je     e5c68
   e5a9d:	48 8b 4c 24 38       	mov    0x38(%rsp),%rcx
   e5aa2:	48 8d bc 24 d0 02 00 00 	lea    0x2d0(%rsp),%rdi
   e5aaa:	48 8d 94 24 80 00 00 00 	lea    0x80(%rsp),%rdx
   e5ab2:	48 89 de             	mov    %rbx,%rsi
   e5ab5:	e8 f6 d6 fe ff       	call   d31b0
