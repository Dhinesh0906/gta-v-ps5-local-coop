.section .entry,"ax",@progbits
.global _start
_start:
    /* Exact stock loads replaced in our private base-physics copy. */
    vmovss -0x9b0(%rbp),%xmm2
    vmovss -0x9f0(%rbp),%xmm3
    vmovaps -0xa10(%rbp),%xmm1
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
    /* At the stock call site RSP was 16-byte aligned; CALL adds eight,
     * and these sixteen saved words preserve that alignment offset. */
    subq $8,%rsp
    movq %r13,%rdi
    call oppressor_apply
    testl %eax,%eax
    jz .restore
    vmovss oppressor_input+72(%rip),%xmm2
    vmovss oppressor_input+76(%rip),%xmm3
.restore:
    addq $8,%rsp
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

#ifdef GTA_OPPRESSOR_PRODUCTION
.org 0x600
.global oppressor_glider_gate
oppressor_glider_gate:
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
    movq %r13,%rdi
    call oppressor_apply
    test %eax,%eax
    jz .base_restore
    movl oppressor_input+72(%rip),%eax
    movl %eax,-0x9b0(%rbp)
    movl oppressor_input+76(%rip),%eax
    movl %eax,-0x9f0(%rbp)
.base_restore:
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
    test %bl,%bl
    je .base_non_glider
    vxorps %xmm0,%xmm0,%xmm0
    jmp *base_yes(%rip)
.base_non_glider:
    jmp *base_no(%rip)
.p2align 3
base_yes: .quad 0x62c02000 + 0x264445f - 0x26428a0
base_no: .quad 0x62c02000 + 0x264457c - 0x26428a0

#endif
