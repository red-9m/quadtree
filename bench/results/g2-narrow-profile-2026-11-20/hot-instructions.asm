
/tmp/quadtree-g2-bulk-streams.TmvHns/narrow-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

0000000000145e73 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS_3BoxIiEEPjSC_SC_PNS3_10SpatialKeyEPhmRmRb+0x2b3>:
  145e73:	add    $0x1,%rsi
  145e77:	mov    (%r10,%rsi,4),%eax
  145e7b:	shl    $0x4,%rax
  145e7f:	add    %r14,%rax
  145e82:	mov    (%rax),%r9d
  145e85:	mov    0x8(%rax),%ecx
  145e88:	mov    0x4(%rax),%edi
  145e8b:	mov    0xc(%rax),%eax
  145e8e:	sub    %r9d,%ecx
  145e91:	sub    %edi,%eax
  145e93:	cmp    %ecx,%r12d

/tmp/quadtree-g2-bulk-streams.TmvHns/narrow-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

000000000001e7bf <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE11walkOrderedIZNKS2_11orderedSelfIZNK5bench12LibraryIndexIiLb1EE5pairsIN12_GLOBAL__N_18PairSinkEEENS5_8CountersERT_mEUlRKSC_RKT0_E_NS_6detail9AcceptAllEEEvRNS_9WorkspaceIiEEbRNS2_7ContextESD_RSG_EUljE_EEvRKNS2_4NodeERKNS_3BoxIiEENS_6FilterESQ_SD_+0x2ef>:
   1e7bf:	mov    %ebx,%ecx
   1e7c1:	shl    %cl,%r13d
   1e7c4:	lea    -0x1(%r13),%edi
   1e7c8:	and    %r15d,%edi
   1e7cb:	call   72c0 <__popcountdi2@plt>
   1e7d0:	mov    0x8(%rbp),%rdx

/tmp/quadtree-g2-bulk-streams.TmvHns/narrow-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

000000000001e92a <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE11walkOrderedIZNKS2_11orderedSelfIZNK5bench12LibraryIndexIiLb1EE5pairsIN12_GLOBAL__N_18PairSinkEEENS5_8CountersERT_mEUlRKSC_RKT0_E_NS_6detail9AcceptAllEEEvRNS_9WorkspaceIiEEbRNS2_7ContextESD_RSG_EUljE_EEvRKNS2_4NodeERKNS_3BoxIiEENS_6FilterESQ_SD_+0x45a>:
   1e92a:	mov    %r15d,%edi
   1e92d:	call   72c0 <__popcountdi2@plt>
   1e932:	mov    %eax,%edx
