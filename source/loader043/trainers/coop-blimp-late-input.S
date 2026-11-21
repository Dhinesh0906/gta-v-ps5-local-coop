/* Exact stock four-float input load follows an integer-only guarded update.
 * All GP registers and flags are preserved. (AI-assisted) */
.section .entry,"ax",@progbits
.global _start
.hidden _start
.type _start,@function
_start:
 pushfq
 pushq %rax
 pushq %rbx
 pushq %rcx
 pushq %rdx
 pushq %rsi
 pushq %rdi
 pushq %r8
 pushq %r9
 pushq %r10
 pushq %r11
 pushq %r12
 pushq %r13
 pushq %r14
 pushq %r15
 pushq %rbp
 subq $8,%rsp
 movq 112(%rsp),%rdi
 call blimp_before_input
 leaq 8(%rsp),%rsp
 popq %rbp
 popq %r15
 popq %r14
 popq %r13
 popq %r12
 popq %r11
 popq %r10
 popq %r9
 popq %r8
 popq %rdi
 popq %rsi
 popq %rdx
 popq %rcx
 popq %rbx
 popq %rax
 popfq
 vmovups 0x1a80(%rbx),%xmm0
 ret
.size _start,.-_start
.section .note.GNU-stack,"",@progbits
