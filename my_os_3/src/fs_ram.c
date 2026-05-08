#include "fs_ram.h"
#include "kern_libc.h"
#include "debugging.h"
#include "fs.h"
#include "uapi/stdint.h"

#define RAMFS_MAX_FILES 100

struct RamFsInode;
//TODO should this be a list of pointers, or just a list
static struct RamFsInode* inode_lookup_table[RAMFS_MAX_FILES];

struct RamFsDir {
    uint64_t num_children;
    uint64_t *children_inode_numbers;
};

struct RamFsFile {
    uint64_t file_size_bytes;
    uint8_t *file_bytes;
};

struct RamFsInode {
    /// This string must be heap allocated
    char *name;
    /// Informs as to which field of the union to use
    mode_t type;
    union {
        struct RamFsFile file_info;
        struct RamFsDir directory_info;
    };
}; 

//declare functions so that I can use them when generating vnodes
int ramfs_directory_lookup(struct VNodeData directory, const char* name, struct VNode* out);
uint64_t ramfs_write_file(struct VNodeData file, uint64_t offset, const uint8_t* in, uint64_t num_bytes);
uint64_t ramfs_read_file(struct VNodeData file, uint64_t offset, uint8_t* out, uint64_t num_bytes);
int ramfs_create_inode(struct VNodeData parent, mode_t new_inode_type, const char* name, struct VNode* out);
struct stat ramfs_stat(struct VNodeData file);

static struct VNode ramfs_generate_vnode(uint64_t inode_number, uint64_t parent_inode) {
    return (struct VNode) {
        .id = {
            .self_inode = inode_number,
            .parent_inode = parent_inode,
            .mount_id = 0,
        },
        .directory_lookup=ramfs_directory_lookup,
        .write_file=ramfs_write_file,
        .read_file=ramfs_read_file,
        .create_inode=ramfs_create_inode,
        .stat_file = ramfs_stat,
    };
}
static uint64_t find_free_inode_number() {
    for(uint64_t curr = 0; curr < RAMFS_MAX_FILES; curr++) {
        if(!inode_lookup_table[curr]) {
            return curr;
        }
    }
    HCF
}

static struct RamFsDir* get_directory_from_vnode(uint64_t directory) {
    if(directory >= RAMFS_MAX_FILES) {HCF}
    struct RamFsInode *inode = inode_lookup_table[directory];
    if(!S_ISDIR(inode->type)) {HCF}//should be a directory
    return &inode->directory_info;
}
static struct RamFsFile* get_file_from_vnode(uint64_t file) {
    if(file >= RAMFS_MAX_FILES) {HCF}
    struct RamFsInode *inode = inode_lookup_table[file];
    if(!S_ISREG(inode->type)) {HCF}//should be a file
    return &inode->file_info;
}

void ramfs_init() {
    //generate something to fill the name attribute
    char* name_placeholder = kmalloc(1);
    *name_placeholder = 0;
    // generate the root inode
    struct RamFsInode *new_inode = kmalloc(sizeof(struct RamFsInode));
    *new_inode = (struct RamFsInode) {
        .name=name_placeholder,
        .type = S_IFDIR,
        .directory_info = (struct RamFsDir) {
            .num_children=0,
            .children_inode_numbers = NULL,
        },
    };

    // store inode in the lookup table
    inode_lookup_table[0] = new_inode;

    vfs_add_mount("ramfs", ramfs_generate_vnode(0, UINT64_MAX));
}

struct stat ramfs_stat(struct VNodeData file) {
    struct RamFsInode *ino = inode_lookup_table[file.self_inode];

    return (struct stat) {
        .st_ino = file.self_inode,
        .st_mode = ino->type,
        .st_uid = 0,//root
        .st_gid = 0,
        .st_size = S_ISREG(ino->type) ? ino->directory_info.num_children*sizeof(uint64_t) : ino->file_info.file_size_bytes,
    };
}

int ramfs_directory_lookup(struct VNodeData directory, const char* name, struct VNode* out){
    const struct RamFsDir* directory_inode = get_directory_from_vnode(directory.self_inode);

    for(uint64_t child_num = 0; child_num < directory_inode->num_children; child_num++) {
        uint64_t curr_child_inode_number = directory_inode->children_inode_numbers[child_num];
        struct RamFsInode* curr_child = inode_lookup_table[curr_child_inode_number];

        //compare filenames
        if (strcmp(curr_child->name, name)) {
            continue;
        } else {
            *out = ramfs_generate_vnode(curr_child_inode_number, directory.self_inode);
            return 0;
        }
    }
    return -1;
}

uint64_t ramfs_write_file(struct VNodeData file, uint64_t offset, const uint8_t* in, uint64_t num_bytes) {
    struct RamFsFile* file_ptr = get_file_from_vnode(file.self_inode);

    if(offset + num_bytes > file_ptr->file_size_bytes) {
        //writing past the end of the file, so reallocate
        uint64_t new_file_size = offset + num_bytes;
        void* new_file = kmalloc(new_file_size);

        memcpy(new_file, file_ptr->file_bytes, file_ptr->file_size_bytes);//copy the original bytes
        if(offset > file_ptr->file_size_bytes) {memset(new_file + file_ptr->file_size_bytes, 0, offset - file_ptr->file_size_bytes);}//fill the gap between the end of the old file and the start of the data with zeroes
        memcpy(new_file + offset, in, num_bytes);//copy the new bytes

        //store the allocation
        kfree(file_ptr->file_bytes);
        file_ptr->file_bytes = new_file;
        file_ptr->file_size_bytes = new_file_size;
    } else {
        //write inside file, just copy the bytes
        memcpy(file_ptr->file_bytes + offset, in, num_bytes);
    }

    return num_bytes;
}

uint64_t ramfs_read_file(struct VNodeData file, uint64_t offset, uint8_t* out, uint64_t num_bytes) {
    const struct RamFsFile* file_ptr = get_file_from_vnode(file.self_inode);
    //return early if offset is bad
    if(offset >= file_ptr->file_size_bytes) {return 0;}
    //cap at the end of the file
    if(offset + num_bytes > file_ptr->file_size_bytes) {num_bytes = file_ptr->file_size_bytes - offset;}

    memcpy(out, file_ptr->file_bytes + offset, num_bytes);
    return num_bytes;
}

int ramfs_create_inode(struct VNodeData parent, mode_t new_inode_type, const char* name, struct VNode* out) {
    struct RamFsDir* parent_dir = get_directory_from_vnode(parent.self_inode);

    uint64_t new_parent_children_count = parent_dir->num_children + 1;
    uint64_t* new_parent_array = kmalloc(8 * new_parent_children_count);
    if(parent_dir->children_inode_numbers != NULL) {
        //copy old children pointers
        memcpy(new_parent_array, parent_dir->children_inode_numbers, 8 * parent_dir->num_children);
    }

    char* cloned_name = kmalloc(strlen(name)+1);
    strcpy(cloned_name, name);

    uint64_t inode_number = find_free_inode_number();

    struct RamFsInode *new_inode = kmalloc(sizeof(struct RamFsInode));
    *new_inode = (struct RamFsInode) {
        .name=cloned_name,
        .type=new_inode_type,
    };
    new_parent_array[parent_dir->num_children] = inode_number;
    //add the new inode to the lookup table
    inode_lookup_table[inode_number] = new_inode;
    *out = ramfs_generate_vnode(inode_number, parent.self_inode);

    //update parent data
    parent_dir->children_inode_numbers = new_parent_array;
    parent_dir->num_children++;

    return 0;
}