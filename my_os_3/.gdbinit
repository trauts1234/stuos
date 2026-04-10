target remote :1234
set architecture i386:x86-64
symbol-file src/bin/myos
break kmain
continue