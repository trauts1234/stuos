#include "debugging.h"
#include "fs.h"
#include "uapi/stat.h"
#include "uapi/stdint.h"
#include "kern_libc.h"
#include "uapi/types.h"

struct __attribute__((packed)) Fat16Header {
    //The first three bytes EB 3C 90 disassemble to JMP SHORT 3C NOP. (The 3C value may be different.)
    // The reason for this is to jump over the disk format information (the BPB and EBPB).
    // Since the first sector of the disk is loaded into ram at location 0x0000:0x7c00 and executed, without this jump, the processor would attempt to execute data that isn't code.
    //ignore
    uint8_t jmp_short_nop[3];
    //padded with spaces, not null terminated
    //ignore
    char oem_identifier[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    //the first n sectors are reserved (including the one that contains the BPB)
    uint16_t reserved_sectors;
    //should be 2 - two copies of the same table
    uint8_t file_allocation_tables_count;
    //number of entries for the root directory
    uint16_t number_of_root_directory_entries;
    //total sectors in the logical volume
    //if number of sectors > UINT16_MAX, set to 0 and place in large_sector_count
    uint16_t number_of_sectors;
    //bitfield - ignore this
    uint8_t media_descriptor_type;
    //fat12/16 only - number of sectors for the file allocation table
    uint16_t number_of_sectors_per_fat;
    //ignore
    uint16_t sectors_per_track;
    //ignore
    uint16_t number_of_heads;
    // should be 0?
    // This field is used by boot code in the remainder of the boot block when it needs to call the machine firmware in order to perform I/O from and to the boot volume.
    // The value in this field is added to the volume-relative block number to yield the disc-relative block number.
    uint32_t number_of_hidden_sectors;
    uint32_t large_sector_count;

    //extended boot partition:

    //ignore
    uint8_t drive_number;
    //reserved
    uint8_t windows_nt_flags;
    //must be 0x28 or 0x29
    uint8_t signature;
    //ignore
    uint32_t volume_serial_number;
    //padded with spaces - no null terminator
    char volume_label[11];
    //ignore
    char system_identifier[8];
    uint8_t boot_code[448];
    //must be 0xAA55
    uint16_t bootable_partition_signature;
};

enum {
    FILE_ATTRIBUTES_RO = 0x1, FILE_ATTRIBUTES_HIDDEN = 0x2,
    FILE_ATTRIBUTES_SYSTEM = 0x4, FILE_ATTRIBUTES_VOLUMEID = 0x8,
    FILE_ATTRIBUTES_DIRECTORY = 0x10,
    //ignore this flag
    FILE_ATTRIBUTES_ARCHIVE = 0x20,
    //long file name
    FILE_ATTRIBUTES_LFN = FILE_ATTRIBUTES_RO | FILE_ATTRIBUTES_HIDDEN | FILE_ATTRIBUTES_SYSTEM | FILE_ATTRIBUTES_VOLUMEID
};
//TODO are the bits reversed?
struct __attribute__((packed)) FatTime {
    uint8_t hour:5;
    uint8_t minute:6;
    //double this
    uint8_t seconds:5;
};
//TODO are the bits reversed?
struct __attribute__((packed)) FatDate {
    uint8_t year: 7;
    uint8_t month: 4;
    uint8_t day: 5;
};

struct __attribute__((packed)) Fat16DirectoryEntry {
    //8.3 file name. The first 8 characters are the name and the last 3 are the extension.
    char filename[8];
    char extension[3];
    //see FILE_ATTRIBUTES_XYZ
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creation_time_hundredths;
    struct FatTime creation_time;
    struct FatDate creation_date;
    struct FatDate access_date;
    //for fat32, the high 16 bits of the cluster number
    uint16_t zero;
    struct FatTime modification_time;
    struct FatDate modification_date;
    uint16_t cluster_number;
    uint32_t file_size_bytes;
};

struct __attribute__((packed)) Fat16LFNEntry {
    //one based index (there are sequence_number-1 LFN entries after me)
    uint8_t sequence_number:5;

    uint8_t is_last_in_sequence: 1;

