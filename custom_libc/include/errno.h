#ifndef ERRNO_H
#define ERRNO_H

#include "uapi/errno.h"

//should contain string representations of all errors
extern const char *sys_errlist[];
extern int sys_nerr;
//current error number value
extern int __errno_value;

#define errno __errno_value

#endif