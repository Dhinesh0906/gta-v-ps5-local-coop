/* Pass-through observer for two verified engine schedule nodes.
 * Preserve every incoming register/flag; touch no SIMD state; tail-jump to
 * the original once. No game calls, graphics commands or character changes.
 * The host derives/checks each absolute read address from this build's code.
 */
.section .entry,"ax",@progbits
.macro OBSERVE stats, forward
    pushfq
    pushq %rax
    pushq %rcx
    pushq %rdx
    leaq observer_mailbox(%rip), %rax
    cmpl $1, 8(%rax)
    jne 9f
    lock incq \stats(%rax)
    movq %fs:0, %rcx
    cmpq $0, \stats+8(%rax)
    je 1f
    cmpq %rcx, \stats+8(%rax)
    je 1f
    incl \stats+60(%rax)
1:  movq %rcx, \stats+8(%rax)
    movl -0x40(%rcx), %edx
    movl %edx, \stats+24(%rax)
    movq -0x38(%rcx), %rdx
    movq %rdx, \stats+32(%rax)
    movabsq $0x61fbd24, %rdx
    movl (%rdx), %ecx
    cmpq $1, \stats(%rax)
    jne 2f
    movl %ecx, \stats+16(%rax)
2:  cmpl %ecx, \stats+20(%rax)
    jne 3f
    incl \stats+28(%rax)
    jmp 4f
3:  incl \stats+56(%rax)
4:  movl %ecx, \stats+20(%rax)
    movabsq $0x4402570, %rdx
    movq (%rdx), %rcx
    movq %rcx, \stats+40(%rax)
    movabsq $0x61fc5e8, %rdx
    movq (%rdx), %rcx
    movq %rcx, \stats+48(%rax)
9:  popq %rdx
    popq %rcx
    popq %rax
    popfq
    jmp *observer_mailbox+\forward(%rip)
.endm
.global _start
.hidden _start
.type _start,@function
_start:
    OBSERVE 64, 16
.size _start,.-_start
.org 0x200
.global _phase4
.hidden _phase4
.type _phase4,@function
_phase4:
    OBSERVE 128, 24
.size _phase4,.-_phase4

.section .observer.data,"aw",@nobits
.balign 8
.global observer_mailbox
.hidden observer_mailbox
observer_mailbox:
    .zero 256
