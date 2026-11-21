/* Preserve all incoming machine state around an integer-only callback.
 * Physics sites replay the exact displaced pitch load. (AI-assisted) */
.macro save_all
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
.endm
.macro restore_all
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
.endm
.section .entry,"ax",@progbits
.global _start
.hidden _start
.type _start,@function
_start:
 save_all
 movq 112(%rsp),%rdi /* saved RBX: original CPlane self at both load sites */
 movl $1,%esi
 call plane_before_input
 restore_all
 vmovss 0x1ecc(%rbx),%xmm0
 ret
.size _start,.-_start
.p2align 4
.global plane_vtol_input
.hidden plane_vtol_input
.type plane_vtol_input,@function
plane_vtol_input:
 save_all
 movq 112(%rsp),%rdi
 movl $2,%esi
 call plane_before_input
 restore_all
 vmovss 0x1ecc(%rbx),%xmm0
 ret
.size plane_vtol_input,.-plane_vtol_input
.section .engine_entry,"ax",@progbits
.global plane_engine_entry
.hidden plane_engine_entry
.type plane_engine_entry,@function
plane_engine_entry:
 save_all
 movq 80(%rsp),%rdi /* saved incoming RDI: vehicle */
 xorl %esi,%esi
 call plane_before_input
 restore_all
 jmp *plane_late+8(%rip)
.size plane_engine_entry,.-plane_engine_entry
.section .note.GNU-stack,"",@progbits
