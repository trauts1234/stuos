#! /bin/bash

gdb

# ni - next instruction in assembly
# step - next line of C
# next - next line of C in the innermost frame (don't jump to code in function calls)
# focus xxx - focus that window
# info stack - display a stack trace
# info registers xyz - display register xyz
# frame x - view frame number
# tui enable/disable - change view mode