# Control: adapter stores candidate and reference in separate qwords

/home/dima/.cache/quadtree-g1-small-headroom.OBTcv2/empty/control-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000ece20 <_ZNK5bench12LibraryIndexIiLb0EE5pairsIN12_GLOBAL__N_18PairSinkEEENS_8CountersERT_m+0x2d0>:
   ece20:	4c 8b b4 24 58 01 00 	mov    0x158(%rsp),%r14
   ece27:	00 
   ece28:	48 8b 94 24 f0 00 00 	mov    0xf0(%rsp),%rdx
   ece2f:	00 
   ece30:	48 c7 43 18 00 00 00 	movq   $0x0,0x18(%rbx)
   ece37:	00 
   ece38:	48 c7 43 28 00 00 00 	movq   $0x0,0x28(%rbx)
   ece3f:	00 
   ece40:	48 89 13             	mov    %rdx,(%rbx)
   ece43:	48 8b 94 24 e8 00 00 	mov    0xe8(%rsp),%rdx
   ece4a:	00 
   ece4b:	4c 89 4b 08          	mov    %r9,0x8(%rbx)
   ece4f:	48 89 53 10          	mov    %rdx,0x10(%rbx)
   ece53:	31 d2                	xor    %edx,%edx
   ece55:	85 ed                	test   %ebp,%ebp
   ece57:	0f 95 c2             	setne  %dl
   ece5a:	3c 0b                	cmp    $0xb,%al
   ece5c:	89 53 20             	mov    %edx,0x20(%rbx)
   ece5f:	0f 94 43 30          	sete   0x30(%rbx)

# Control: runner calls adapter then reloads both qwords with movdqa

/home/dima/.cache/quadtree-g1-small-headroom.OBTcv2/empty/control-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000eda3d <_ZN12_GLOBAL__N_16RunnerIN5bench12LibraryIndexIiLb0EEEiE4stepERKNS_7FixtureIiEENS1_8BoundaryEm+0x86d>:
   eda3d:	48 8b 4c 24 78       	mov    0x78(%rsp),%rcx
   eda42:	48 8d bc 24 10 03 00 	lea    0x310(%rsp),%rdi
   eda49:	00 
   eda4a:	48 8d 94 24 c0 00 00 	lea    0xc0(%rsp),%rdx
   eda51:	00 
   eda52:	48 89 de             	mov    %rbx,%rsi
   eda55:	e8 f6 f0 ff ff       	call   ecb50 <_ZNK5bench12LibraryIndexIiLb0EE5pairsIN12_GLOBAL__N_18PairSinkEEENS_8CountersERT_m>
   eda5a:	8b 74 24 1c          	mov    0x1c(%rsp),%esi
   eda5e:	8b 84 24 30 03 00 00 	mov    0x330(%rsp),%eax
   eda65:	48 b9 ab aa aa aa aa 	movabs $0xaaaaaaaaaaaaaaab,%rcx
   eda6c:	aa aa aa 
   eda6f:	48 8b bc 24 38 03 00 	mov    0x338(%rsp),%rdi
   eda76:	00 
   eda77:	66 0f 6f 74 24 30    	movdqa 0x30(%rsp),%xmm6
   eda7d:	39 c6                	cmp    %eax,%esi
   eda7f:	48 8b 94 24 c8 00 00 	mov    0xc8(%rsp),%rdx
   eda86:	00 
   eda87:	66 0f 6f 84 24 20 03 	movdqa 0x320(%rsp),%xmm0
   eda8e:	00 00 
   eda90:	66 0f 6f 94 24 10 03 	movdqa 0x310(%rsp),%xmm2
   eda97:	00 00 

# Empty probe: adapter is inlined; zero query counters go through one SIMD store/load

