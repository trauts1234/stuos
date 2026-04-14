#include "errno.h"

int __errno_value = 0;

const char *sys_errlist[] = {};
int sys_nerr = 0;