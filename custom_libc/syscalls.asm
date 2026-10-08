global do_syscall

;passes data pointer in RDI and syscall number in RSI
do_syscall:
    ;like a function call, I don't guarantee that all registers will be preserved with this call
    syscall
    ret