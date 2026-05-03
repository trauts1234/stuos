#!/bin/sh
set -e

rm filesystem
fallocate -l 20M filesystem
sudo mkfs.fat -F 16 -f 2 -S 512 filesystem
mkdir ./filesystem_mnt
sudo mount -o uid=$(id -u),gid=$(id -g) filesystem ./filesystem_mnt