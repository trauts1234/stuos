#!/bin/sh
set -euo pipefail

SFDISK=/sbin/sfdisk
PARTED=/sbin/parted
MKFAT=/sbin/mkfs.fat

LIMINE_FOLDER=./ports/limine
LIMINE_CONFIG=./stuos/src/limine.conf
PUT_IN_FILESYSTEM=./put_in_filesystem

OUTPUT_DISK=./filesystem.img
OUTPUT_SYSROOT=./sysroot

# sets up folders required
setup_sysroot() {
    mkdir -p \
        "${OUTPUT_SYSROOT}/dev" \
        "${OUTPUT_SYSROOT}/usr/include/uapi" \
        "${OUTPUT_SYSROOT}/usr/lib" \
        "${OUTPUT_SYSROOT}/boot/limine" \
        "${OUTPUT_SYSROOT}/EFI/BOOT"
}

build_uapi() {
    cp ./abi/uapi/*.h "${OUTPUT_SYSROOT}/usr/include/uapi/"
}

build_bootloader() {
    make -C "${LIMINE_FOLDER}"

    cp "${LIMINE_CONFIG}" \
       "${LIMINE_FOLDER}/limine-bios.sys" \
       "${LIMINE_FOLDER}/limine-bios-cd.bin" \
       "${LIMINE_FOLDER}/limine-uefi-cd.bin" \
       "${OUTPUT_SYSROOT}/boot/limine/"

    cp "${LIMINE_FOLDER}/BOOTX64.EFI" \
       "${LIMINE_FOLDER}/BOOTIA32.EFI" \
       "${OUTPUT_SYSROOT}/EFI/BOOT/"
}

build_os() {
    make -C stuos
    cp stuos/.build/stuos "${OUTPUT_SYSROOT}/boot/stuos"
}

build_libc() {
    make -C custom_libc/
    cp -r custom_libc/include/. "${OUTPUT_SYSROOT}/usr/include/"
    cp custom_libc/.build/crt*.o custom_libc/.build/libc.a "${OUTPUT_SYSROOT}/usr/lib/"
}

build_tcc() {
    make -C ports/tcc cross-x86_64 install
    cp ports/tcc/linux_to_stuos/lib/tcc/x86_64-libtcc1.a "${OUTPUT_SYSROOT}/usr/lib/libtcc1.a"
    cp ports/tcc/linux_to_stuos/bin/x86_64-tcc ./x86_64-tcc
}

build_binaries() {
    make -C ports/fuzzing
    make -C ports/coreutils
}

create_disk_image() {
    rm -f "${OUTPUT_DISK}"
    fallocate -l 15M "${OUTPUT_DISK}"

    # Create MBR with a partition at 1MB offset
    "${PARTED}" -s "${OUTPUT_DISK}" \
        mklabel msdos \
        mkpart primary fat16 1M 100% \
        set 1 boot on

    "${MKFAT}" -F 16 -f 2 -S 512 --offset 2048 "${OUTPUT_DISK}"

    mcopy -Q -s -i "${OUTPUT_DISK}"@@1M ./${OUTPUT_SYSROOT}/* ::/

    "${LIMINE_FOLDER}/limine" bios-install "${OUTPUT_DISK}"
}

build() {
    setup_sysroot
    build_uapi
    build_bootloader
    build_os
    build_libc
    build_tcc
    build_binaries
    create_disk_image
}

clean() {
    rm -rf "${OUTPUT_DISK}" "${OUTPUT_SYSROOT}"
    make -C custom_libc/ clean
    make -C ports/fuzzing clean
    make -C ports/coreutils clean
    make -C stuos/ clean
}

case "${1:-build}" in
    build)
        build
        ;;
    clean)
        clean
        ;;
    *)
        echo "Usage: $0 {build|clean}" >&2
        exit 1
        ;;
esac