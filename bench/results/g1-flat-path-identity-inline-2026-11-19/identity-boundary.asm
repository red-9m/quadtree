Control:

control-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000e3cfb <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_RT2_St
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
   e3d29:	je     e2b94 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_
   e3d2f:	mov    0x60(%rsp),%r11
   e3d34:	mov    0x8(%rsp),%rsi
   e3d39:	movdqa 0x10(%rsp),%xmm1
   e3d3f:	mov    0x20(%rsp),%r10
   e3d44:	mov    0x78(%r11),%rdx
   e3d48:	test   %rdx,%rdx

Retained:

build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000e3c4e <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_RT2_St
   e3c4e:	mov    0x7f62b(%rip),%rdx        # 163280 <_ZN8QuadTree6detail21moduleIdentityCounterE>
   e3c55:	mov    %rax,%rsi
   e3c58:	cmp    $0xffffffffffffffff,%rdx
   e3c5c:	je     e3e5b <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_
   e3c62:	lea    0x1(%rdx),%rcx
   e3c66:	mov    %rdx,%rax
   e3c69:	lock cmpxchg %rcx,0x7f60e(%rip)        # 163280 <_ZN8QuadTree6detail21moduleIdentityCounterE>
   e3c72:	mov    %rax,%rdx
   e3c75:	jne    e3c58 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_
   e3c77:	mov    %rsi,%rax
   e3c7a:	add    $0x1,%rdx
   e3c7e:	je     e3e5b <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6StatusERT1_
   e3c84:	mov    0x78(%r11),%rsi
   e3c88:	test   %rsi,%rsi
