#include "uapi/stdint.h"
#include "debugging.h"
#include "fs.h"
#include "kern_libc.h"
#include "fs_tar.h"

enum UstarFileTypeFlag {NORMAL = '0', HARD_LINK = '1', SYMBOLIC_LINK = '2', CHARACTER_DEVICE = '3', BLOCK_DEVICE = '4', DIRECTORY = '5', NAMED_FIFO_PIPE = '6'};

 
struct __attribute__((__packed__)) UstarMetadata {
    char filename[100];
    char file_mode[8];
    char owner_user_id[8];
    char group_user_id[8];
    /// @brief This field is a string of an octal number i.e "70" = 56
    char file_size_octal[12];
    char last_modification_octal[12];
    char header_record_checksum[8];
    char type_flag;//UstarFileTypeFlag
    char name_of_linked_file[100];
    char ustar_text[6];//should be set to "ustar\0"
    char ustar_version[2];
    char owner_username[32];
    char owner_group_name[32];
    char device_major_number[8];
    char device_minor_number[8];
    char filename_prefix[155];
    char pad[12];
};

/// @brief Takes an octal string and returns an integer
/// @param str the input octal string
/// @param size how many bytes to read from `str`
/// @return the octal number stored in `str`
static int oct2bin(char *str, int size) {
    int n = 0;
    char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

#define TARFS_MAX_FILES 100

struct TarFsDir {
    uint64_t num_children;
    uint64_t *children_inode_numbers;
};

struct TarFsFile {
    uint64_t file_size_bytes;
    uint8_t *file_bytes;
};

struct TarFsInode {
    /// This string must be heap allocated
    char *name;
    uint64_t inode_number;
    uint64_t parent_inode_number;
    /// Informs as to which field of the union to use
    enum VNodeType type;
    union {
        struct TarFsFile file_info;
        struct TarFsDir directory_info;
    };
}; 

static struct TarFsInode inode_lookup_table[TARFS_MAX_FILES];

void tarfs_debug_tree(struct TarFsInode root, uint64_t depth) {
    for(uint64_t i = 0; i < depth; i++) {debug_print("  ");}

    debug_print(root.name);
    debug_print(" (");
    debug_int(root.inode_number);
    debug_print(")");

    switch (root.type) {
    case VNODE_FILE:
        debug_print("\n");
        return;
    case VNODE_DIR:
        debug_print(":\n");
        debug_print(root.name);
        for(uint64_t i=0; i<root.directory_info.num_children; i++) {
            struct TarFsInode child = inode_lookup_table[root.directory_info.children_inode_numbers[i]];
            tarfs_debug_tree(child, depth+1);
        }
        break;
    }
}

static struct TarFsDir* get_directory_from_vnode(uint64_t directory_inode_num) {
    if(directory_inode_num >= TARFS_MAX_FILES) {HCF}
    struct TarFsInode *inode = &inode_lookup_table[directory_inode_num];
    if(inode->type != VNODE_DIR) {HCF}//should be searching in a directory
    return &inode->directory_info;
}
static struct TarFsFile* get_file_from_vnode(uint64_t file_inode_num) {
    if(file_inode_num >= TARFS_MAX_FILES) {HCF}
    struct TarFsInode *inode = &inode_lookup_table[file_inode_num];
    if(inode->type != VNODE_FILE) {HCF}//should be searching in a directory
    return &inode->file_info;
}

static uint64_t find_free_inode_number() {
    for(uint64_t curr = 0; curr < TARFS_MAX_FILES; curr++) {
        if(!inode_lookup_table[curr].name) {
            return curr;
        }
    }
    HCF
}

//forward declare the fs functions
int tarfs_directory_lookup(struct VNodeId directory, const char* name, struct VNode* out);
uint64_t tarfs_read_file(struct VNodeId file, uint64_t offset, uint8_t* out, uint64_t num_bytes);
int tarfs_create_inode(struct VNodeId parent, enum VNodeType new_inode_type, const char* name, struct VNode* out);

static struct VNode tarfs_generate_vnode(uint64_t inode_number, enum VNodeType inode_type) {
    return (struct VNode) {
        .id = {
            .inode_number = inode_number,
        },
        .inode_type = inode_type,
        .directory_lookup = tarfs_directory_lookup,
        .write_file = NULL,//can't do this
        .read_file = tarfs_read_file,
        .create_inode = tarfs_create_inode,//please don't call this from userspace
    };
}

int tarfs_directory_lookup(struct VNodeId directory, const char* name, struct VNode* out) {
    struct TarFsDir* dir = get_directory_from_vnode(directory.inode_number);
    for(uint64_t index = 0; index < dir->num_children; index++) {
        uint64_t inode_number = dir->children_inode_numbers[index];
        if(inode_number > TARFS_MAX_FILES) {HCF}
        struct TarFsInode inode = inode_lookup_table[inode_number];
        if(strcmp(name, inode.name) == 0) {
            *out = tarfs_generate_vnode(inode_number, inode.type);
            return 0;
        }
    }
    return -1;
}
uint64_t tarfs_read_file(struct VNodeId file, uint64_t offset, uint8_t* out, uint64_t num_bytes) {
    const struct TarFsFile* file_ptr = get_file_from_vnode(file.inode_number);
    //return early if offset is bad
    if(offset >= file_ptr->file_size_bytes) {return 0;}
    //cap at the end of the file
    if(offset + num_bytes > file_ptr->file_size_bytes) {num_bytes = file_ptr->file_size_bytes - offset;}

    memcpy(out, file_ptr->file_bytes + offset, num_bytes);
    return num_bytes;
}
int tarfs_create_inode(struct VNodeId parent, enum VNodeType new_inode_type, const char* name, struct VNode* out) {
    struct TarFsDir* parent_dir = get_directory_from_vnode(parent.inode_number);

    uint64_t new_parent_children_count = parent_dir->num_children + 1;
    uint64_t* new_parent_array = kmalloc(8 * new_parent_children_count);
    if(parent_dir->children_inode_numbers != NULL) {
        //copy old children pointers
        memcpy(new_parent_array, parent_dir->children_inode_numbers, 8 * parent_dir->num_children);
    }

    char* cloned_name = kmalloc(strlen(name)+1);
    strcpy(cloned_name, name);

    uint64_t inode_number = find_free_inode_number();

    new_parent_array[parent_dir->num_children] = inode_number;
    //add the new inode to the lookup table
    inode_lookup_table[inode_number] = (struct TarFsInode) {
        .name=cloned_name,
        .inode_number=inode_number,
        .parent_inode_number=parent.inode_number,
        .type=new_inode_type
    };
    *out = tarfs_generate_vnode(inode_number, new_inode_type);

    //update parent data
    parent_dir->children_inode_numbers = new_parent_array;
    parent_dir->num_children++;

    return 0;
}

static void create_path_and_file(char* path, void* data_location, uint64_t data_size) {
    char* path_copy = kmalloc(strlen(path) + 1);
    strcpy(path_copy, path);

    char* curr_segment = NULL;
    struct VNode curr_inode = tarfs_generate_vnode(0, VNODE_DIR);//0 since that is where the first inode is
    char* next_segment = path_copy;

    while(1) {
        //set the current segment
        curr_segment = next_segment;
        //find the next segment
        while(*next_segment != '/') {
            if(*next_segment == '\0') {
                next_segment = NULL;
                struct VNode result;
                tarfs_create_inode(curr_inode.id, VNODE_FILE, curr_segment, &result);
                inode_lookup_table[result.id.inode_number].file_info = (struct TarFsFile) {.file_bytes = data_location, .file_size_bytes=data_size};
                kfree(path_copy);
                return;//found end
            }
            next_segment++;
        }

        *next_segment++ = '\0';//mark end of curr, and step to start of next segment

        struct VNode result;
        if(tarfs_directory_lookup(curr_inode.id, curr_segment, &result) == -1) {
            //folder not found, create
            tarfs_create_inode(curr_inode.id, VNODE_DIR, curr_segment, &result);
        }

        curr_inode = result;//walk to the child that was created
    }

    HCF
}

void tarfs_init(void* archive) {

    //generate something to fill the name attribute
    char* name_placeholder = kmalloc(1);
    *name_placeholder = 0;

    inode_lookup_table[0] = (struct TarFsInode) {
        .name=name_placeholder,
        .inode_number=0,
        .parent_inode_number = UINT64_MAX,
        .type = VNODE_DIR,
        .directory_info = (struct TarFsDir) {
            .num_children=0,
            .children_inode_numbers = NULL,
        },
    };

    if(sizeof(struct UstarMetadata) != 512) {
        //something is broken...
        HCF
    }
    struct UstarMetadata *metadata = (struct UstarMetadata*)archive;

    while (!memcmp(metadata->ustar_text, "ustar", 5)) {//ensure I have a valid ustar metadata
        int filesize = oct2bin(metadata->file_size_octal, 11);//read octal text to integer

        if(metadata->type_flag == NORMAL) {
            create_path_and_file(metadata->filename, (void*)metadata + 512, filesize);//add the relevant inodes
        }

        uint64_t filesize_blocks = (filesize + 511) / 512; // data blocks
        metadata++;//skip metadata
        metadata += filesize_blocks;
    }
    
    vfs_add_mount("tarfs", tarfs_generate_vnode(0, VNODE_DIR));
}
