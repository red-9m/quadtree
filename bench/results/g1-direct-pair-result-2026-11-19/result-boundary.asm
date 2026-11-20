control
   d31b0:	sub    $0x168,%rsp
   d31b7:	mov    0xd0(%rsi),%r9d
   d31be:	pxor   %xmm0,%xmm0
   d31c2:	mov    %rbx,0x138(%rsp)
   d31ca:	mov    %rdi,%rbx
   d31cd:	mov    %rbp,0x140(%rsp)
   d31d5:	mov    %r9,%rbp
   d31d8:	mov    %rsi,0x10(%rsp)
   d31dd:	mov    %fs:0x28,%rax
   d31e6:	mov    %rax,0x128(%rsp)
   d31ee:	xor    %eax,%eax
   d31f0:	mov    %rdx,0x58(%rsp)
   d31f5:	cmp    $0x1,%r9d
   d31f9:	jbe    d3508
   d31ff:	rex.WR

control
   d3442:	movdqa 0x70(%rsp),%xmm0
   d3448:	movdqa 0x60(%rsp),%xmm2
   d344e:	movzbl 0x60(%rsp),%eax
   d3453:	movaps %xmm0,0xf0(%rsp)
   d345b:	movdqa 0x80(%rsp),%xmm0
   d3464:	movaps %xmm2,0xe0(%rsp)
   d346c:	movaps %xmm0,0x100(%rsp)
   d3474:	cmp    $0xb,%al
   d3476:	je     d3480
   d3478:	test   %al,%al
   d347a:	jne    d37ce
   d3480:	mov    0x158(%rsp),%r14
   d3488:	mov    0xf0(%rsp),%rdx
   d3490:	movq   $0x0,0x18(%rbx)
   d3498:	movq   $0x0,0x28(%rbx)
   d34a0:	mov    %rdx,(%rbx)
   d34a3:	mov    0xe8(%rsp),%rdx
   d34ab:	mov    %r9,0x8(%rbx)
   d34af:	mov    %rdx,0x10(%rbx)
   d34b3:	xor    %edx,%edx
   d34b5:	test   %ebp,%ebp
   d34b7:	setne  %dl
   d34ba:	cmp    $0xb,%al
   d34bc:	mov    %edx,0x20(%rbx)
   d34bf:	sete   0x30(%rbx)

direct
   d34f0:	sub    $0x178,%rsp
   d34f7:	mov    0xd0(%rsi),%r9d
   d34fe:	pxor   %xmm0,%xmm0
   d3502:	mov    %rbp,0x150(%rsp)
   d350a:	mov    %rdi,%rbp
   d350d:	mov    %r12,0x158(%rsp)
   d3515:	mov    %r9,%r12
   d3518:	mov    %rsi,0x10(%rsp)
   d351d:	mov    %fs:0x28,%rax
   d3526:	mov    %rax,0x138(%rsp)
   d352e:	xor    %eax,%eax
   d3530:	mov    %rdx,0x68(%rsp)
   d3535:	cmp    $0x1,%r9d
   d3539:	jbe    d3858
   d353f:	rex.W
   d3540:	.byte 0x89
   d3541:	pushf
   d3542:	and    $0x48,%al
   d3544:	.byte 0x1

direct
   d3782:	movdqa 0x80(%rsp),%xmm0
   d378b:	movdqa 0x70(%rsp),%xmm2
   d3791:	movzbl 0x70(%rsp),%eax
   d3796:	movaps %xmm0,0x100(%rsp)
   d379e:	movdqa 0x90(%rsp),%xmm0
   d37a7:	movaps %xmm2,0xf0(%rsp)
   d37af:	movaps %xmm0,0x110(%rsp)
   d37b7:	test   %al,%al
   d37b9:	je     d37c3
   d37bb:	cmp    $0xb,%al
   d37bd:	jne    d3b07
   d37c3:	mov    0x148(%rsp),%rbx
   d37cb:	mov    0x168(%rsp),%r14
   d37d3:	mov    0x100(%rsp),%rdx
   d37db:	movq   $0x0,0x18(%rbp)
   d37e3:	movq   $0x0,0x28(%rbp)
   d37eb:	mov    %rdx,0x0(%rbp)
   d37ef:	mov    0xf8(%rsp),%rdx
   d37f7:	mov    %r9,0x8(%rbp)
   d37fb:	mov    %rdx,0x10(%rbp)
   d37ff:	xor    %edx,%edx
   d3801:	test   %r12d,%r12d
   d3804:	setne  %dl
   d3807:	cmp    $0xb,%al
   d3809:	mov    %edx,0x20(%rbp)
   d380c:	sete   0x30(%rbp)
   d3810:	mov    0x138(%rsp),%rax
   d3818:	sub    %fs:0x28,%rax
   d3821:	jne    d3b34
   d3827:	mov    %rbp,%rax
   d382a:	mov    0x158(%rsp),%r12
   d3832:	mov    0x150(%rsp),%rbp
   d383a:	add    $0x178,%rsp
   d3841:	ret
   d3842:	nopw   0x0(%rax,%rax,1)
   d3848:	mov    %ebp,(%rsp)
   d384b:	mov    %r13d,%edx
   d384e:	jmp    d3664
   d3853:	nopl   0x0(%rax,%rax,1)

direct
   d3ab3:	mov    %r11,0x100(%rsp)
   d3abb:	mov    0x50(%rsp),%r9
   d3ac0:	xor    %eax,%eax
   d3ac2:	mov    0x58(%rsp),%rbp
   d3ac7:	mov    0x148(%rsp),%rbx
   d3acf:	movq   $0x0,0xf8(%rsp)
   d3adb:	mov    0x160(%rsp),%r13
   d3ae3:	mov    0x168(%rsp),%r14
   d3aeb:	mov    0x170(%rsp),%r15
   d3af3:	jmp    d37d3

