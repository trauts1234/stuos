#include "fs.h"
#include "debugging.h"
#include "kern_libc.h"
#include "uapi/stdint.h"
#include "uapi/fcntl.h"

#define ROOT_INODE_NUM 69
#define MAX_MOUNTS 10

/// You can only mount against the root directory i.e /mountpoint is OK, but /a/b/mountpoint is not OK
struct MountPoint {
    /// the filesystem will be mounted at /{mount_name}
    ///
    /// NULL pointer represents a NULL mountpoint
    const char* mount_name;
    /// Root of the filesystem that is mounted, represents /{mount_name}/
    ///
    /// Must be a directory, and includes function pointers for how to use the vnode
    struct VNode mount_root;
} filesystem_mount_points[MAX_MOUNTS] = {};

static uint64_t path_segment_len(const char* path) {
    uint64_t size = 0;
    while(*path != '\0' && *path != '/') {
        size++;
        path++;
    }
    return size;
}

//scan for mount points that match a directory name
static int vfs_root_dir_lookup(uint64_t dir_inode_num, const char* name, struct VNode* out) {
    if(dir_inode_num != ROOT_INODE_NUM) HCF

    for (uint64_t mount_idx = 0; mount_idx < MAX_MOUNTS && filesystem_mount_points[mount_idx].mount_name;mount_idx++) {
        if(strcmp(name, filesystem_mount_points[mount_idx].mount_name) == 0) {
            *out = filesystem_mount_points[mount_idx].mount_root;//start at the filesystem's root
            return 0;
        }
    }

    return -1;
}
//fail to do the following as I can only handle mount points
static uint64_t fail_write_file(uint64_t, uint64_t, const uint8_t*, uint64_t) { HCF }
static uint64_t fail_read_file(uint64_t, uint64_t, uint8_t*, uint64_t) { HCF }
int fail_create_inode(uint64_t, enum VNodeType, const char*, struct VNode*) { HCF }

struct VNode vfs_get_root() {
    return (struct VNode) {
        .inode_number = ROOT_INODE_NUM,
        .inode_type = VNODE_DIR,
        .directory_lookup = vfs_root_dir_lookup,
        .write_file = fail_write_file,
        .read_file = fail_read_file,
        .create_inode = fail_create_inode,
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

static enum StepPathResult step_path2(struct VNode* current, const char** path_start) {
    //if starts with root, return root node
    if(**path_start == '/') {
        (*path_start)++;
        *current = vfs_get_root();
        return STEPPATH_INPROGRESS;
    }

    //path is invalid or empty, so return
    if(*path_start == NULL || **path_start == '\0') {
        *path_start = NULL;
        return STEPPATH_COMPLETE;
    }

    uint64_t segment_len = path_segment_len(*path_start);
    char* segment_cpy = kmalloc(segment_len + 1);
    memcpy(segment_cpy, *path_start, segment_len);
    segment_cpy[segment_len] = '\0';

    struct VNode result;
    int status = current->directory_lookup(current->inode_number, segment_cpy, &result);
    kfree(segment_cpy);

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

    if(must_be_folder && result.inode_type != VNODE_DIR) HCF

    *current = result;

    if(was_last_segment) {
        *path_start = NULL;
        return STEPPATH_COMPLETE;
    } else {
        *path_start += segment_len + 1;
        return STEPPATH_INPROGRESS;
    }
}

void vfs_add_mount(const char* mount_name, struct VNode filesystem_root) {
    uint64_t mount_idx = 0;
    while (filesystem_mount_points[mount_idx].mount_name) {
        mount_idx++;
        if(mount_idx == MAX_MOUNTS) {HCF}/// out of mount points
    }

    filesystem_mount_points[mount_idx] = (struct MountPoint) {.mount_name = mount_name, .mount_root=filesystem_root};
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
    while(path) {
        switch (step_path2(&location, &path)) {

        case STEPPATH_COMPLETE:
            break;// path is null and loop will finish
        case STEPPATH_INPROGRESS:
            break;// path is not null and will continue
        case STEPPATH_NOTEXIST_LAST:
            if(open_flags & O_CREAT) {
                if (open_flags & O_DIRECTORY) HCF
                //since not a dir, the last bit of the `path` is a valid string that I can pass
                location.create_inode(location.inode_number, VNODE_FILE, path, &location);
                path = NULL;//to quit loop
                break;
            }
            /* FALLTHROUGH, since I have doesn't exist and no OPEN_CREATE */
            [[ fallthrough ]];
        case STEPPATH_NOTEXIST:
            kprintf("failed to parse remaining part of path: %s", path);
            HCF
        default:
            HCF
        }
    }

    if(location.inode_type != VNODE_DIR && open_flags & O_DIRECTORY) HCF //tried to open a directory and it wasn't
    if(open_flags & O_TRUNC) {
        if(location.inode_type != VNODE_FILE) HCF//tried to open_clear a non-file
        debug_print("TODO clear file\n");
    }
    if(open_flags & O_APPEND) {
        HCF
    }

    return location;
}

struct VNode make_null_vnode() {
    return (struct VNode) {
        .inode_number = (uint64_t)-1
    };
}