/home/dima/.cache/quadtree-g1-small-headroom.OBTcv2/empty/candidate-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000effbf <_ZN12_GLOBAL__N_16RunnerIN5bench12LibraryIndexIiLb0EEEiE4stepERKNS_7FixtureIiEENS1_8BoundaryEm+0x81f>:
   effbf:	44 8b 9b d0 00 00 00 	mov    0xd0(%rbx),%r11d
   effc6:	48 8d 84 24 80 00 00 	lea    0x80(%rsp),%rax
   effcd:	00 
   effce:	48 89 84 24 50 01 00 	mov    %rax,0x150(%rsp)
   effd5:	00 
   effd6:	45 85 db             	test   %r11d,%r11d
   effd9:	0f 85 f1 02 00 00    	jne    f02d0 <_ZN12_GLOBAL__N_16RunnerIN5bench12LibraryIndexIiLb0EEEiE4stepERKNS_7FixtureIiEENS1_8BoundaryEm+0xb30>
   effdf:	66 0f ef c0          	pxor   %xmm0,%xmm0
   effe3:	31 c0                	xor    %eax,%eax
   effe5:	0f 11 84 24 d8 02 00 	movups %xmm0,0x2d8(%rsp)
   effec:	00 
   effed:	f3 0f 6f 84 24 d8 02 	movdqu 0x2d8(%rsp),%xmm0
   efff4:	00 00 
   efff6:	45 31 ff             	xor    %r15d,%r15d
   efff9:	45 85 db             	test   %r11d,%r11d
   efffc:	44 89 d9             	mov    %r11d,%ecx
   effff:	41 0f 95 c7          	setne  %r15b
   f0003:	3c 0b                	cmp    $0xb,%al
   f0005:	41 0f 94 c4          	sete   %r12b
   f0009:	66 0f c6 c0 01       	shufpd $0x1,%xmm0,%xmm0
   f000e:	48 83 7c 24 08 18    	cmpq   $0x18,0x8(%rsp)
   f0014:	0f 84 66 01 00 00    	je     f0180 <_ZN12_GLOBAL__N_16RunnerIN5bench12LibraryIndexIiLb0EEEiE4stepERKNS_7FixtureIiEENS1_8BoundaryEm+0x9e0>

# Empty probe: call to nonempty path

/home/dima/.cache/quadtree-g1-small-headroom.OBTcv2/empty/candidate-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000f02d0 <_ZN12_GLOBAL__N_16RunnerIN5bench12LibraryIndexIiLb0EEEiE4stepERKNS_7FixtureIiEENS1_8BoundaryEm+0xb30>:
   f02d0:	4c 8b 44 24 58       	mov    0x58(%rsp),%r8
   f02d5:	48 8d bc 24 d0 02 00 	lea    0x2d0(%rsp),%rdi
   f02dc:	00 
   f02dd:	48 8d 94 24 50 01 00 	lea    0x150(%rsp),%rdx
   f02e4:	00 
   f02e5:	44 89 5c 24 10       	mov    %r11d,0x10(%rsp)
   f02ea:	4c 8d 8c 24 00 01 00 	lea    0x100(%rsp),%r9
   f02f1:	00 
   f02f2:	48 8d 8b 88 01 00 00 	lea    0x188(%rbx),%rcx
   f02f9:	48 8d 73 08          	lea    0x8(%rbx),%rsi
   f02fd:	e8 1e c9 ff ff       	call   ecc20 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE19forEachNonemptyPairIZNK5bench12LibraryIndexIiLb0EE5pairsIN12_GLOBAL__N_
   f0302:	0f b6 84 24 d0 02 00 	movzbl 0x2d0(%rsp),%eax
   f0309:	00 
   f030a:	44 8b 5c 24 10       	mov    0x10(%rsp),%r11d
   f030f:	3c 0b                	cmp    $0xb,%al
   f0311:	0f 84 d6 fc ff ff    	je     effed <_ZN12_GLOBAL__N_16RunnerIN5bench12LibraryIndexIiLb0EEEiE4stepERKNS_7FixtureIiEENS1_8BoundaryEm+0x84d>
   f0317:	84 c0                	test   %al,%al
   f0319:	0f 84 ce fc ff ff    	je     effed <_ZN12_GLOBAL__N_16RunnerIN5bench12LibraryIndexIiLb0EEEiE4stepERKNS_7FixtureIiEENS1_8BoundaryEm+0x84d>

