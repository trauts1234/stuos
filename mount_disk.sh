#! /bin/sh

LOOP_DEV=$(sudo losetup --find --show --partscan ./filesystem.img)
sudo mount -o uid=$(id -u),gid=$(id -g) "${LOOP_DEV}p1" ./filesystem_mnt