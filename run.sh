#! /bin/bash

qemu-system-x86_64 -m 4096M -drive file=image.iso,format=raw -serial stdio -no-reboot -no-shutdown -smp 1 # -S -s # -d cpu_reset,int