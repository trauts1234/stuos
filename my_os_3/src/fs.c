#include "fs.h"
#include "debugging.h"
#include "kern_libc.h"
#include <uapi/stat.h>
#include <uapi/stdint.h>
#include <uapi/stdbool.h>
#include <uapi/fcntl.h>
#include <uapi/types.h>

#define ROOT_INODE_NUM 69
#define TMPFS_DEV 70
#define MAX_MOUNTS 10

struct MountPoint2 {
    struct VNode parent;
    struct VNode replacement;
} mounts[MAX_MOUNTS] = {};

static uint64_t path_segment_len(const char* path) {
    uint64_t size = 0;
    while(*path != '\0' && *path != '/') {
        size++;
        path++;
    }
    return size;
}

bool is_eq_vnode(struct VNode l, struct VNode r) {
    return 1 &&
        l.id.inode == r.id.inode &&
        l.id.mount_id == r.id.mount_id &&
        l.read_dirents == r.read_dirents &&
        l.write_file == r.write_file &&
        l.read_file == r.read_file &&
        l.create_inode == r.create_inode &&
        l.stat_file == r.stat_file;
}

bool is_null_vnode(struct VNode v) {
    return 1 &&
        v.id.inode == 0 &&
        v.id.mount_id == 0 &&
        v.read_dirents == 0 &&
        v.write_file == 0 &&
        v.read_file == 0 &&
        v.create_inode == 0 &&
        v.stat_file == 0;
}

static struct stat tmpfs_stat_file(struct VNodeData id) {
    if(id.mount_id != 0) HCF

    switch(id.inode) {
        case ROOT_INODE_NUM:
        case TMPFS_DEV:
        return (struct stat) {
            .st_ino = id.inode,
            .st_mode = S_IFDIR,
            .st_size = 0,//TODO
        };
    }

    HCF
}

static uint64_t tmpfs_read_dirents(struct VNodeData inode_num, uint64_t dirent_index, struct dirent* dirent_buf, struct VNode* vnode_buf, uint64_t dirent_count) {
    struct dirent dirents[] = {
        [0] = {
            .d_ino = TMPFS_DEV,
            .d_name = "dev"
        }
    };
    struct VNode vnodes[] = {
        [0] = {
            .id = {
                .inode = TMPFS_DEV,
                .mount_id = 0
            },
            .stat_file = tmpfs_stat_file
        }
    };

    const uint64_t num_dirents = sizeof(dirents) / sizeof(struct dirent);
    if(num_dirents != sizeof(vnodes)/sizeof(struct VNode)) HCF

    uint64_t i=0;
    while((i+dirent_index) < num_dirents && i < dirent_count) {
        if(dirent_buf) dirent_buf[i] = dirents[i+dirent_index];
        if(vnode_buf) vnode_buf[i] = vnodes[i+dirent_index];
        i++;
    }

    return i;
}

struct VNode vfs_get_root() {
    return (struct VNode) {
        .id = {
            .inode = ROOT_INODE_NUM,
            .mount_id = 0
        },
        .read_dirents = tmpfs_read_dirents,
        .stat_file = tmpfs_stat_file,
    };
}

enum StepPathResult {
    /// all parsing is done - *path_start is now set to NULL and *current is valid
    STEPPATH_COMPLETE,
    /// Parsing is incomplete and must be run again - *path_start points to the remainder of the path to be parsed, and *current is an intermediate vnode
    STEPPATH_INPROGRESS,
    /// Parsing failed at the last item, *path_start is not modified and *current points to the directory that should have stored the last file
    STEPPATH_NOTEXIST_LAST,
    /// Parsing failed somewhere, *path_start points to the remaining path and *current points to the directory walked to so far
    STEPPATH_NOTEXIST
};

#define DIR_LOOKUP_BUF_SIZE 2
//returns: 0 on success, -1 otherwise
static int directory_lookup(struct VNode* dir, const char* name, struct VNode* out) {
    struct VNode vnode_buf[DIR_LOOKUP_BUF_SIZE];
    struct dirent dirent_buf[DIR_LOOKUP_BUF_SIZE];

    uint64_t offset = 0;
    while(1) {
        uint64_t res = dir->read_dirents(dir->id, offset, dirent_buf, vnode_buf, DIR_LOOKUP_BUF_SIZE);
        if(res == 0) {
            return -1;
        }
        for(uint64_t i=0; i<res; i++) {
            if(strcmp(dirent_buf[i].d_name, name) == 0) {
                //names match:
                *out = vnode_buf[i];
                return 0;
            }
        }
        offset += res;
    }
}

