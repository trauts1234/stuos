#include "debugging.h"
#include "fs.h"
#include "uapi/stdint.h"
#include "kern_libc.h"
#include "virtio_driver.h"

uint64_t write_file(struct VNodeId inode_num, uint64_t offset, const uint8_t* input_buf, uint64_t num_bytes) {
    if(inode_num.inode_number != 0) HCF

    switch (inode_num.mount_id) {
    case 0:
    const uint64_t offset_in_first_sector = offset % BLOCK_DEVICE_READ_SIZE;
    const uint64_t first_sector_number = offset / BLOCK_DEVICE_READ_SIZE;
    const uint64_t last_sector_number = (offset + num_bytes-1) / BLOCK_DEVICE_READ_SIZE;
    
    uint64_t bytes_written = 0;
    uint64_t offset_in_sector = offset_in_first_sector;
    for(uint64_t sector = first_sector_number; sector <= last_sector_number; sector++) {
        //read the sector
        uint8_t data[BLOCK_DEVICE_READ_SIZE];
        virtio_block_read(sector, data);

        //write over the appropriate region
        while(offset_in_sector < BLOCK_DEVICE_READ_SIZE && bytes_written < num_bytes) {
            data[offset_in_sector++] = input_buf[bytes_written++];
        }
        offset_in_sector = 0;//write from the start of the next sector

        //write back modified data
        virtio_block_write(sector, data);
    }
    return num_bytes;
    
    default:
    HCF

    }
}

uint64_t read_file(struct VNodeId inode_num, uint64_t offset, uint8_t* output_buf, uint64_t num_bytes) {
    if(inode_num.inode_number != 0) HCF

    switch (inode_num.mount_id) {
    case 0:
    const uint64_t offset_in_first_sector = offset % BLOCK_DEVICE_READ_SIZE;
    const uint64_t first_sector_number = offset / BLOCK_DEVICE_READ_SIZE;
    const uint64_t last_sector_number = (offset + num_bytes-1) / BLOCK_DEVICE_READ_SIZE;
    
    uint64_t bytes_read = 0;
    uint64_t offset_in_sector = offset_in_first_sector;
    for(uint64_t sector = first_sector_number; sector <= last_sector_number; sector++) {
        //read the sector
        uint8_t data[BLOCK_DEVICE_READ_SIZE];
        virtio_block_read(sector, data);

        //write over the appropriate region
        while(offset_in_sector < BLOCK_DEVICE_READ_SIZE && bytes_read < num_bytes) {
            output_buf[bytes_read++] = data[offset_in_sector++];
        }
        offset_in_sector = 0;//write from the start of the next sector

    }
    return num_bytes;
    
    default:
    HCF

    }
}

static const char* device_names[] = {"disk"};

static const struct VNode device_vnodes[] = {
    (struct VNode){
        .id = {
            .inode_number=0,
            .mount_id=0,
        },
        .inode_type=VNODE_FILE,
        .directory_lookup = 0,
        .write_file = write_file,
        .read_file = read_file,
        .create_inode = 0,
    },
};

#define DEV_ROOT_DIR_INODE_NUM 10000

static int directory_lookup(struct VNodeId dir_inode_num, const char* name, struct VNode* out) {
    if(dir_inode_num.inode_number != 0 || dir_inode_num.mount_id != 0) HCF
    for(uint64_t i=0; i<sizeof(device_names)/sizeof(char*); i++) {
        if(strcmp(name, device_names[i])) continue;
        *out = device_vnodes[i];
        return 0;
    }
    return -1;
}

void devfs_init() {
    vfs_add_mount(
        "dev",
        (struct VNode) {
            .id = {
                .inode_number = 0,
                .mount_id = 0,
            },
            .inode_type = VNODE_DIR,
            .directory_lookup = directory_lookup,
            .write_file = 0,
            .read_file = 0,
            .create_inode = 0,
        }
    );
}