    //first five chars
    uint16_t unicode_name_1[5];
    //must be FILE_ATTRIBUTES_LFN
    uint8_t attributes;
    //should be 0?
    uint8_t zero_a;
    uint8_t checksum;
    //the next six chars
    uint16_t unicode_name_2[6];
    uint16_t zero_b;
    uint16_t unicode_name_3[2];
};

//use a reserved cluster number for the root directory
#define ROOT_DIR_CLUSTER_NUMBER 0
union InodeNumberData {
    uint64_t data;
    struct {
        //can also be ROOT_DIR_CLUSTER_NUMBER
        uint16_t cluster_number;
        //only valid if I am the parent inode of an inode, representing the directory entry number that represents the child
        //
        //reserved otherwise
        uint16_t parent_directory_entry_number;
        uint32_t reserved;
    };
};

#define MAX_FAT_MOUNTS 10
struct Fat16Volume {
    //Which underlying block device am I mounting
    // because: I need to know where to write and read data from
    struct VNode block_device;
    //the sector number of the first file allocation table
    // because: I need to read the FAT, or skip past it to find the root directory
    uint16_t fats_start_sector;
    //how many contiguous file allocation tables there are
    // because: I need to duplicate writes to all tables, or skip past all the tables to find the root directory
    uint8_t number_of_fats;
    //number of sectors per file allocation table
    // because: I need to know how big the FAT is
    uint16_t number_of_sectors_per_fat;
    //because: I read from block_device at a byte offset
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t number_of_sectors_root_directory;
} all_fat_mounts[MAX_FAT_MOUNTS] = {};

//finds the byte offset of the start of the data section
static uint64_t calculate_data_section_start(struct Fat16Volume *vol) {
    uint16_t sector = vol->fats_start_sector + vol->number_of_fats*vol->number_of_sectors_per_fat + vol->number_of_sectors_root_directory;
    return sector * vol->bytes_per_sector;
}

// returns true if n isn't pointing to a real cluster (either free sentinel or NULL sentinel)
bool is_invalid_cluster_num(uint16_t n){
    return n < 2 || n >= 0xFFF7;
}
bool is_free_cluster_num(uint16_t n) {
    return n < 2;
}
// if a FAT entry contains n, where is_bad_cluster_num(n), then the entry is corrupted and mustn't be used
bool is_bad_cluster_num(uint16_t n) {
    return n == 0xFFF7;
}

//Read a file allocation table entry
//
//The returned value is the cluster number of the next item (<2 for FREE, >=0xFFF8 for end of linked list)
static uint16_t read_fat(struct Fat16Volume *vol, uint16_t cluster_number) {
    if(is_invalid_cluster_num(cluster_number)) HCF
    uint16_t fat_data = 0;
    vol->block_device.read_file(vol->block_device.id, vol->fats_start_sector*vol->bytes_per_sector + 2*cluster_number, (uint8_t*)&fat_data, 2);
    return fat_data;
}
//Write a file allocation table entry
static void write_fat(struct Fat16Volume *vol, uint16_t cluster_number, uint16_t value) {
    if(is_invalid_cluster_num(cluster_number)) HCF
    vol->block_device.write_file(vol->block_device.id, vol->fats_start_sector*vol->bytes_per_sector + 2*cluster_number, (uint8_t*)&value, 2);
}

static uint16_t find_free_cluster_number(struct Fat16Volume *vol) {
    uint16_t num_clusters = vol->number_of_sectors_per_fat*vol->bytes_per_sector / 2;
    for(uint16_t i=0; i < num_clusters; i++) {
        if(is_free_cluster_num(read_fat(vol, i))) {
            return i;
        }
    }
    HCF//out of clusters
}

//write to a file or directory
// static void raw_write(struct Fat16Volume *vol, uint16_t cluster_number, uint64_t requested_offset, void* input_buf, uint64_t num_bytes) {
//     //TODO
// }


// offset and num_bytes must be sanitised first, as this will crash or cause UB on reading past the end of file
//
// - if cluster number = ROOT_DIR_CLUSTER_NUMBER, will read from the root directory entry
// - if cluster number is invalid, will crash
static void read_from_cluster_chain(struct Fat16Volume *vol, uint16_t cluster_number, uint64_t requested_offset, void* output_buf, uint64_t num_bytes) {
    const uint64_t bytes_per_cluster = vol->bytes_per_sector * vol->sectors_per_cluster;
    const uint64_t required_cluster_number = requested_offset / bytes_per_cluster;
    const uint64_t required_offset_in_first_cluster = requested_offset % bytes_per_cluster;
    const uint64_t required_number_of_clusters = (num_bytes + bytes_per_cluster-1) / bytes_per_cluster;

    if(cluster_number == ROOT_DIR_CLUSTER_NUMBER) {
        //offset should be n directory entries in
        if(requested_offset % sizeof(struct Fat16DirectoryEntry)) HCF
        //num bytes should be n directory entries
        if(num_bytes % sizeof(struct Fat16DirectoryEntry)) HCF

        uint64_t root_dir_start_byte = (vol->fats_start_sector + vol->number_of_sectors_per_fat*vol->number_of_fats) * vol->bytes_per_sector;
        //read must be in range
        if(requested_offset + num_bytes >= vol->number_of_sectors_root_directory*vol->bytes_per_sector) HCF

        vol->block_device.read_file(vol->block_device.id, root_dir_start_byte + requested_offset, output_buf, num_bytes);
        return;
    }

    //skip the required number of clusters
    for(uint64_t i=0; i<required_cluster_number + required_number_of_clusters; i++) {
        //if I have skipped enough clusters, start reading
        if(i >= required_cluster_number) {
            uint64_t src_addr = calculate_data_section_start(vol) + bytes_per_cluster * (cluster_number-2);//-2 as the first two fat entries are reserved
            uint64_t available_bytes_in_cluster = bytes_per_cluster;

            if(i == required_cluster_number) {
                //first cluster - read at an offset
                src_addr += required_offset_in_first_cluster;
                available_bytes_in_cluster -= required_offset_in_first_cluster;
            }

            uint64_t num_bytes_to_read = num_bytes;
            if(num_bytes_to_read > available_bytes_in_cluster) num_bytes_to_read = available_bytes_in_cluster;

            vol->block_device.read_file(vol->block_device.id, src_addr, output_buf, num_bytes_to_read);
            output_buf += num_bytes_to_read;
            num_bytes -= num_bytes_to_read;
        }
        
        //skip to the next entry
        cluster_number = read_fat(vol, cluster_number);
    }
}

static struct stat stat_file(struct VNodeData inode_num) {
    struct Fat16Volume vol = all_fat_mounts[inode_num.mount_id];

