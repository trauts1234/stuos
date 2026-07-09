global _start

extern main
extern stuos_libc_init
extern HALT_SYSCALL

_start:
    push rdi
    push rsi
    call stuos_libc_init
    pop rsi
    pop rdi
    call main
    mov rax, 0; halt syscall number
    syscall
