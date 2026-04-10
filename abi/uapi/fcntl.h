#ifndef UAPI_FCNTL_H
#define UAPI_FCNTL_H

#define O_RDONLY 0b01
#define O_WRONLY 0b01
#define O_RDWR   0b11

#define O_APPEND    0b00000000100
#define O_CREAT     0b00000001000
#define O_DIRECTORY 0b00000010000
#define O_TRUNC     0b00000100000

#endif