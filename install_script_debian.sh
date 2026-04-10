#! /bin/sh

set -e
set -o xtrace

if ! command -v brew $> /dev/null; then
    echo "Install homebrew from https://brew.sh/"

if ! command -v cargo $> /dev/null; then
    echo "install rust from https://doc.rust-lang.org/cargo/getting-started/installation.html"

brew install x86_64-elf-gcc
sudo apt install qemu xorriso git

git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1 my_os_3/limine
make -C my_os_3/limine