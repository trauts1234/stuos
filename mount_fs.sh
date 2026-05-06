#!/bin/sh

sudo mount -o uid=$(id -u),gid=$(id -g) filesystem ./filesystem_mnt