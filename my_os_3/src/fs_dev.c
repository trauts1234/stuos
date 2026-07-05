#include "fs_dev.h"
#include "debugging.h"
#include "fs.h"
#include <uapi/fcntl.h>
#include <uapi/stat.h>
#include <uapi/stdint.h>
#include "kern_libc.h"

#define DEV_ROOT_DIR_INODE_NUM UINT64_MAX

#define MAX_BLOCK_DEVICES 26
#define MAX_PARTITIONS 9

struct BlockDevice {
    //heap allocated data
    void* driver_private;
    void (*block_read)(void* driver_private, uint64_t sector_number, uint8_t output[BLOCK_DEVICE_READ_SIZE]);
    void (*block_write)(void* driver_private, uint64_t sector_number, uint8_t input[BLOCK_DEVICE_READ_SIZE]);

    //guaranteed to be at least 1, since offset 0 length n to represent the whole drive is valid
    uint8_t num_partitions;
    struct BlockDevicePartition {
        uint64_t byte_offset;
        //TODO
        // uint64_t byte_length;
    } partitions[MAX_PARTITIONS];
};

static uint64_t next_free_block_device = 0;
static struct BlockDevice block_devices[MAX_BLOCK_DEVICES];

void fs_dev_add_block_device(
    void* driver_private,
    void (*block_read)(void* driver_private, uint64_t sector_number, uint8_t output[BLOCK_DEVICE_READ_SIZE]),
    void (*block_write)(void* driver_private, uint64_t sector_number, uint8_t input[BLOCK_DEVICE_READ_SIZE])
) {
    if(next_free_block_device == MAX_BLOCK_DEVICES) HCF

    struct BlockDevice result = {
        .driver_private = driver_private,
        .block_read = block_read,
        .block_write = block_write,
        .num_partitions = 1,
        .partitions = {
            [0] = {
                .byte_offset = 0
            }
        }
    };

    //try and read a MBR
    //TODO check for "EFI PART" GPT canary
    struct __attribute__((packed)) {
        uint8_t boot_code[440];
        //optional - can put boot code here
        uint32_t disk_id;
        //optional - can put boot code here
        uint16_t reserved;

        struct __attribute__((packed)) {
            uint8_t drive_attributes;
            // CHS Address of partition start
            // Ignore this as it is used for legacy <8GB disks
            uint8_t chs_part_start[3];
            uint8_t partition_type;
            // CHS address of last partition sector
            // Ignore this as it is used for legacy <8GB disks
            uint8_t chs_last_sector[3];
            uint32_t lba_partition_start;
            uint32_t partition_sector_count;
        } partition_table[4];
        uint16_t bootsector_signature;
    } mbr;
    block_read(driver_private, 0, (void*)&mbr);

    if(mbr.bootsector_signature == 0xAA55) {
        //MBR or fat, for sure
        //TODO try to exclude FAT using heuristics
        for(int i=0;i<4;i++) {
            if(result.num_partitions >= MAX_PARTITIONS) HCF
            if(mbr.partition_table[i].partition_sector_count == 0) break;
            result.partitions[result.num_partitions] = (struct BlockDevicePartition) {
                .byte_offset = mbr.partition_table[i].lba_partition_start * BLOCK_DEVICE_READ_SIZE
            };
            result.num_partitions++;
        }
    }

    block_devices[next_free_block_device++] = result;
}

void get_block_device(struct VNodeData inode_num, struct BlockDevice** blk_out, struct BlockDevicePartition* part_out) {
    if(inode_num.mount_id != 0) HCF
    if(inode_num.inode == DEV_ROOT_DIR_INODE_NUM) HCF

    uint16_t dev_num = (inode_num.inode >> 16) & 0xFFFF;
    if(dev_num >= next_free_block_device) HCF

    *blk_out = block_devices + dev_num;

    uint16_t part_num = inode_num.inode & 0xFFFF;
    if(part_num >= (*blk_out)->num_partitions) HCF

    *part_out = (*blk_out)->partitions[part_num];

}

