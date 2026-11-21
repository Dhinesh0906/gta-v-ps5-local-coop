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
call bike_engine_apply
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
_start_target: .quad 40666400
.org 768
.global bike_engine_entry
.type bike_engine_entry,@function
bike_engine_entry:
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
call bike_engine_apply
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
jmp *bike_engine_entry_target(%rip)
.size bike_engine_entry,.-bike_engine_entry
bike_engine_entry_target: .quad 0x62a04000
.org 1536
.global bike_engine_consume
.type bike_engine_consume,@function
bike_engine_consume:
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
movq %rbx,%rdi
movl $2,%esi
call bike_engine_apply
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
jmp *bike_engine_consume_target(%rip)
.size bike_engine_consume,.-bike_engine_consume
bike_engine_consume_target: .quad 40302048
