global _start

extern main
extern stuos_libc_init
extern exit

; RDI: argc
; RSI: argv
; RDX: envp
_start:
    sub rsp, 8; ensure stack is aligned according to SYSV
    push rdi
    push rsi
    push rdx
    call stuos_libc_init
    pop rdx
    pop rsi
    pop rdi
    add rsp, 8
    call main
    mov rdi, 0; TODO should be exit code of main 
    call exit
