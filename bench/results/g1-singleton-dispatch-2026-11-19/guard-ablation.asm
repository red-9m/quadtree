
# workspace minus control
--- control
+++ workspace
@@ -3,23 +3,23 @@
    e29df:	64 48 8b 04 25 28 00 00 00 	mov    %fs:0x28,%rax
    e29e8:	48 89 84 24 18 01 00 00 	mov    %rax,0x118(%rsp)
    e29f0:	31 c0                	xor    %eax,%eax
-   e29f2:	48 39 91 90 00 00 00 	cmp    %rdx,0x90(%rcx)
-   e29f9:	0f 82 31 01 00 00    	jb     e2b30
+   e29f2:	66 0f 1f 84 00 00 00 00 00 	nopw   0x0(%rax,%rax,1)
+   e29fb:	0f 1f 40 00          	nopl   0x0(%rax)
    e29ff:	49 89 d2             	mov    %rdx,%r10
    e2a02:	49 89 c9             	mov    %rcx,%r9
-   e2a05:	48 39 91 a8 00 00 00 	cmp    %rdx,0xa8(%rcx)
-   e2a0c:	0f 82 1e 01 00 00    	jb     e2b30
-   e2a12:	48 39 91 c0 00 00 00 	cmp    %rdx,0xc0(%rcx)
-   e2a19:	0f 82 11 01 00 00    	jb     e2b30
-   e2a1f:	80 b9 cc 00 00 00 00 	cmpb   $0x0,0xcc(%rcx)
-   e2a26:	0f 85 62 11 00 00    	jne    e3b8e
+   e2a05:	66 0f 1f 84 00 00 00 00 00 	nopw   0x0(%rax,%rax,1)
+   e2a0e:	0f 1f 40 00          	nopl   0x0(%rax)
+   e2a12:	66 0f 1f 84 00 00 00 00 00 	nopw   0x0(%rax,%rax,1)
+   e2a1b:	0f 1f 40 00          	nopl   0x0(%rax)
+   e2a1f:	66 0f 1f 84 00 00 00 00 00 	nopw   0x0(%rax,%rax,1)
+   e2a28:	0f 1f 40 00          	nopl   0x0(%rax)
    e2a2c:	8b 57 24             	mov    0x24(%rdi),%edx
    e2a2f:	4c 89 a4 24 38 01 00 00 	mov    %r12,0x138(%rsp)
    e2a37:	49 89 fb             	mov    %rdi,%r11
    e2a3a:	66 0f ef c0          	pxor   %xmm0,%xmm0
    e2a3e:	4c 89 ac 24 40 01 00 00 	mov    %r13,0x140(%rsp)
    e2a46:	44 0f b6 a4 24 70 01 00 00 	movzbl 0x170(%rsp),%r12d
-   e2a4f:	c6 81 cc 00 00 00 01 	movb   $0x1,0xcc(%rcx)
+   e2a4f:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
    e2a56:	4c 8b ac 24 60 01 00 00 	mov    0x160(%rsp),%r13
    e2a5e:	48 89 d1             	mov    %rdx,%rcx
    e2a61:	4c 39 d2             	cmp    %r10,%rdx
@@ -58,7 +58,7 @@
    e2b01:	48 8b ac 24 30 01 00 00 	mov    0x130(%rsp),%rbp
    e2b09:	4c 8b bc 24 50 01 00 00 	mov    0x150(%rsp),%r15
    e2b11:	bb 02 00 00 00       	mov    $0x2,%ebx
-   e2b16:	41 c6 81 cc 00 00 00 00 	movb   $0x0,0xcc(%r9)
+   e2b16:	0f 1f 84 00 00 00 00 00 	nopl   0x0(%rax,%rax,1)
    e2b1e:	4c 8b a4 24 38 01 00 00 	mov    0x138(%rsp),%r12
    e2b26:	4c 8b ac 24 40 01 00 00 	mov    0x140(%rsp),%r13
    e2b2e:	eb 05                	jmp    e2b35

# dispatch minus control
--- control
+++ dispatch
@@ -22,8 +22,7 @@
    e2a4f:	c6 81 cc 00 00 00 01 	movb   $0x1,0xcc(%rcx)
    e2a56:	4c 8b ac 24 60 01 00 00 	mov    0x160(%rsp),%r13
    e2a5e:	48 89 d1             	mov    %rdx,%rcx
-   e2a61:	4c 39 d2             	cmp    %r10,%rdx
-   e2a64:	72 0f                	jb     e2a75
+   e2a61:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
    e2a66:	8b 87 cc 00 00 00    	mov    0xcc(%rdi),%eax
    e2a6c:	4c 39 d0             	cmp    %r10,%rax
    e2a6f:	0f 83 eb 00 00 00    	jae    e2b60
@@ -71,11 +70,10 @@
    e2b56:	48 81 c4 58 01 00 00 	add    $0x158,%rsp
    e2b5d:	c3                   	ret
    e2b5e:	66 90                	xchg   %ax,%ax
-   e2b60:	8b 87 d0 00 00 00    	mov    0xd0(%rdi),%eax
-   e2b66:	4c 39 d0             	cmp    %r10,%rax
-   e2b69:	0f 82 06 ff ff ff    	jb     e2a75
+   e2b60:	66 0f 1f 84 00 00 00 00 00 	nopw   0x0(%rax,%rax,1)
+   e2b69:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
    e2b6f:	48 8b 06             	mov    (%rsi),%rax
-   e2b72:	80 3f 00             	cmpb   $0x0,(%rdi)
-   e2b75:	0f 85 ad 04 00 00    	jne    e3028
+   e2b72:	e9 b1 04 00 00       	jmp    e3028
+   e2b77:	0f 1f 40 00          	nopl   0x0(%rax)
    e2b7b:	49 83 fa 01          	cmp    $0x1,%r10
    e2b7f:	0f 84 33 02 00 00    	je     e2db8
