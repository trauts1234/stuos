#ifndef FS_H
#define FS_H

#include "uapi/stdint.h"

enum VNodeType { VNODE_FILE, VNODE_DIR};

/// Represents an abstract node in the filesystem, which can be a file or directory
struct VNode {
    /// Unique ID of a node in the filesystem - ID -1 means NULL
    uint64_t inode_number;
    /// Whether the inode is a file or directory
    enum VNodeType inode_type;

    /// This function should check that `directory` is a directory node, then try to find a node with name `name`
    ///
    /// Returns:
    /// 0 on success
    /// -1 if `name` is not found
    int (*directory_lookup)(uint64_t dir_inode_num, const char* name, struct VNode* out);

    /// This function should check that file is a valid file, then try to write `byte` to index `offset`
    ///
    /// Returns:
    /// number of bytes written
    uint64_t (*write_file)(uint64_t inode_num, uint64_t offset, const uint8_t* input_buf, uint64_t num_bytes);

    /// This function should check that file is a valid file, then try to read `num_bytes` bytes into `out` at index in the file `offset`
    ///
    /// Returns:
    /// number of bytes read
    uint64_t (*read_file)(uint64_t inode_num, uint64_t offset, uint8_t* output_buf, uint64_t num_bytes);

    /// This function should check that `parent` is a directory node, then create a new inode of type `new_inode_type`, returning the VNode that was created into out.
    /// parent.inode_number == 0 means that there is no parent, and the inode that is created is the root node
    /// `name` is cloned
    ///
    /// Returns:
    /// 0 on success
    int (*create_inode)(uint64_t parent_inode_num, enum VNodeType new_inode_type, const char* name, struct VNode* out);
};

struct VNode make_null_vnode();

/// mount_name's data must live forever, as the pointer is copied, as must filesystem_root
void vfs_add_mount(const char* mount_name, struct VNode filesystem_root);

//gets the VNode at `path`
struct VNode vfs_get(const char* cwd_path, const char* path, int open_flags);

#endif