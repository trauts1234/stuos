bits 64

section .text

global loop_hlt
global check_cpuid_sse
global enable_sse
global get_pml4_phys
global set_pml4_phys
global invalidate_page
global apply_gdt_tss
global apply_idt
global syscall_init

extern syscall_table
extern syscall_stack_top

%define KERN_CS 0x08
%define KERN_SS 0x10
%define USER_CS 0x1B
%define USER_SS 0x23

; stops permanently - call this if you need to crash
loop_hlt:
    hlt
    jmp loop_hlt

; returns (bool) 1 if the CPU supports SSE, else 0
check_cpuid_sse:
    push rbx; this is preserved in SysV ABI

    mov eax, 1 ; CPUID leaf 1 (seems to have most information)
    cpuid
    and edx, 0x06_00_00_00; get only the SSE and SSE2 bits
    xor edx, 0x06_00_00_00; flip the (hopefully set) bits to get 0
    cmp edx, 0
    sete al; if it is actually 0, set boolean to 1

    pop rbx
    ret

; sets up SSE support
enable_sse:
    mov rax, cr0
    and rax, ~0x4        ; clear EM (bit 2)
    or  rax, 0x2         ; set MP (bit 1)
    mov cr0, rax

    mov rax, cr4
    or  rax, (1 << 9) | (1 << 10)  ; set OSFXSR + OSXMMEXCPT
    mov cr4, rax
    ret

; reads CR3 and masks out the crud in the lower bits
get_pml4_phys:
    mov rax, cr3
    and rax, 0xFFFFFFFFFFFFF000 ; clear the low 12 bits (flags)
    ret
; sets the CR3 
set_pml4_phys:
    and rdi, 0xFFFFFFFFFFFFF000 ; clear the low 12 bits (flags)
    mov cr3, rdi
    ret

invalidate_page:
    invlpg [rdi]
    ret

apply_gdt_tss:
    cli ; stops interrupts

    lgdt [rdi]               ; rdi -> {limit, base} for your *new* GDT

    ; Reload CS from the NEW GDT
    push KERN_CS
    lea rax, [rel .cs_done]
    push rax
    retfq                       ; pops RIP then CS

    .cs_done:
    ; Now reload data segments from the NEW GDT
    mov ax, KERN_SS
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov eax, 0           ; FS/GS often left null; bases set via MSRs if needed
    mov fs, ax
    mov gs, ax

    ; Load 64-bit TSS descriptor (type = Available 64-bit TSS, uses two GDT slots)
    mov    ax, 0x28            ; TSS selector in your new GDT
    ltr    ax
    ret

apply_idt:
    lidt [rdi]
    sti
    ret

; stick with borrowing the user's stack for now
; passed: syscall number in RAX, syscall data pointer in RDI
handle_syscall:

    mov rsi, rsp ; put user RSP in RSI
    mov rsp, [syscall_stack_top] ; set stack to syscall stack

    ; set up the stack for iretq
    push USER_SS
    push rsi ; store user RSP
    push r11 ; store user RFLAGS
    push USER_CS
    push rcx; store user RIP

    push rax
    push rbx
    push 0; rcx is storing the user's RIP
    push rdx
    push rbp
    push rdi
    push 0; rsi is storing the old RSP
    push r8
    push r9
    push r10
    push 0; r11 stores the user's RFLAGS
    push r12
    push r13
    push r14
    push r15

    mov rax, [syscall_table + rax*8]; calculate syscall address from syscall number
    mov rsi, rsp; convert the stack into a struct, as it has the right layout - pass a pointer to it
    call rax

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
    iretq; pops all other values

syscall_init:
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1; set the syscall enable bit
    wrmsr

    mov ecx, 0xC0000081
    mov edx, 0x00180008 ; upper 32 bits in this reg
    wrmsr

    mov ecx, 0xC0000082
    mov rdx, handle_syscall
    mov rax, rdx
    shr rdx, 32 ; get upper bits in edxi
    mov eax, eax ; get lower bits in eax
    wrmsr

    mov ecx, 0xC0000084
    mov rdx, 0
    mov eax, 1<<9; disable interrupts when calling a syscall
    wrmsr
    ret