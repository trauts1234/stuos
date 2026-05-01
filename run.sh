#! /bin/bash

if [[ "$1" == "debug" ]]; then
    qemu-system-x86_64 -m 4096M -drive file=stuos_image.iso,format=raw -drive if=virtio,file=filesystem,format=raw -serial stdio -no-reboot -no-shutdown -smp 1 -S -s -d cpu_reset,int
else
    qemu-system-x86_64 -m 4096M -drive file=stuos_image.iso,format=raw -drive if=virtio,file=filesystem,format=raw -serial stdio -no-reboot -no-shutdown -smp 1 # -S -s # -d cpu_reset,int
fi
