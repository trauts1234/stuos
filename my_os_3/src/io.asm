bits 64

section .text

global in8
global out8
global in16
global out16
global in32
global out32
global spin_wait

;first param: rdi (port)
;second param: rsi (value)

; in x, y means x = read port y
; out x, y means write y to port x

in8:
    mov dx, di
    in al, dx
    ret

out8:
    mov dx, di
    mov al, sil
    out dx, al
    ret

in16:
    mov dx, di
    in ax, dx
    ret

out16:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

in32:
    mov dx, di
    in eax, dx
    ret

out32:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

spin_wait:
    mov rax, 100000
internal:
    nop
    dec rax
    test rax, rax
    jnz internal
    ret