# Singleton probe: the retained capacity/size predicates and unranked flat path

/home/dima/.cache/quadtree-g1-small-headroom.OBTcv2/singleton/candidate-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000d16f0 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6St
   d16f0:	8b 87 cc 00 00 00    	mov    0xcc(%rdi),%eax
   d16f6:	4c 39 d8             	cmp    %r11,%rax
   d16f9:	0f 82 91 01 00 00    	jb     d1890 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d16ff:	8b 87 d0 00 00 00    	mov    0xd0(%rdi),%eax
   d1705:	4c 39 d8             	cmp    %r11,%rax
   d1708:	0f 82 82 01 00 00    	jb     d1890 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d170e:	0f b6 07             	movzbl (%rdi),%eax
   d1711:	84 c0                	test   %al,%al
   d1713:	0f 84 37 05 00 00    	je     d1c50 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d1719:	49 83 fb 01          	cmp    $0x1,%r11
   d171d:	0f 85 7d 05 00 00    	jne    d1ca0 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d1723:	83 bf c8 00 00 00 01 	cmpl   $0x1,0xc8(%rdi)
   d172a:	0f 84 c2 0a 00 00    	je     d21f2 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d1730:	48 8b 06             	mov    (%rsi),%rax
   d1733:	80 78 30 00          	cmpb   $0x0,0x30(%rax)
   d1737:	0f 85 06 03 00 00    	jne    d1a43 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d173d:	8b 70 08             	mov    0x8(%rax),%esi
   d1740:	39 30                	cmp    %esi,(%rax)
   d1742:	0f 8f cb 02 00 00    	jg     d1a13 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d1748:	8b 70 0c             	mov    0xc(%rax),%esi
   d174b:	39 70 04             	cmp    %esi,0x4(%rax)
   d174e:	0f 8f bf 02 00 00    	jg     d1a13 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d1754:	f3 0f 6f 00          	movdqu (%rax),%xmm0
   d1758:	b9 01 00 00 00       	mov    $0x1,%ecx
   d175d:	48 8b 15 1c db 09 00 	mov    0x9db1c(%rip),%rdx        # 16f280 <_ZN8QuadTree6detail21moduleIdentityCounterE>
   d1764:	48 89 c7             	mov    %rax,%rdi
   d1767:	48 83 fa ff          	cmp    $0xffffffffffffffff,%rdx
   d176b:	0f 84 2e 22 00 00    	je     d399f <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d1771:	48 8d 72 01          	lea    0x1(%rdx),%rsi
   d1775:	48 89 d0             	mov    %rdx,%rax
   d1778:	f0 48 0f b1 35 ff da 	lock cmpxchg %rsi,0x9daff(%rip)        # 16f280 <_ZN8QuadTree6detail21moduleIdentityCounterE>
   d177f:	09 00 
   d1781:	48                   	rex.W
   d1782:	89                   	.byte 0x89

# Singleton probe: skip work and return Status::Ok if no output and Snapshot

/home/dima/.cache/quadtree-g1-small-headroom.OBTcv2/singleton/candidate-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000000d21f2 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9AcceptAllEEENS_6St
   d21f2:	80 3c 24 01          	cmpb   $0x1,(%rsp)
   d21f6:	0f 84 34 f5 ff ff    	je     d1730 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d21fc:	3c 02                	cmp    $0x2,%al
   d21fe:	0f 85 2c f5 ff ff    	jne    d1730 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
   d2204:	31 ed                	xor    %ebp,%ebp
   d2206:	e9 0d f8 ff ff       	jmp    d1a18 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EEN
