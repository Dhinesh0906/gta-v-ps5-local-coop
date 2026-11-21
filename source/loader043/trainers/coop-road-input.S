.section .entry,"ax",@progbits
.global _start
.hidden _start
.type _start,@function
_start:
#ifdef ROAD_PHYSICS_ENTRY
    /* Supply inputs before ANY stock tyre/brake/engine calculation. Preserve
     * all incoming registers/flags and leave stack arguments in place. */
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
    movq 80(%rsp),%rdi
    movl $0,%esi
    call road_after_input
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
    jmp *road_mailbox+8(%rip)
#else
    /* The fingerprinted base automobile 0x670 slot is a fingerprinted RET. Its caller
     * supplies only self and expects no return value or stack arguments. Keep
     * all registers intact. There are no
     * incoming stack arguments. Keep self, then preserve every post-call GP
     * register and flags. The observer is integer-only and makes no calls. */
    pushq %rdi
    call *road_mailbox+8(%rip)
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
    movq 128(%rsp),%rdi
    movl $0,%esi
    call road_after_input
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
    leaq 8(%rsp),%rsp
    ret
#endif
.size _start,.-_start
#ifdef ROAD_DUAL_INPUT
.p2align 4
.global road_mid_entry
.hidden road_mid_entry
.type road_mid_entry,@function
road_mid_entry:
    /* The base automobile's mid-step slot was a RET. Refresh inputs after
     * stock substeps too, preserving its complete incoming machine state. */
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
    movq 80(%rsp),%rdi
    movl $1,%esi
    call road_after_input
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
.size road_mid_entry,.-road_mid_entry
#endif
.section .note.GNU-stack,"",@progbits
