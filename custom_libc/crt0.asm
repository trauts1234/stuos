global _start

extern main
extern HALT_SYSCALL

_start:
    call main
    mov rax, 0; halt syscall number
    syscall
