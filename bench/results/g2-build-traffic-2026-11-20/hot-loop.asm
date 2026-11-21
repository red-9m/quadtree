
/tmp/quadtree-g9-radix.SDtlif/candidate-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

00000000001458c0 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x400>:
  1458c0:	cmp    %eax,%ecx
  1458c2:	jb     145929 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x469>
  1458c4:	xor    %ebx,%ebx
  1458c6:	cmpb   $0x0,0x57(%rsp)
  1458cb:	je     1458db <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x41b>
  1458cd:	shr    $1,%edi
  1458cf:	xor    %ebx,%ebx
  1458d1:	add    %r10d,%edi
  1458d4:	cmp    0x40(%rsp),%edi
  1458d8:	setge  %bl
  1458db:	shr    $1,%eax
  1458dd:	add    %r9d,%eax
  1458e0:	cmp    %edx,%eax
  1458e2:	setge  %al
  1458e5:	movzbl %al,%eax
  1458e8:	add    %eax,%eax
  1458ea:	or     %ebx,%eax
  1458ec:	add    $0x1,%eax
  1458ef:	mov    %eax,0x10(%r11,%rsi,1)
  1458f4:	add    $0x18,%rsi
  1458f8:	addl   $0x1,0xb0(%rsp,%rax,4)
  145900:	cmp    %rsi,%rbp
  145903:	je     145930 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x470>
  145905:	mov    0x14(%r15,%rsi,1),%eax
  14590a:	imul   $0x38,%rax,%rax
  14590e:	add    %r14,%rax
  145911:	mov    (%rax),%r10d
  145914:	mov    0x8(%rax),%edi
  145917:	mov    0x4(%rax),%r9d
  14591b:	mov    0xc(%rax),%eax
  14591e:	sub    %r10d,%edi
  145921:	sub    %r9d,%eax
  145924:	cmp    %edi,%r12d
  145927:	jae    1458c0 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x400>
  145929:	xor    %eax,%eax
  14592b:	jmp    1458ef <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x42f>

/tmp/quadtree-g9-radix.SDtlif/candidate-build/bench/quadtree-bench:     file format elf64-x86-64


Disassembly of section .text:

0000000000145af8 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x638>:
  145af8:	nopl   0x0(%rax,%rax,1)
  145b00:	mov    0x10(%r10,%rdi,1),%r8d
  145b05:	mov    0x14(%rbx,%rdi,1),%r9d
  145b0a:	add    $0x18,%rdi
  145b0e:	mov    0xf0(%rsp,%r8,4),%esi
  145b16:	lea    0x1(%rsi),%r12d
  145b1a:	lea    (%rsi,%rsi,2),%rsi
  145b1e:	mov    %r12d,0xf0(%rsp,%r8,4)
  145b26:	mov    %r9d,0x14(%rbp,%rsi,8)
  145b2b:	cmp    %r11,%rdi
  145b2e:	jne    145b00 <_ZNK8QuadTree5IndexIjiNS_9UncheckedELb1EE8planBulkERKNS_6detail4CellIiEEjjjPKNS3_10DescriptorIiEEPNS3_10SpatialKeyESD_SD_SD_mRmRb+0x640>
