.global rco_jump_context
.text
.type rco_jump_context, @function

rco_jump_context:

#ifdef __i386__

    leaq (%esp), %eax
	
	// from context
	movl 4(%eax), %edx

	movl %esp,  0(%edx)
	movl %ebp,  4(%edx)
	movl %eax,  8(%edx)
	movl %ebx, 12(%edx)
	movl %edi, 16(%edx)

	// to context
	movl 8(%eax), %ecx

	movl  0(%edx), %esp 
	movl  4(%edx), %ebp
	movl  8(%edx), %eax 
	movl 12(%edx), %ebx
	movl 16(%edx), %edi

	ret

#elif __x86_64__

    leaq (%rsp), %rax

    movq %rsp,  0(%rdi)
    movq %rbp,  8(%rdi)
    movq %rax, 16(%rdi)
    movq %rbx, 24(%rdi)
    movq %rdi, 32(%rdi)
    movq %r12, 40(%rdi)
    movq %r13, 48(%rdi)
    movq %r14, 56(%rdi)
    movq %r15, 64(%rdi)

    movq  0(%rsi), %rsp
    movq  8(%rsi), %rbp
    movq 16(%rsi), %rax
    movq 24(%rsi), %rbx
    movq 32(%rsi), %rdi
    movq 40(%rsi), %r12
    movq 48(%rsi), %r13
    movq 56(%rsi), %r14
    movq 64(%rsi), %r15

    ret

#endif
