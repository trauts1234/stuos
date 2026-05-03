#include "debugging.h"
#include "fs.h"
#include "uapi/stdint.h"

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
    FILE_ATTRIBUTES_LFN = FILE_ATTRIBUTES_RO | FILE_ATTRIBUTES_HIDDEN | FILE_ATTRIBUTES_SYSTEM | FILE_ATTRIBUTES_VOLUMEID | FILE_ATTRIBUTES_DIRECTORY | FILE_ATTRIBUTES_ARCHIVE
};

struct __attribute__((packed)) FatTime {
    uint8_t hour:5;
    uint8_t minute:6;
    //double this
    uint8_t seconds:5;
};

struct __attribute__((packed)) FatDate {
    uint8_t year: 7;
    uint8_t month: 4;
    uint8_t day: 5;
};

struct __attribute__((packed)) Fat16DirectoryEntry {
    //8.3 file name. The first 8 characters are the name and the last 3 are the extension.
    char filename[11];
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



#define MAX_FAT_MOUNTS 10
struct Fat16Volume {
    struct VNode block_device;
    //the sector number of the first file allocation table
    uint16_t fats_start_sector;
    //how many contiguous file allocation tables there are
    uint8_t number_of_fats;
    //number of sectors per file allocation table
    uint16_t number_of_sectors_per_fat;
} all_fat_mounts[MAX_FAT_MOUNTS] = {};

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

    // uint16_t root_dir_sectors = (hdr.number_of_root_directory_entries*sizeof(struct Fat16DirectoryEntry) + hdr.bytes_per_sector-1) / hdr.bytes_per_sector;
    uint32_t root_dir_start_sector = hdr.reserved_sectors + hdr.number_of_sectors_per_fat*hdr.file_allocation_tables_count;
    
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
    };
    all_fat_mounts[mount_id] = new_vol;
    

    vfs_add_mount(mount_name, (struct VNode) {
        .id = {
            .inode_number = root_dir_start_sector,
            .mount_id = mount_id
        }
    });
}