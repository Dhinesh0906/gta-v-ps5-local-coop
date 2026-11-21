/* Bounded fixture-only observer. No shared game code edits. */
.section .entry,"ax",@progbits
.org 0
.global _start
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
movq %rdi,%rdi
movl $0,%esi
call trike_engine_apply
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
jmp *_start_target(%rip)
.size _start,.-_start
_start_target: .quad 0x26bd7d0
.org 768
.global trike_engine_entry
.type trike_engine_entry,@function
trike_engine_entry:
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
movq %rdi,%rdi
movl $1,%esi
call trike_engine_apply
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
jmp *trike_engine_entry_target(%rip)
.size trike_engine_entry,.-trike_engine_entry
trike_engine_entry_target: .quad 0x63004000
.org 1536
.global trike_engine_consume
.type trike_engine_consume,@function
trike_engine_consume:
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
movq %r15,%rdi
movl $2,%esi
call trike_engine_apply
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
jmp *trike_engine_consume_target(%rip)
.size trike_engine_consume,.-trike_engine_consume
trike_engine_consume_target: .quad 40302048
