#ifndef FAT_H
#define FAT_H
#include "fs.h"

void mount_fat16(struct VNode block_device, const char* mount_name);

#endif