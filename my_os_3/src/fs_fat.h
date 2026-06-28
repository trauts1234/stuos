#ifndef FAT_H
#define FAT_H
#include "fs.h"

//Take a file as a block device, and mount it under the specified mount name
void mount_fat16(struct VNode block_device, const char* mount_name);

#endif