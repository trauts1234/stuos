section .note.GNU-stack progbits

global do_syscall

do_syscall:
    mov rax, rsi; put syscall number in rax
    ; syscall data pointer is already in rdi
    syscall
    ret