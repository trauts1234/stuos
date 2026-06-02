

global setjmp
global longjmp

setjmp:
    ;copy simple registers
    mov     [rdi + 0*8], rsi
    mov     [rdi + 1*8], rdi
    mov     [rdi + 2*8], rbx
    mov     [rdi + 3*8], r12
    mov     [rdi + 4*8], r13
    mov     [rdi + 5*8], r14
    mov     [rdi + 6*8], r15

    ;copy return address
    mov     rax, [rsp]
    mov     [rdi + 7*8], rax

    ;copy stack pointers
    mov     [rdi + 8*8], rbp
    mov     [rdi + 9*8], rsp

    mov rax, 0 ;return 0
    ret

longjmp:
    ; set esi to 1 if it is zero, as zero is reserved
    cmp esi, 0
    setz sil

    mov eax, esi; put in return address

    ;restore simple registers
    mov rsi, [rdi + 0*8]
    mov rdi, [rdi + 1*8]
    mov rbx, [rdi + 2*8]
    mov r12, [rdi + 3*8]
    mov r13, [rdi + 4*8]
    mov r14, [rdi + 5*8]
    mov r15, [rdi + 6*8]

    ; restore stack
    mov rbp, [rdi + 8*8]
    mov rsp, [rdi + 9*8]

    ;restore return address
    mov rax, [rdi + 7*8]
    mov [rsp], rax
    ret

