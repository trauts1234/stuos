#ifndef UAPI_STAT_H
#define UAPI_STAT_H

// #include <sys/stat.h>
#include "types.h"
#include "dirent.h"

/* File types.  */
#define S_IFMT   0170000 //everything or-ed together
#define	S_IFDIR	0040000	/* Directory.  */
#define	S_IFCHR	0020000	/* Character device.  */
#define	S_IFBLK	0060000	/* Block device.  */
#define	S_IFREG	0100000	/* Regular file.  */
#define	S_IFIFO	0010000	/* FIFO.  */
#define	S_IFLNK	0120000	/* Symbolic link.  */
#define	S_IFSOCK	0140000	/* Socket.  */

#define S_ISDIR(m)   (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)   (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)   (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)   (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m)  (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)   (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m)  (((m) & S_IFMT) == S_IFSOCK)

//TODO S_ISXYZ macros

//TODO commented values
struct stat {
    // dev_t     st_dev;     /* ID of device containing file */
    ino_t     st_ino;     /* inode number */
    mode_t    st_mode;    /* file type? */
    // nlink_t   st_nlink;   /* number of hard links */
    uid_t     st_uid;     /* user ID of owner */
    gid_t     st_gid;     /* group ID of owner */
    // dev_t     st_rdev;    /* device ID (if special file) */
    off_t     st_size;    /* total size, in bytes */
    // blksize_t st_blksize; /* blocksize for file system I/O */
    // blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
    // time_t    st_atime;   /* time of last access */
    // time_t    st_mtime;   /* time of last modification */
    // time_t    st_ctime;   /* time of last status change */
};

#endif