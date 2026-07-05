#ifndef FS_H
#define FS_H

#include <uapi/dirent.h>
#include <uapi/stdint.h>
#include <uapi/types.h>
#include <uapi/stat.h>

/// Represents an abstract node in the filesystem, which can be a file or directory
struct VNode {
    struct VNodeData {
        /// Unique ID per file under a mount
        uint64_t inode;
        // Unique per filesystem, and can be used to differentiate between inode 69 in one mount versus inode 69 in another mount
        uint64_t mount_id;
    } id;

    struct stat (*stat_file)(struct VNodeData id);

    /// This function reads directory entries, and fills dirent_buf and vnode_buf accordingly
    ///
    /// dirent_buf and/or vnode_buf can be NULL
    ///
    /// Returns the number of items put into dirent_buf and vnode_buf
    uint64_t (*read_dirents)(struct VNodeData inode_num, uint64_t dirent_index, struct dirent* dirent_buf, struct VNode* vnode_buf, uint64_t dirent_count);

    /// This function should check that file is a valid file, then try to write data from `input_buf` to index `offset`
    ///
    /// Returns:
    /// number of bytes written
    uint64_t (*write_file)(struct VNodeData inode_num, uint64_t offset, const uint8_t* input_buf, uint64_t num_bytes);

    /// This function should check that file is a valid file, then try to read `num_bytes` bytes into `out` at index in the file `offset`
    ///
    /// This operation is also valid for open directories, where the directory pretends to be a list of struct dirent. may crash if offset or num_bytes are not a multiple of sizeof(struct dirent)
    ///
    /// Returns:
    /// number of bytes read
    uint64_t (*read_file)(struct VNodeData inode_num, uint64_t offset, uint8_t* output_buf, uint64_t num_bytes);

    /// This function should check that `parent` is a directory node, then create a new inode of type `new_inode_type`, returning the VNode that was created into out.
    /// parent.inode_number == 0 means that there is no parent, and the inode that is created is the root node
    /// `name` is cloned
    ///
    /// Returns:
    /// 0 on success
    int (*create_inode)(struct VNodeData parent_inode_num, mode_t new_inode_type, const char* name, struct VNode* out);
};

/// mounts filesystem_root on top of mount_against
void vfs_add_mount(struct VNode mount_against, struct VNode filesystem_root);

//gets the VNode at `path`
struct VNode vfs_get(const char* cwd_path, const char* path, int open_flags);

#endif