target remote :1234
set architecture i386:x86-64
symbol-file my_os_3/.build/myos
break kmain
continue