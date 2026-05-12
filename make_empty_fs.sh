#!/bin/sh

rm filesystem 2> /dev/null
fallocate -l 20M filesystem
sudo mkfs.fat -F 16 -f 2 -S 512 filesystem
mkdir ./filesystem_mnt 2> /dev/null