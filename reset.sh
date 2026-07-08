#!/bin/sh
set -e

LIMINE_FOLDER=./limine
OUTPUT_DISK=./filesystem
OUTPUT_DIR=./filesystem_mnt
LIMINE_CONFIG=./my_os_3/src/limine.conf
PUT_IN_FILESYSTEM=./put_in_filesystem

#remove mount, loop device, and filesystem image
sudo umount filesystem_mnt || true
# sudo losetup -j filesystem | awk -F: '{print $1}' | xargs -r sudo losetup -d
sudo losetup -D
rm "${OUTPUT_DISK}" -f

#create new filesystem image and add MBR
fallocate -l 20M "${OUTPUT_DISK}"
sudo parted -s "${OUTPUT_DISK}" \
    mklabel msdos \
    mkpart primary fat16 1M 100% \
    set 1 boot on

#create loop device
LOOP_DEV=$(sudo losetup --find --show --partscan "${OUTPUT_DISK}")
sudo mkfs.fat -F 16 -f 2 -S 512 "${LOOP_DEV}p1"
mkdir ./filesystem_mnt || true

#mount filesystem
sudo mount -o uid=$(id -u),gid=$(id -g) "${LOOP_DEV}p1" ./filesystem_mnt

mkdir -p ${OUTPUT_DIR}/boot/limine ${OUTPUT_DIR}/EFI/BOOT ${OUTPUT_DIR}/dev
cp ${LIMINE_CONFIG} ${LIMINE_FOLDER}/limine-bios.sys ${LIMINE_FOLDER}/limine-bios-cd.bin ${LIMINE_FOLDER}/limine-uefi-cd.bin ${OUTPUT_DIR}/boot/limine/
cp ${LIMINE_FOLDER}/BOOTX64.EFI ${LIMINE_FOLDER}/BOOTIA32.EFI ${OUTPUT_DIR}/EFI/BOOT

./build_all.sh

cp ./my_os_3/.build/myos ${OUTPUT_DIR}/boot
cp ${PUT_IN_FILESYSTEM}/* ${OUTPUT_DIR}/
${LIMINE_FOLDER}/limine bios-install "${OUTPUT_DISK}"

sudo umount filesystem_mnt
sudo losetup -D