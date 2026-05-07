#include "debugging.h"
#include "fs.h"
#include "uapi/stdint.h"
#include "kern_libc.h"

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
    FILE_ATTRIBUTES_DIRECTORY = 0x10, FILE_ATTRIBUTES_ARCHIVE = 0x20,
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

//offset and num_bytes must be sanitised first, as this will crash or cause UB on reading past the end of file
static void read_cluster_chain(struct Fat16Volume *vol, uint16_t cluster_number, uint64_t requested_offset, void* output_buf, uint64_t num_bytes) {
    const uint64_t bytes_per_cluster = vol->bytes_per_sector * vol->sectors_per_cluster;
    const uint64_t required_cluster_number = requested_offset / bytes_per_cluster;
    const uint64_t required_offset_in_first_cluster = requested_offset % bytes_per_cluster;
    const uint64_t required_number_of_clusters = (num_bytes + bytes_per_cluster-1) / bytes_per_cluster;


    //skip the required number of clusters
    for(uint64_t i=0; i<required_cluster_number + required_number_of_clusters; i++) {

        //out of sectors
        if(cluster_number >= 0xFFF8) HCF

        uint64_t fat_offset = vol->fats_start_sector*vol->bytes_per_sector + 2*cluster_number;
        uint16_t fat_data = 0;
        vol->block_device.read_file(vol->block_device.id, fat_offset, (uint8_t*)&fat_data, 2);

        //bad sector
        if(fat_data == 0xFFF7) HCF

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

            // kprintf("reading %d bytes from 0x%lX\n", num_bytes_to_read, src_addr);

            vol->block_device.read_file(vol->block_device.id, src_addr, output_buf, num_bytes_to_read);
            output_buf += num_bytes_to_read;
            num_bytes -= num_bytes_to_read;
        }
        
        //skip to the next entry
        cluster_number = fat_data;
    }
}

static uint64_t read_file(struct VNodeId inode_num, uint64_t offset, uint8_t* output_buf, uint64_t num_bytes) {
    struct Fat16Volume vol = all_fat_mounts[inode_num.mount_id];
    kprintf("read in mount %d, inode %d\n", inode_num.mount_id, inode_num.inode_number * vol.bytes_per_sector);

    read_cluster_chain(&vol, inode_num.inode_number, offset, output_buf, num_bytes);

    return num_bytes;
}

static int directory_lookup(struct VNodeId directory_entry, const char* name, struct VNode* out) {
    struct Fat16Volume vol = all_fat_mounts[directory_entry.mount_id];

    //microsoft spec limits file names to 255 chars
    char long_file_name[256] = {};
    char* long_file_name_next_free = long_file_name;
    
    for(uint64_t directory_i=0;;directory_i++) {
        struct Fat16DirectoryEntry ent;
        uint64_t addr;
        if(directory_entry.inode_number == UINT64_MAX) {
            //is root directory
            addr = (vol.fats_start_sector + vol.number_of_sectors_per_fat*vol.number_of_fats) * vol.bytes_per_sector;
        } else {
            //inode number is a cluster number
            HCF
        }
        addr += directory_i*sizeof(struct Fat16DirectoryEntry);

        vol.block_device.read_file(vol.block_device.id, addr, (uint8_t*)&ent, sizeof(struct Fat16DirectoryEntry));
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
            if(ent.zero) HCF

            char short_file_name[12] = {0};
            char* short_file_name_next_free = short_file_name;
            //copy file name
            for(int i=0; i<sizeof(ent.filename)/sizeof(char) && ent.filename[i] != ' '; i++) {
                *short_file_name_next_free++ = ent.filename[i];
            }
            if(ent.extension[0] != ' ') *short_file_name_next_free++ = '.';
            for(int i=0; i<sizeof(ent.extension)/sizeof(char) && ent.extension[i] != ' '; i++) {
                *short_file_name_next_free++ = ent.extension[i];
            }

            enum VNodeType node_type;
            if(ent.attributes & FILE_ATTRIBUTES_DIRECTORY) {
                node_type = VNODE_DIR;
            } else {
                node_type = VNODE_FILE;
            }

            if(long_file_name_next_free != long_file_name) {
                kprintf("long file name: %s, expected %s\n", long_file_name, name);
                if(strcmp(long_file_name, name) == 0) {
                    *out = (struct VNode) {
                        .id = {
                            .inode_number = ent.cluster_number,
                            .mount_id = directory_entry.mount_id
                        },
                        .inode_type = node_type,
                        .directory_lookup = directory_lookup,
                        .write_file = NULL,
                        .read_file = read_file,
                        .create_inode = NULL,
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

    struct Fat16Volume result;
    result.block_device = block_device;

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
    kprintf("fats start sector: %d\nnumber of sectors per fat: %d\nbytes per sector: %d\n", new_vol.fats_start_sector, new_vol.number_of_sectors_per_fat, new_vol.bytes_per_sector);
    all_fat_mounts[mount_id] = new_vol;

    struct VNode mount_vnode = {
        .id = {
            .inode_number = UINT64_MAX,
            .mount_id = mount_id
        },
        .directory_lookup=directory_lookup,
        .read_file = read_file,
    };

    vfs_add_mount(mount_name, mount_vnode);
}