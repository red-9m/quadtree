# Original unordered runner: build, then call LibraryIndex::pairs
   e565d:	e8 6e d3 ff ff       	call   e29d0 <QuadTree::Status QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::buildPrepared<false, false, std::vector<QuadTree::BuildEntry<unsigned int, int, QuadTree::Box<int> >, std::allocator<QuadTree::BuildEntry<unsigned int, int, QuadTree::Box<int> > > >, QuadTree::detail::AcceptAll>(std::vector<QuadTree::BuildEntry<unsigned int, int, QuadTree::Box<int> >, std::allocator<QuadTree::BuildEntry<unsigned int, int, QuadTree::Box<int> > > >&, QuadTree::detail::AcceptAll&, std::optional<std::span<QuadTree::Handle<QuadTree::detail::EntryStore<int, unsigned int, QuadTree::Unchecked, QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true> > >, 18446744073709551615ul> >, unsigned long, QuadTree::Workspace<int>&) [clone .isra.0]>
   e5662:	48 83 c4 20          	add    $0x20,%rsp
   e5666:	84 c0                	test   %al,%al
   e5668:	0f 85 bc 06 00 00    	jne    e5d2a <(anonymous namespace)::Runner<bench::LibraryIndex<int, false>, int>::step((anonymous namespace)::Fixture<int> const&, bench::Boundary, unsigned long)+0xf2a>
   e566e:	48 83 bc 24 80 00 00 00 18 	cmpq   $0x18,0x80(%rsp)
   e5677:	0f 84 e3 01 00 00    	je     e5860 <(anonymous namespace)::Runner<bench::LibraryIndex<int, false>, int>::step((anonymous namespace)::Fixture<int> const&, bench::Boundary, unsigned long)+0xa60>
   e567d:	48 8b 4c 24 78       	mov    0x78(%rsp),%rcx
   e5682:	48 8d bc 24 10 03 00 00 	lea    0x310(%rsp),%rdi
   e568a:	48 8d 94 24 c0 00 00 00 	lea    0xc0(%rsp),%rdx
   e5692:	48 89 de             	mov    %rbx,%rsi
   e5695:	e8 16 db fe ff       	call   d31b0 <bench::Counters bench::LibraryIndex<int, false>::pairs<(anonymous namespace)::PairSink>((anonymous namespace)::PairSink&, unsigned long) const>

# Wrapper unordered runner: size <= 1 falls through to zero counters
   e7f44:	83 bb d0 00 00 00 01 	cmpl   $0x1,0xd0(%rbx)
   e7f4b:	0f 87 9f 01 00 00    	ja     e80f0
   e7f51:	66 0f ef c0          	pxor   %xmm0,%xmm0
   e7f55:	31 d2                	xor    %edx,%edx
   e7f57:	0f 11 84 24 98 01 00 00 	movups %xmm0,0x198(%rsp)
   e7f5f:	8b 8b d0 00 00 00    	mov    0xd0(%rbx),%ecx
   e7f65:	45 31 ff             	xor    %r15d,%r15d
   e7f68:	f3 0f 6f 84 24 98 01 00 00 	movdqu 0x198(%rsp),%xmm0
   e7f71:	85 c9                	test   %ecx,%ecx
   e7f73:	66 0f c6 c0 01       	shufpd $0x1,%xmm0,%xmm0
   e7f78:	41 0f 95 c7          	setne  %r15b
   e7f7c:	80 fa 0b             	cmp    $0xb,%dl
   e7f7f:	40 0f 94 c5          	sete   %bpl
   e7f83:	48 83 7c 24 08 18    	cmpq   $0x18,0x8(%rsp)
   e7f89:	0f 84 b1 01 00 00    	je     e8140

# Wrapper out-of-line path begins at e80f0 (taken only above one entry)
   e80f0:	48 8d 94 24 40 01 00 00 	lea    0x140(%rsp),%rdx
   e80f8:	48 8d bc 24 90 01 00 00 	lea    0x190(%rsp),%rdi
   e8100:	48 89 ee             	mov    %rbp,%rsi
   e8103:	48 8d 8c 24 10 03 00 00 	lea    0x310(%rsp),%rcx
   e810b:	4c 8d 84 24 f0 00 00 00 	lea    0xf0(%rsp),%r8
   e8113:	e8 28 ce fe ff       	call   d4f40 <QuadTree::QueryResult QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::forEachPairNontrivial<bench::LibraryIndex<int, false>::pairs<(anonymous namespace)::PairSink>((anonymous namespace)::PairSink&, unsigned long) const::{lambda(auto:1 const&, auto:2 const&)#1}, QuadTree::detail::AcceptAll>((anonymous namespace)::PairSink&, QuadTree::QueryOptions<int> const&, QuadTree::detail::AcceptAll&) const>
   e8118:	0f b6 94 24 90 01 00 00 	movzbl 0x190(%rsp),%edx

# Wrapper ordered runner: early rank check; call the helper only above one entry
   e6fc0:	48 83 f8 01          	cmp    $0x1,%rax
   e6fc4:	77 32                	ja     e6ff8
   e6fc6:	48 3b 83 88 00 00 00 	cmp    0x88(%rbx),%rax
   e6fcd:	0f 84 5d 03 00 00    	je     e7330
   e6fd3:	ba 07 00 00 00       	mov    $0x7,%edx
   e6fd8:	48 8b 3d 81 e2 07 00 	mov    0x7e281(%rip),%rdi        # 165260
   e6fdf:	48 8d 35 ba 59 06 00 	lea    0x659ba(%rip),%rsi        # 14c9a0
   e6fe6:	31 c0                	xor    %eax,%eax
   e6fe8:	e8 43 03 f2 ff       	call   7330
   e6fed:	e8 2e 01 f2 ff       	call   7120
   e6ff2:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
   e6ff8:	48 8d 94 24 40 01 00 00 	lea    0x140(%rsp),%rdx
   e7000:	48 8d bc 24 90 01 00 00 	lea    0x190(%rsp),%rdi
   e7008:	48 89 ee             	mov    %rbp,%rsi
   e700b:	48 8d 8c 24 10 03 00 00 	lea    0x310(%rsp),%rcx
   e7013:	4c 8d 84 24 f0 00 00 00 	lea    0xf0(%rsp),%r8
   e701b:	e8 60 0e ff ff       	call   d7e80 <QuadTree::QueryResult QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::forEachPairNontrivial<bench::LibraryIndex<int, true>::pairs<(anonymous namespace)::PairSink>((anonymous namespace)::PairSink&, unsigned long) const::{lambda(auto:1 const&, auto:2 const&)#1}, QuadTree::detail::AcceptAll>((anonymous namespace)::PairSink&, QuadTree::QueryOptions<int> const&, QuadTree::detail::AcceptAll&) const>
   e7020:	0f b6 94 24 90 01 00 00 	movzbl 0x190(%rsp),%edx

# Wrapper ordered 0/1 success path
   e7330:	66 0f ef c0          	pxor   %xmm0,%xmm0
   e7334:	31 d2                	xor    %edx,%edx
   e7336:	0f 11 84 24 98 01 00 00 	movups %xmm0,0x198(%rsp)
   e733e:	e9 ee fc ff ff       	jmp    e7031
   e7031:	8b 8b d0 00 00 00    	mov    0xd0(%rbx),%ecx
   e7037:	45 31 ff             	xor    %r15d,%r15d
   e703a:	f3 0f 6f 84 24 98 01 00 00 	movdqu 0x198(%rsp),%xmm0
