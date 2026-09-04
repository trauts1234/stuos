#! /bin/bash

FLAGS=""
if [[ "$1" == "debug" ]]; then
    FLAGS="-S -s"
fi

#-trace usb_xhci_* -D ./qemu.log
#-d cpu_reset,int

qemu-system-x86_64 $FLAGS -m 4096M \
  -drive file=filesystem.img,format=raw,if=none,id=usbstick \
  -device qemu-xhci,id=xhci \
  -device usb-storage,drive=usbstick \
  -serial stdio -no-reboot -no-shutdown \
  -d cpu_reset,int \
  -trace 'usb_*' -trace 'scsi_*' -D ./qemu.log
  # -device usb-kbd \

# qemu-system-x86_64 $FLAGS -m 4096M -drive if=virtio,file=filesystem.img,format=raw -serial stdio -no-reboot -no-shutdown