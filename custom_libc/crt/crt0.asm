global _start

extern main
extern stuos_libc_init
extern exit

_start:
    push rdi
    push rsi
    call stuos_libc_init
    pop rsi
    pop rdi
    call main
    mov rdi, 0
    call exit