//if current is the host for a mount, current is also replaced with the root of the mounted directory
static enum StepPathResult step_path2(struct VNode* current, const char** path_start) {
    //if starts with root, return root node
    if(**path_start == '/') {
        (*path_start)++;
        *current = vfs_get_root();
        return STEPPATH_INPROGRESS;
    }

    //if a mountpoint, replace with the mounted root dir
    for(int i=0; i<MAX_MOUNTS; i++) {
        if(is_eq_vnode(mounts[i].parent, *current)) {
            *current = mounts[i].replacement;
            return STEPPATH_INPROGRESS;
        }
    }

    //path is invalid or empty, so return
    if(*path_start == NULL || **path_start == '\0') {
        *path_start = NULL;
        return STEPPATH_COMPLETE;
    }

    uint64_t segment_len = path_segment_len(*path_start);
    char* segment_cpy = malloc(segment_len + 1);
    memcpy(segment_cpy, *path_start, segment_len);
    segment_cpy[segment_len] = '\0';

    struct VNode result;
    int status = directory_lookup(current, segment_cpy, &result);
    free(segment_cpy);

    bool must_be_folder;
    bool was_last_segment;
    if((*path_start)[segment_len] == '/' && (*path_start)[segment_len+1] == '\0') {
        must_be_folder = true;
        was_last_segment = true;
    } else if ((*path_start)[segment_len] == '\0') {
        must_be_folder = false;
        was_last_segment = true;
    } else {
        must_be_folder = false;
        was_last_segment = false;
    }

    if(status == -1) {
        if(was_last_segment) {
            return STEPPATH_NOTEXIST_LAST;
        } else {
            return STEPPATH_NOTEXIST;
        }
    }

    if(must_be_folder && !S_ISDIR(result.stat_file(result.id).st_mode)) HCF

    *current = result;

    if(was_last_segment) {
        *path_start = NULL;
        return STEPPATH_COMPLETE;
    } else {
        *path_start += segment_len + 1;
        return STEPPATH_INPROGRESS;
    }
}

void vfs_add_mount(struct VNode mount_against, struct VNode filesystem_root) {
    struct MountPoint2* curr = mounts;
    for(; !is_null_vnode(curr->replacement); curr++) {
        if(is_eq_vnode(curr->parent, mount_against)) HCF//already mounted
        if(mounts-curr == MAX_MOUNTS) {HCF}/// out of mount points
    }

    *curr = (struct MountPoint2) {
        .parent = mount_against,
        .replacement = filesystem_root
    };
}

struct VNode vfs_get(const char* cwd_path, const char* path, int open_flags) {
    //walk the cwd to generate a vnode (this destroys the pointer cwd_path)
    struct VNode location = vfs_get_root();//so that a relative CWD is relative to root
    while(cwd_path) {
        switch (step_path2(&location, &cwd_path)) {

        case STEPPATH_COMPLETE:
            break;// cwd_path is null and loop will finish
        case STEPPATH_INPROGRESS:
            break;// cwd_path is not null and will continue
        case STEPPATH_NOTEXIST_LAST:
        case STEPPATH_NOTEXIST:
            HCF//CWD doesn't exist?
        }
    }
    //walk the path to generate the actual locaiton (this destroys the pointer path)
    bool dbg_have_created_inode = false;
    while(path) {
        switch (step_path2(&location, &path)) {

        case STEPPATH_COMPLETE:
            break;// path is null and loop will finish
        case STEPPATH_INPROGRESS:
            break;// path is not null and will continue
        case STEPPATH_NOTEXIST_LAST:
            if(open_flags & O_CREAT) {
                if (open_flags & O_DIRECTORY) HCF
                if(dbg_have_created_inode) HCF
                //since not a dir, the last bit of the `path` is a valid string that I can pass
                location.create_inode(location.id, S_IFREG, path, &location);
                dbg_have_created_inode = true;
                continue;//try and read the file now I have created it
            }
            /* FALLTHROUGH, since I have doesn't exist and no OPEN_CREATE */
            [[ fallthrough ]];
        case STEPPATH_NOTEXIST:
            printf("failed to parse remaining part of path: %s", path);
            HCF
        default:
            HCF
        }
    }

    mode_t location_mode = location.stat_file(location.id).st_mode;
    if(!S_ISDIR(location_mode) && open_flags & O_DIRECTORY) HCF //tried to open a directory and it wasn't
    if(S_ISDIR(location_mode) && !(open_flags & O_DIRECTORY)) HCF //tried to open a non directory and it was
    if(open_flags & O_NONBLOCK) HCF//do I need to do anything
    if(open_flags & O_TRUNC) {
        if(!S_ISREG(location_mode)) HCF//tried to open_clear a non-file
        printf("TODO clear file\n");
    }
    if(open_flags & O_APPEND) {
        HCF
    }

    return location;
}