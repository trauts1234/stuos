#! /bin/bash

FLAGS=""
if [[ "$1" == "debug" ]]; then
    FLAGS="-S -s -d cpu_reset,int -trace usb_xhci_* -D ./qemu-xhci.log"
fi

qemu-system-x86_64 $FLAGS -m 4096M \
  -drive file=filesystem,format=raw,if=none,id=usbstick \
  -device qemu-xhci,id=xhci \
  -device usb-storage,drive=usbstick \
  -serial stdio -no-reboot -no-shutdown