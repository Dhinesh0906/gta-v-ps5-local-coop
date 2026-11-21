/* Private CBike dispatch only; no shared branch or ped-type edits. */
.section .entry,"ax",@progbits
.global _start
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
    call glider_apply
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
    jmp *root_copy(%rip)
.p2align 3
root_copy: .quad 0x62208000
.org 0x300
.global glider_balance_gate
glider_balance_gate:
    cmp $2,%eax
    je .accept_stock
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
    movq %rbx,%rdi
    call glider_apply
    test %eax,%eax
    jz .reject
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
    jmp *accept_copy(%rip)
.reject:
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
    jmp *reject_copy(%rip)
.accept_stock:
    jmp *accept_copy(%rip)
.p2align 3
accept_copy: .quad 0x62208000 + 0x26cbd08 - 0x26c8520
reject_copy: .quad 0x62208000 + 0x26cbe32 - 0x26c8520
.org 0x600
.global glider_base_gate
glider_base_gate:
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
    call glider_apply
    test %eax,%eax
    jz .base_restore
    movl glider_input+72(%rip),%eax
    movl %eax,-0x9b0(%rbp)
    movl glider_input+76(%rip),%eax
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
base_yes: .quad 0x62202000 + 0x264445f - 0x26428a0
base_no: .quad 0x62202000 + 0x264457c - 0x26428a0