static uint64_t blockdev_write_file(struct VNodeData inode_num, uint64_t offset, const uint8_t* input_buf, uint64_t num_bytes) {
    struct BlockDevice* dev;
    struct BlockDevicePartition part;
    get_block_device(inode_num, &dev, &part);

    //use partition to adjust location into disk
    offset += part.byte_offset;

    const uint64_t offset_in_first_sector = offset % BLOCK_DEVICE_READ_SIZE;
    const uint64_t first_sector_number = offset / BLOCK_DEVICE_READ_SIZE;
    const uint64_t last_sector_number = (offset + num_bytes-1) / BLOCK_DEVICE_READ_SIZE;
    
    uint64_t bytes_written = 0;
    uint64_t offset_in_sector = offset_in_first_sector;
    for(uint64_t sector = first_sector_number; sector <= last_sector_number; sector++) {
        //read the sector
        uint8_t data[BLOCK_DEVICE_READ_SIZE];
        dev->block_read(dev->driver_private, sector, data);

        //write over the appropriate region
        while(offset_in_sector < BLOCK_DEVICE_READ_SIZE && bytes_written < num_bytes) {
            data[offset_in_sector++] = input_buf[bytes_written++];
        }
        offset_in_sector = 0;//write from the start of the next sector

        //write back modified data
        dev->block_write(dev->driver_private, sector, data);
    }
    return num_bytes;
}

static uint64_t blockdev_read_file(struct VNodeData inode_num, uint64_t offset, uint8_t* output_buf, uint64_t num_bytes) {
    struct BlockDevice* dev;
    struct BlockDevicePartition part;
    get_block_device(inode_num, &dev, &part);

    //use partition to adjust location into disk
    offset += part.byte_offset;

    const uint64_t offset_in_first_sector = offset % BLOCK_DEVICE_READ_SIZE;
    const uint64_t first_sector_number = offset / BLOCK_DEVICE_READ_SIZE;
    const uint64_t last_sector_number = (offset + num_bytes-1) / BLOCK_DEVICE_READ_SIZE;
    
    uint64_t bytes_read = 0;
    uint64_t offset_in_sector = offset_in_first_sector;
    for(uint64_t sector = first_sector_number; sector <= last_sector_number; sector++) {
        //read the sector
        uint8_t data[BLOCK_DEVICE_READ_SIZE];
        dev->block_read(dev->driver_private, sector, data);

        //write over the appropriate region
        while(offset_in_sector < BLOCK_DEVICE_READ_SIZE && bytes_read < num_bytes) {
            output_buf[bytes_read++] = data[offset_in_sector++];
        }
        offset_in_sector = 0;//write from the start of the next sector

    }
    return num_bytes;
}

static struct stat blockdev_stat(struct VNodeData inode_num) {
    if(inode_num.mount_id != 0) HCF
    if(inode_num.inode == DEV_ROOT_DIR_INODE_NUM) HCF

    uint32_t device_type = inode_num.inode >> 32;
    if(device_type != 0) HCF//other device types not impletmented
    
    return (struct stat) {
        .st_ino = inode_num.inode,
        .st_mode = S_IFBLK,
        .st_uid = 0,
        .st_gid = 0,
        .st_size = 0,//TODO
    };
}

uint64_t devroot_read_dirents(struct VNodeData inode_num, uint64_t dirent_index, struct dirent* dirent_buf, struct VNode* vnode_buf, uint64_t dirent_count) {
    if(inode_num.inode != DEV_ROOT_DIR_INODE_NUM || inode_num.mount_id != 0) HCF

    uint64_t count = 0;
    for(uint64_t dev_num=0;dev_num<next_free_block_device;dev_num++) {
        struct BlockDevice *blk;
        blk = block_devices + dev_num;
        for(uint64_t partition_num=0; partition_num<blk->num_partitions; partition_num++) {
            if(dirent_index) {
                //still skipping
                dirent_index--;
                continue;
            }
            if(count == dirent_count) {
                //reached n
                return dirent_count;
            }

            const uint64_t ino = dev_num << 16 | partition_num;
            *dirent_buf++ = (struct dirent){
                .d_ino = ino,
                .d_name = {
                    'b','l','k',
                    'A' + dev_num,
                    partition_num ? 'p' : '\0',
                    partition_num ? '0'+partition_num : '\0',
                    '\0'
                }
            };
            *vnode_buf++ = (struct VNode){
                .id = {
                    .inode = ino,
                    .mount_id = 0
                },
                .stat_file=blockdev_stat,
                .write_file=blockdev_write_file,
                .read_file=blockdev_read_file,
            };

            count++;
        }
    }

    return count;
}

struct stat devroot_stat(struct VNodeData inode_num) {
    //TODO
    HCF
}

void devfs_init() {
    vfs_add_mount(
        vfs_get("/", "/dev", O_DIRECTORY),
        (struct VNode) {
            .id = {
                .inode = DEV_ROOT_DIR_INODE_NUM,
                .mount_id = 0,
            },
            .read_dirents = devroot_read_dirents,
            .write_file = 0,
            .read_file = 0,
            .stat_file = devroot_stat,
            .create_inode = 0,
        }
    );
}