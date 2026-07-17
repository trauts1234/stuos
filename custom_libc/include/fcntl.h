#ifndef FCNTL_H
#define FCNTL_H

#include "uapi/fcntl.h"

int open(const char *pathname, int flags, ...);
int open64(const char *pathname, int flags, ...);

/* Values for the second argument to `fcntl'.  */
#define F_DUPFD		0	/* Duplicate file descriptor.  */
#define F_GETFD		1	/* Get file descriptor flags.  */
#define F_SETFD		2	/* Set file descriptor flags.  */
#define F_GETFL		3	/* Get file status flags.  */
#define F_SETFL		4	/* Set file status flags.  */

int fcntl(int fildes, int cmd, ...);

#endif