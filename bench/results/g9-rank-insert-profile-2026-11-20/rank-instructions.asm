; Staged GCC x86-64 Release ELF; objdump -d -Mintel --no-show-raw-insn.
; insertRank, with hashInsert and buildRankTree inlined.
  144140: push   rbx                                         ; insert_setup_return
  144141: mov    rcx,rsi                                     ; insert_setup_return
  144144: sub    rsp,0x30                                    ; insert_setup_return
  144148: mov    rbx,QWORD PTR [rdi+0x28]                    ; insert_setup_return
  14414c: cmp    BYTE PTR [rdi+0x44],0x0                     ; insert_setup_return
  144150: je     1441f0                                      ; insert_setup_return
  14419e: call   12d800                                      ; tree_insert
  1441e0: add    rsp,0x30                                    ; insert_setup_return
  1441e4: pop    rbx                                         ; insert_setup_return
  1441e5: ret                                                ; insert_setup_return
  1441e6: cs nop WORD PTR [rax+rax*1+0x0]                    ; insert_setup_return
  1441f0: mov    rax,QWORD PTR [rdi+0x30]                    ; insert_setup_return
  1441f4: mov    r9,QWORD PTR [rdi+0x58]                     ; probe_stats_and_bound / probe_count_load
  1441f8: movabs rdx,0xbf58476d1ce4e5b9                      ; home_mix_mask
  144202: mov    r10d,DWORD PTR [rdi+0x40]                   ; insert_setup_return
  144206: lea    rsi,[rax-0x1]                               ; home_mix_mask
  14420a: mov    rax,rcx                                     ; home_mix_mask
  14420d: lea    r8,[r9+0x1]                                 ; probe_stats_and_bound
  144211: add    r9,0x82                                     ; probe_stats_and_bound
  144218: shr    rax,0x1e                                    ; home_mix_mask
  14421c: xor    rax,rcx                                     ; home_mix_mask
  14421f: imul   rax,rdx                                     ; home_mix_mask
  144223: mov    rdx,rax                                     ; home_mix_mask
  144226: shr    rdx,0x1b                                    ; home_mix_mask
  14422a: xor    rax,rdx                                     ; home_mix_mask
  14422d: movabs rdx,0x94d049bb133111eb                      ; home_mix_mask
  144237: imul   rax,rdx                                     ; home_mix_mask
  14423b: mov    rdx,rax                                     ; home_mix_mask
  14423e: shr    rdx,0x1f                                    ; home_mix_mask
  144242: xor    rax,rdx                                     ; home_mix_mask
  144245: and    rax,rsi                                     ; home_mix_mask
  144248: jmp    144293                                      ; insert_setup_return
  14424a: cs nop WORD PTR [rax+rax*1+0x0]                    ; insert_setup_return
  144254: data16 cs nop WORD PTR [rax+rax*1+0x0]             ; insert_setup_return
  14425f: data16 cs nop WORD PTR [rax+rax*1+0x0]             ; insert_setup_return
  14426a: data16 cs nop WORD PTR [rax+rax*1+0x0]             ; insert_setup_return
  144275: data16 cs nop WORD PTR [rax+rax*1+0x0]             ; insert_setup_return
  144280: add    rax,0x1                                     ; probe_advance
  144284: lea    rdx,[r8+0x1]                                ; probe_stats_and_bound
  144288: and    rax,rsi                                     ; probe_advance
  14428b: cmp    rdx,r9                                      ; probe_stats_and_bound
  14428e: je     144300                                      ; probe_stats_and_bound
  144290: mov    r8,rdx                                      ; probe_stats_and_bound
  144293: mov    rdx,rax                                     ; epoch_address_test
  144296: shl    rdx,0x4                                     ; epoch_address_test
  14429a: add    rdx,rbx                                     ; epoch_address_test
  14429d: cmp    DWORD PTR [rdx+0x8],r10d                    ; epoch_address_test / epoch_load
  1442a1: je     144280                                      ; epoch_address_test
  1442a3: mov    QWORD PTR [rdi+0x58],r8                     ; probe_stats_and_bound / probe_count_store
  1442a7: mov    QWORD PTR [rdx],rcx                         ; bucket_publish / key_store
  1442aa: mov    DWORD PTR [rdx+0x8],r10d                    ; bucket_publish / epoch_store
  1442ae: mov    DWORD PTR [rdx+0xc],0x0                     ; bucket_publish / reserved_word_store
  1442b5: add    rsp,0x30                                    ; insert_setup_return
  1442b9: pop    rbx                                         ; insert_setup_return
  1442ba: ret                                                ; insert_setup_return
  144300: mov    eax,DWORD PTR [rdi+0x80]                    ; tree_conversion
  14430d: add    QWORD PTR [rdi+0x68],0x1                    ; tree_conversion
  144316: mov    BYTE PTR [rdi+0x44],0x1                     ; tree_conversion
  1443a5: call   12d800                                      ; tree_conversion
