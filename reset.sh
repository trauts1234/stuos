#!/bin/sh

sudo umount filesystem_mnt
./make_empty_fs.sh
./mount_fs.sh
./build_all.sh
sudo umount filesystem_mnt