#! /bin/sh

make -C my_os_3/ $1
make -C custom_libc/ $1
make -C custom_libc/fuzzing $1