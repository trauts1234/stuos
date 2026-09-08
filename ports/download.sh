#!/bin/sh

git clone https://github.com/limine-bootloader/limine.git --branch=v11.x-binary --depth=1 ./limine

git clone https://github.com/Tiny-C-Compiler/tinycc-mirror-repository.git ./tcc

#build tcc since I will soon use ./tcc to build a native compiler
(cd tcc && ./configure --prefix=./linux_to_stuos --enable-cross --cpu=x86_64 --targetos=none --sysroot=/home/stuart/Programming/stuos/sysroot)
make -C tcc cross-x86_64 install