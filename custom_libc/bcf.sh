#!/bin/bash

make
make -C fuzzing/
cp fuzzing/*.out ../my_os_3/included_binaries/