    union InodeNumberData inode = {.data = inode_num.self_inode};
    if(inode.reserved) HCF

    //special case for root directory
    if(inode.cluster_number == ROOT_DIR_CLUSTER_NUMBER) return (struct stat) {
        .st_ino = inode_num.self_inode,
        .st_mode = S_IFDIR,
        .st_uid = 0,
        .st_gid = 0,
        .st_size = vol.number_of_sectors_root_directory * vol.bytes_per_sector
    };

    // check file size
    union InodeNumberData parent_inode = {.data = inode_num.parent_inode};
    if(parent_inode.reserved) HCF
    
    struct Fat16DirectoryEntry entry_in_parent;
    read_from_cluster_chain(&vol, parent_inode.cluster_number, parent_inode.parent_directory_entry_number*sizeof(struct Fat16DirectoryEntry), &entry_in_parent, sizeof(struct Fat16DirectoryEntry));
    
    mode_t mode;
    switch (entry_in_parent.attributes & ~FILE_ATTRIBUTES_ARCHIVE) {
        case FILE_ATTRIBUTES_DIRECTORY:
        mode = S_IFDIR;break;
        case 0:
        mode = S_IFREG;break;
        default:
        HCF
    }
    
    return (struct stat) {
        .st_ino = inode_num.self_inode,
        .st_mode = entry_in_parent.attributes,
        .st_uid = 0,
        .st_gid = 0,
        .st_size = entry_in_parent.file_size_bytes,
    };
}

static uint64_t read_file(struct VNodeData inode_num, uint64_t offset, uint8_t* output_buf, uint64_t num_bytes) {
    struct Fat16Volume vol = all_fat_mounts[inode_num.mount_id];

    union InodeNumberData inode = {.data = inode_num.self_inode};
    if(inode.reserved) HCF

    if(inode.cluster_number != ROOT_DIR_CLUSTER_NUMBER) {
        //only check file size if not using the root directory, as that is a fixed size and doesn't have a parent
        union InodeNumberData parent_inode = {.data = inode_num.parent_inode};
        if(parent_inode.reserved) HCF
        
        struct Fat16DirectoryEntry entry_in_parent;
        read_from_cluster_chain(&vol, parent_inode.cluster_number, parent_inode.parent_directory_entry_number*sizeof(struct Fat16DirectoryEntry), &entry_in_parent, sizeof(struct Fat16DirectoryEntry));
        
        //don't read past the end of the file
        if(offset + num_bytes > entry_in_parent.file_size_bytes) num_bytes = entry_in_parent.file_size_bytes - offset;
    }

    //read the data
    read_from_cluster_chain(&vol, inode.cluster_number, offset, output_buf, num_bytes);
    return num_bytes;
}

static uint64_t write_file(struct VNodeData inode_num, uint64_t offset, const uint8_t *input_buf, uint64_t num_bytes) {
    // struct Fat16Volume vol = all_fat_mounts[inode_num.mount_id];

    // union InodeNumberData inode = {.data = inode_num.self_inode};
    // if(inode.reserved) HCF

    // if(S_ISDIR(stat_file(inode_num).st_mode)) {
    //     //directories and root dir aren't writeable
    //     HCF
    // }

    // union InodeNumberData parent_inode = {.data = inode_num.parent_inode};
    // if(parent_inode.reserved) HCF
    
    // struct Fat16DirectoryEntry entry_in_parent;
    // read_from_cluster_chain(&vol, parent_inode.cluster_number, parent_inode.parent_directory_entry_number*sizeof(struct Fat16DirectoryEntry), &entry_in_parent, sizeof(struct Fat16DirectoryEntry));
    
    // //read the data
    // read_from_cluster_chain(&vol, inode.cluster_number, offset, input_buf, num_bytes);
    // return num_bytes;
    HCF
}

static int create_inode(struct VNodeData parent_inode_num, mode_t new_inode_type, const char *name, struct VNode *out) {
    //TODO
    HCF
}

static int directory_lookup(struct VNodeData directory_entry, const char* name, struct VNode* out) {
    struct Fat16Volume vol = all_fat_mounts[directory_entry.mount_id];

    union InodeNumberData inode = {.data = directory_entry.self_inode};
    if(inode.reserved) HCF
    // union InodeNumberData parent_inode = {.data = directory_entry.parent_inode};
    // if(parent_inode.reserved) HCF

    //microsoft spec limits file names to 255 chars
    char long_file_name[256] = {};
    char* long_file_name_next_free = long_file_name;
    
    for(uint64_t directory_i=0;;directory_i++) {
        struct Fat16DirectoryEntry ent;
        read_from_cluster_chain(&vol, inode.cluster_number, directory_i*sizeof(struct Fat16DirectoryEntry), &ent, sizeof(struct Fat16DirectoryEntry));

        if(ent.attributes & 0b11000000) HCF
        
        //reached end of dir
        if(ent.filename[0] == 0) return -1;
        //empty entry
        if(ent.filename[0] == (char)0xE5) continue;
        
        if(ent.attributes == FILE_ATTRIBUTES_LFN) {
            //long file name
            struct Fat16LFNEntry lfn = *(struct Fat16LFNEntry*)&ent;
            if(lfn.attributes != FILE_ATTRIBUTES_LFN) HCF
            if(lfn.zero_a || lfn.zero_b) HCF

            bool done = false;
            
            for(int i=0; i<5 && !done; i++) {
                uint16_t c = lfn.unicode_name_1[i];
                if(c & 0xFF00) HCF//unicode... weird.

                *long_file_name_next_free++ = (char)c;
                if(c == 0) done = true;
            }
            for(int i=0; i<6 && !done; i++) {
                uint16_t c = lfn.unicode_name_2[i];
                if(c & 0xFF00) HCF//unicode... weird.

                *long_file_name_next_free++ = (char)c;
                if(c == 0) done = true;
            }
            for(int i=0; i<2 && !done; i++) {
                uint16_t c = lfn.unicode_name_3[i];
                if(c & 0xFF00) HCF//unicode... weird.

                *long_file_name_next_free++ = (char)c;
                if(c == 0) done = true;
            }

            if(!done) HCF
        } else {
            if(ent.cluster_number < 2) HCF
            if(ent.zero) HCF

            char short_file_name[12] = {0};
            char* short_file_name_next_free = short_file_name;
            //copy file name
            for(uint64_t i=0; i<sizeof(ent.filename)/sizeof(char) && ent.filename[i] != ' '; i++) {
                *short_file_name_next_free++ = ent.filename[i];
            }
            if(ent.extension[0] != ' ') *short_file_name_next_free++ = '.';
            for(uint64_t i=0; i<sizeof(ent.extension)/sizeof(char) && ent.extension[i] != ' '; i++) {
                *short_file_name_next_free++ = ent.extension[i];
            }

            mode_t node_type = 0;
            if(ent.attributes & FILE_ATTRIBUTES_DIRECTORY) {
                node_type |= S_IFDIR;
            } else {
                node_type |= S_IFREG;
            }

            if(long_file_name_next_free != long_file_name) {
                //update my local copy of the now-parent id to point at my specific directory entry number
                inode.parent_directory_entry_number = directory_i;

                if(strcmp(long_file_name, name) == 0) {
                    *out = (struct VNode) {
                        .id = {
                            .mount_id = directory_entry.mount_id,
                            .parent_inode = inode.data,
                            .self_inode = ent.cluster_number,
                        },
                        .directory_lookup = directory_lookup,
                        .write_file = write_file,
                        .read_file = read_file,
                        .create_inode = create_inode,
                        .stat_file = stat_file,
                    };
                    return 0;
                }
            } else {
                //short file name
                HCF
            }

            //reset LFN
            memset(long_file_name, 0, sizeof(long_file_name));
            long_file_name_next_free = long_file_name;
        }
    }
}

void mount_fat16(struct VNode block_device, const char* mount_name) {
    struct Fat16Header hdr;
    block_device.read_file(block_device.id, 0, (uint8_t*)&hdr, sizeof(struct Fat16Header));

    if(hdr.reserved_sectors < 1) HCF
    if(hdr.number_of_root_directory_entries < 1) HCF
    if(hdr.number_of_sectors == 0 && hdr.large_sector_count == 0) HCF
    if(hdr.number_of_sectors_per_fat < 1) HCF
    if(hdr.number_of_hidden_sectors != 0) HCF
    if(hdr.signature != 0x28 && hdr.signature != 0x29) HCF
    if(hdr.bootable_partition_signature != 0xAA55) HCF

    uint64_t mount_id=0;
    while(all_fat_mounts[mount_id].number_of_fats) {
        mount_id++;
        if(mount_id == MAX_FAT_MOUNTS) HCF//out of mounts
    }
    
    struct Fat16Volume new_vol = (struct Fat16Volume) {
        .block_device = block_device,
        //all the FATs start after the reserved sectors
        .fats_start_sector = hdr.reserved_sectors,
        .number_of_fats = hdr.file_allocation_tables_count,
        .number_of_sectors_per_fat = hdr.number_of_sectors_per_fat,
        .bytes_per_sector = hdr.bytes_per_sector,
        .sectors_per_cluster=hdr.sectors_per_cluster,
        .number_of_sectors_root_directory = (hdr.number_of_root_directory_entries * sizeof(struct Fat16DirectoryEntry) + hdr.bytes_per_sector-1) / hdr.bytes_per_sector,
    };
    all_fat_mounts[mount_id] = new_vol;

    struct VNode mount_vnode = {
        .id = {
            .mount_id = mount_id,
            .self_inode = 0,
            .parent_inode = UINT64_MAX
        },
        .directory_lookup=directory_lookup,
        .read_file = read_file,
        .write_file = write_file,
        .stat_file = stat_file,
        .create_inode = 0
    };

    vfs_add_mount(mount_name, mount_vnode);
}