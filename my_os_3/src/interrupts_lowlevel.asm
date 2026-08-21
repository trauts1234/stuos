bits 64

global total_timer_interrupts
global start_userland
global enable_apic
global vector_n_handlers

extern run_next_task
extern memory_exception_handle
extern apic_eoi
extern apic_timer_counter
extern handle_incoming_byte
extern general_purpose_interrupt_handlers

section .data

; the error handlers, vector 32 is the timer interrupt, and then general purpose interrupts
vector_n_handlers:
    resq 14
    dq vector_14_handler
    resq 17
    dq vector_32_handler
    %assign i 33
    %rep 222
    dq vector_%+i%+_handler
    %assign i i+1
    %endrep

section .text

enable_apic:
    ; get APIC base
    mov ecx, 0x1B
    rdmsr
    or eax, 0x800
    wrmsr
    ret

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

    call apic_timer_counter; do timer things
    call apic_eoi; re-enable interrupts

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

; create a stub for each interrupt from 33 onwards
%assign i 33
%rep 222
vector_%+i%+_handler:
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

    mov rax, [general_purpose_interrupt_handlers + 8 * %[i]]
    mov rdi, %[i]
    call rax
    call apic_eoi

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
%assign i i+1
%endrep

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