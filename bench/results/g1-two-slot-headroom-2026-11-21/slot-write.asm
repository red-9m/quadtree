# Control: slot construction loop.
   d2243:	48 8b 4b 68          	mov    0x68(%rbx),%rcx
   d2247:	31 c0                	xor    %eax,%eax
   d2249:	eb 0f                	jmp    d225a <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d224b:	31 ff                	xor    %edi,%edi
   d224d:	89 41 04             	mov    %eax,0x4(%rcx)
   d2250:	83 c0 01             	add    $0x1,%eax
   d2253:	48 83 c1 08          	add    $0x8,%rcx
   d2257:	89 79 f8             	mov    %edi,-0x8(%rcx)
   d225a:	41 39 c3             	cmp    %eax,%r11d
   d225d:	75 ec                	jne    d224b <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d225f:	e9 8a 17 00 00       	jmp    d39ee <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d2264:	0f 1f 40 00          	nopl   0x0(%rax)
   d2268:	bd 04 00 00 00       	mov    $0x4,%ebp
   d226d:	e9 4e f6 ff ff       	jmp    d18c0 <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep

# Diagnostic: count == 2 jumps past the slot construction loop.
   d227b:	41 83 fb 02          	cmp    $0x2,%r11d
   d227f:	0f 84 a9 17 00 00    	je     d3a2e <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d2285:	48 8b 4b 68          	mov    0x68(%rbx),%rcx
   d2289:	31 c0                	xor    %eax,%eax
   d228b:	eb 0f                	jmp    d229c <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d228d:	31 ff                	xor    %edi,%edi
   d228f:	89 41 04             	mov    %eax,0x4(%rcx)
   d2292:	83 c0 01             	add    $0x1,%eax
   d2295:	48 83 c1 08          	add    $0x8,%rcx
   d2299:	89 79 f8             	mov    %edi,-0x8(%rcx)
   d229c:	41 39 c3             	cmp    %eax,%r11d
   d229f:	75 ec                	jne    d228d <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d22a1:	e9 88 17 00 00       	jmp    d3a2e <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d22a6:	66 2e 0f 1f 84 00 00 00 00 00 	cs nopw 0x0(%rax,%rax,1)
   d3a2e:	48 89 93 c0 00 00 00 	mov    %rdx,0xc0(%rbx)
   d3a35:	44 89 9b c8 00 00 00 	mov    %r11d,0xc8(%rbx)
   d3a3c:	44 89 9b d4 00 00 00 	mov    %r11d,0xd4(%rbx)
   d3a43:	80 7c 24 08 00       	cmpb   $0x0,0x8(%rsp)
   d3a48:	74 21                	je     d3a6b <_ZN8QuadTree5IndexIjiNS_9UncheckedELb1EE13buildPreparedILb0ELb0ESt6vectorINS_10BuildEntryIjiNS_3BoxIiEEEESaIS8_EENS_6detail9Accep
   d3a4a:	48 8b 44 24 10       	mov    0x10(%rsp),%rax
