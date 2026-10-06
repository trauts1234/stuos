target remote :1234
set architecture i386:x86-64
symbol-file stuos/.build/stuos
break kmain
break loop_hlt
continue