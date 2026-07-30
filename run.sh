#! /bin/bash

FLAGS=""
if [[ "$1" == "debug" ]]; then
    FLAGS="-S -s -d cpu_reset,int"
fi

qemu-system-x86_64 $FLAGS -m 4096M \
  -drive file=filesystem,format=raw,if=none,id=usbstick \
  -device usb-ehci,id=ehci \
  -device usb-storage,bus=ehci.0,drive=usbstick \
  -serial stdio -no-reboot -no-shutdown