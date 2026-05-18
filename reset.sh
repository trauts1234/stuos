#!/bin/sh
set -e

#remove mount, loop device, and filesystem image
sudo umount filesystem_mnt || true
sudo losetup -j filesystem | awk -F: '{print $1}' | xargs -r sudo losetup -d
rm filesystem -f

#create new filesystem image and add MBR
fallocate -l 20M filesystem
sudo parted -s filesystem \
    mklabel msdos \
    mkpart primary fat16 1M 100% \
    set 1 boot on

#create loop device
LOOP_DEV=$(sudo losetup --find --show --partscan filesystem)
sudo mkfs.fat -F 16 -f 2 -S 512 "${LOOP_DEV}p1"
mkdir ./filesystem_mnt || true

#mount filesystem
sudo mount -o uid=$(id -u),gid=$(id -g) "${LOOP_DEV}p1" ./filesystem_mnt

./build_all.sh

sudo umount filesystem_mnt