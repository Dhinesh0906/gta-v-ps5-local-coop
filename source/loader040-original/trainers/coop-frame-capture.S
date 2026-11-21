.section .entry,"ax",@progbits
.global _start
.hidden _start
.type _start,@function
_start:
    /* Preserve the sole incoming self argument and every input register.
     * Original Present has no stack arguments. Push flags supplies the ABI's
     * 16-byte pre-call alignment without changing incoming flags/registers.
     */
    pushfq
    call *capture_mailbox+8(%rip)
    leaq 8(%rsp),%rsp
    /* Keep all original post-call GP registers/flags. The helper uses no
     * SIMD/FP instructions, so all original SIMD state passes through.
     */
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
    call capture_after_present
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
    ret
.size _start,.-_start
.section .note.GNU-stack,"",@progbits
