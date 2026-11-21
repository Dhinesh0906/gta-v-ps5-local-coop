#include "coop-deluxo-flight-profile.h"
.section .entry,"ax",@progbits
.global _start
_start:
#ifdef GTA_OWNED_HOVER
    /* Called only by the owned physics copy at the original argument-load
     * site. RBP is the unchanged stock method frame, R13 its vehicle. */
    vmovss -0x9b0(%rbp),%xmm2
    vmovss -0x9f0(%rbp),%xmm3
    vmovaps -0xa10(%rbp),%xmm1
#endif
    /* Entry to the existing hover-physics function. Preserve its arguments,
     * flags and scratch registers; alter only pitch/roll for the owned NPC. */
    pushfq
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
#ifdef GTA_OWNED_HOVER
    pushq %rdi
    movq %r13,%rdi
#endif
    lock incq flight_mailbox+24(%rip)
    cmpl $1,flight_mailbox+16(%rip)
    jne .forward
    movq flight_mailbox+8(%rip),%r8
    testq %r8,%r8
    jz .forward
    movabsq $0x315353504f4f43,%rax
    cmpq %rax,0x8000(%r8)
    jne .forward
    cmpl $2,0x8010(%r8)
    jne .forward
    cmpl $0,0x8014(%r8)
    jne .forward
    cmpl $0,0x8018(%r8)
    jne .forward
    cmpl $6,0x8020(%r8)
    jne .forward
    cmpl $3,0x8208(%r8)
    jne .forward
    cmpl $0,0x8218(%r8)
    jne .forward
    cmpl $1,0x8074(%r8)
    jne .forward
    cmpl $1,0x80fc(%r8)
    jne .forward
    movl 0x802c(%r8),%eax
    cmpl flight_mailbox+20(%rip),%eax
    jae .forward
    movabsq $0x5abefb8,%rax
    cmpb $0,(%rax)
    jne .forward
    cmpl $DELUXO_MODEL,0x85d4(%r8)
    jne .forward
    cmpl $1,0x85f0(%r8)
    jne .forward
    cmpl $1,0x8628(%r8)
    jne .forward
    movl 0x8054(%r8),%eax
    cmpl 0x8050(%r8),%eax
    je .forward
    movl 0x8450(%r8),%eax
    call .lookup
    cmpq %rax,%rdi
    jne .forward
    movl 0x8054(%r8),%eax
    call .lookup
    testq %rax,%rax
    jz .forward
    /* Exact stock driver index and seat array, also used by the audited
     * primary flight-input branch. Protect P1 even between script ticks. */
    movsbl 0xc71(%rdi),%ecx
    testl %ecx,%ecx
    js .forward
    movzbl 0xc70(%rdi),%edx
    cmpl %edx,%ecx
    jae .forward
    cmpq %rax,0xc78(%rdi,%rcx,8)
    jne .forward
    movss 0x8100(%r8),%xmm2
    movss 0x8104(%r8),%xmm3
    xorps .sign(%rip),%xmm3
    lock incq flight_mailbox+32(%rip)
    movq %rdi,flight_mailbox+40(%rip)
    movss %xmm2,flight_mailbox+48(%rip)
    movss %xmm3,flight_mailbox+52(%rip)
.forward:
#ifdef GTA_OWNED_HOVER
    popq %rdi
#endif
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
    popfq
#ifdef GTA_OWNED_HOVER
    ret
#else
    /* Original whole-instruction prologue, without RIP-relative operands. */
    .byte ORIGINAL_PROLOGUE_BYTES
    jmp *.continuation(%rip)
#endif
.lookup:
    testl %eax,%eax
    jle .absent
    movabsq $0x61a8250,%rdx
    movq (%rdx),%rdx
    testq %rdx,%rdx
    jz .absent
    movl %eax,%ecx
    shrl $8,%ecx
    cmpl 0x18(%rdx),%ecx
    jae .absent
    movq 0x10(%rdx),%r10
    testq %r10,%r10
    jz .absent
    cmpb %al,(%r10,%rcx)
    jne .absent
    movl 0x1c(%rdx),%r11d
    cmpl $16,%r11d
    jb .absent
    cmpl $256,%r11d
    ja .absent
    imull %r11d,%ecx
    movq 8(%rdx),%rdx
    testq %rdx,%rdx
    jz .absent
    movq 8(%rdx,%rcx),%rax
    ret
.absent:
    xorl %eax,%eax
    ret
.p2align 4
.sign: .long 0x80000000,0,0,0
.continuation: .quad HOVER_CONTINUATION
#ifdef GTA_OWNED_HOVER
    /* Fixed owned-image entry. Keep the real concrete method's complete
     * prologue intact; the tail branch introduces no additional frame. */
    .org 0x800
.global owned_physics_entry
owned_physics_entry:
    pushfq
    lock incq flight_mailbox+64(%rip)
    popfq
    .byte 0xe9
    .long 0x6800 - (. - _start) - 4
#endif
.section .flight.data,"aw",@progbits
.global flight_mailbox
flight_mailbox: .zero 128
.section .note.GNU-stack,"",@progbits
