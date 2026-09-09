#ifndef RESOURCE_H
#define RESOURCE_H

#include "uapi/resource.h"

int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);

#endif