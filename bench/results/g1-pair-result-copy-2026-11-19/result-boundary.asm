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

member
   d3516:	movzbl 0x60(%rsp),%eax
   d351b:	mov    (%rsp),%r9
   d351f:	test   %al,%al
   d3521:	setne  %dl
   d3524:	cmp    $0xb,%al
   d3526:	setne  %cl
   d3529:	and    %ecx,%edx
   d352b:	movdqu 0x68(%rsp),%xmm0
   d3531:	mov    0x88(%rsp),%rcx
   d3539:	movups %xmm0,0xe8(%rsp)
   d3541:	movdqu 0x78(%rsp),%xmm0
   d3547:	mov    %rcx,0x108(%rsp)
   d354f:	movups %xmm0,0xf8(%rsp)
   d3557:	test   %dl,%dl
   d3559:	jne    d38e4
   d355f:	mov    0x158(%rsp),%r14
   d3567:	mov    0xf0(%rsp),%rdx
   d356f:	movq   $0x0,0x18(%rbx)
   d3577:	movq   $0x0,0x28(%rbx)
   d357f:	mov    %rdx,(%rbx)
   d3582:	mov    0xe8(%rsp),%rdx
   d358a:	mov    %r9,0x8(%rbx)
   d358e:	mov    %rdx,0x10(%rbx)
   d3592:	xor    %edx,%edx
   d3594:	test   %ebp,%ebp
   d3596:	setne  %dl
   d3599:	cmp    $0xb,%al
   d359b:	mov    %edx,0x20(%rbx)
   d359e:	sete   0x30(%rbx)

scalar
   d36d6:	movzbl 0x60(%rsp),%eax
   d36db:	mov    0x68(%rsp),%rdx
   d36e0:	mov    0x70(%rsp),%rdi
   d36e5:	mov    (%rsp),%r9
   d36e9:	test   %al,%al
   d36eb:	setne  %cl
   d36ee:	cmp    $0xb,%al
   d36f0:	setne  %sil
   d36f4:	and    %esi,%ecx
   d36f6:	test   %cl,%cl
   d36f8:	jne    d3a63
   d36fe:	mov    0x160(%rsp),%r15
   d3706:	mov    %rdx,0x10(%rbx)
   d370a:	xor    %edx,%edx
   d370c:	test   %ebp,%ebp
   d370e:	setne  %dl
   d3711:	movq   $0x0,0x18(%rbx)
   d3719:	cmp    $0xb,%al
   d371b:	movq   $0x0,0x28(%rbx)
   d3723:	mov    %rdi,(%rbx)
   d3726:	mov    %r9,0x8(%rbx)
   d372a:	mov    %edx,0x20(%rbx)
   d372d:	sete   0x30(%rbx)

tiny
   d3782:	movdqa 0x70(%rsp),%xmm0
   d3788:	movdqa 0x60(%rsp),%xmm2
   d378e:	movzbl 0x60(%rsp),%eax
   d3793:	movaps %xmm0,0xf0(%rsp)
   d379b:	movdqa 0x80(%rsp),%xmm0
   d37a4:	movaps %xmm2,0xe0(%rsp)
   d37ac:	movaps %xmm0,0x100(%rsp)
   d37b4:	cmp    $0xb,%al
   d37b6:	je     d37c0
   d37b8:	test   %al,%al
   d37ba:	jne    d3b34

tiny
   d3adf:	mov    %r15,0xf0(%rsp)
   d3ae7:	mov    0x40(%rsp),%r9
   d3aec:	xor    %eax,%eax
   d3aee:	mov    0x48(%rsp),%rbx
   d3af3:	mov    0x148(%rsp),%r12
   d3afb:	movq   $0x0,0xe8(%rsp)
   d3b07:	mov    0x150(%rsp),%r13
   d3b0f:	mov    0x158(%rsp),%r14
   d3b17:	mov    0x160(%rsp),%r15
   d3b1f:	jmp    d37c8

