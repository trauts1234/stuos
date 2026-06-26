#ifndef FS_H
#define FS_H

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

    /// This function should check that `directory` is a directory node, then try to find a node with name `name`
    ///
    /// Returns:
    /// 0 on success
    /// -1 if `name` is not found
    int (*directory_lookup)(struct VNodeData dir_inode_num, const char* name, struct VNode* out);

    /// This function should check that file is a valid file, then try to write `byte` to index `offset`
    ///
    /// Returns:
    /// number of bytes written
    uint64_t (*write_file)(struct VNodeData inode_num, uint64_t offset, const uint8_t* input_buf, uint64_t num_bytes);

    /// This function should check that file is a valid file, then try to read `num_bytes` bytes into `out` at index in the file `offset`
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