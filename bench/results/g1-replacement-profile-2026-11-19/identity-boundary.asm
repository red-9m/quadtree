
quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000e3cfb <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_RT2_St8optionalISt4spanINS_6HandleINSB_10EntryStoreIijS1_S2_EEEELm18446744073709551615EEEmRNS_9WorkspaceIiEE.isra.0+0x12ab>:
   e3cfb:	mov    %r9,0x30(%rsp)
   e3d00:	mov    $0xf,%ebx
   e3d05:	mov    %r10,0x20(%rsp)
   e3d0a:	mov    %r11,0x60(%rsp)
   e3d0f:	mov    %rsi,0x8(%rsp)
   e3d14:	movaps %xmm1,0x10(%rsp)
   e3d19:	call   f3e50 <_ZN8QuadTree6detail21acquireModuleIdentityEv>
   e3d1e:	mov    0x30(%rsp),%r9
   e3d23:	test   %rax,%rax
   e3d26:	mov    %rax,%rcx
   e3d29:	je     e2b94 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_RT2_St8optionalISt4spanINS_6HandleINSB_10EntryStoreIijS1_S2_EEEELm18446744073709551615EEEmRNS_9WorkspaceIiEE.isra.0+0x144>
   e3d2f:	mov    0x60(%rsp),%r11
   e3d34:	mov    0x8(%rsp),%rsi
   e3d39:	movdqa 0x10(%rsp),%xmm1
   e3d3f:	mov    0x20(%rsp),%r10
   e3d44:	mov    0x78(%r11),%rdx
   e3d48:	test   %rdx,%rdx
   e3d4b:	jne    e3e74 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_RT2_St8optionalISt4spanINS_6HandleINSB_10EntryStoreIijS1_S2_EEEELm18446744073709551615EEEmRNS_9WorkspaceIiEE.isra.0+0x1424>
   e3d51:	mov    $0xffffffff,%eax
   e3d56:	xor    %edx,%edx
   e3d58:	mov    %rax,0xd8(%r11)
   e3d5f:	mov    %edx,0xe0(%r11)
   e3d66:	cmpl   $0xffffffff,0x15c(%r11)
   e3d6e:	je     e3d8b <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_RT2_St8optionalISt4spanINS_6HandleINSB_10EntryStoreIijS1_S2_EEEELm18446744073709551615EEEmRNS_9WorkspaceIiEE.isra.0+0x133b>
   e3d70:	mov    %rax,0x15c(%r11)
   e3d77:	xor    %eax,%eax
   e3d79:	mov    %rax,0x168(%r11)
   e3d80:	movl   $0xffffffff,0x170(%r11)
   e3d8b:	cmpb   $0x0,0x158(%r11)
   e3d93:	movups %xmm1,0x148(%r11)
   e3d9b:	jne    e3da5 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_RT2_St8optionalISt4spanINS_6HandleINSB_10EntryStoreIijS1_S2_EEEELm18446744073709551615EEEmRNS_9WorkspaceIiEE.isra.0+0x1355>
   e3d9d:	movb   $0x1,0x158(%r11)

quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000f3e50 <_ZN8QuadTree6detail21acquireModuleIdentityEv>:
   f3e50:	mov    0x6f429(%rip),%rax        # 163280 <_ZN8QuadTree6detail21moduleIdentityCounterE>
   f3e57:	cmp    $0xffffffffffffffff,%rax
   f3e5b:	je     f3e78 <_ZN8QuadTree6detail21acquireModuleIdentityEv+0x28>
   f3e5d:	lea    0x1(%rax),%rdx
   f3e61:	lock cmpxchg %rdx,0x6f416(%rip)        # 163280 <_ZN8QuadTree6detail21moduleIdentityCounterE>
   f3e6a:	jne    f3e57 <_ZN8QuadTree6detail21acquireModuleIdentityEv+0x7>
   f3e6c:	add    $0x1,%rax
   f3e70:	ret
   f3e71:	nopl   0x0(%rax)
   f3e78:	xor    %eax,%eax
   f3e7a:	ret
   f3e7b:	nopl   0x0(%rax,%rax,1)
