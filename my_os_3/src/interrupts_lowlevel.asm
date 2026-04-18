bits 64

global total_timer_interrupts
global vector_32_handler
global vector_33_handler
global vector_14_handler
global start_userland

extern run_next_task
extern memory_exception_handle
extern acknowledge_interrupt
extern tty_poll_keyboard

SECTION .data
total_timer_interrupts dq 0; counts the number of times the timer interrupt has gone off (uint64_t) - This is updated via assembly

section .text

; interrupt 14
vector_14_handler:
    add rsp, 8; get rid of error code
    pop rsi
    mov rdi, cr2
    ; TODO stack may not be aligned here!!
    ; and rsp, 0xFFFFFFFFFFFFFFF0
    ;this function doesn't return
    call memory_exception_handle

; interrupt 32
vector_32_handler:
    ; push the remaining registers to form a full processor state (certain registers have already been pushed)
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    

    ; increment the total number of timer interrupts
    mov rax, [total_timer_interrupts]
    inc rax
    mov [total_timer_interrupts], rax

    mov rdi, 32
    call acknowledge_interrupt; might be to re-enable interrupts?

    mov rax, [rsp + 128]; 8*15 to skip pushed register state, 8 to skip return address, to get cs
    test ax, 3; check if I interrupted a userland process
    jnz .definitely_from_userland

    ; kernel got interrupted, give up and return
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

    .definitely_from_userland:

    ; convert the stack into a struct, as it has the right layout - pass a pointer to it
    mov rdi, rsp
    call run_next_task; this should never return

vector_33_handler:
    ud2
    ; push the remaining registers to form a full processor state (certain registers have already been pushed)
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, 32
    call acknowledge_interrupt

    call tty_poll_keyboard

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; RDI - pointer to interrupt context struct
start_userland:
    mov rsp, rdi; pretend the struct that was pointed to from C is the stack

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq; return from the remaining struct data, which sets RSP and others