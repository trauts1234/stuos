#include "debugging.h"
#include "kern_libc.h"
#include "pci.h"
#include "memory.h"
#include "uapi/stdint.h"
#include "virtio_driver.h"

struct BlockDeviceRegisters {
    uint64_t total_sector_count;
    uint32_t maximum_segment_size;
    uint32_t maximum_segment_count;
    uint16_t cylinder_count;
    uint8_t head_count;
    uint8_t sector_count;
    uint32_t block_length;
};

struct VirtioCapabilitiesHeader {
    uint8_t capability_id;
    //0 for NULL
    uint8_t next;
    uint8_t capability_length;
    uint8_t config_type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset_in_bar;
    uint32_t length_in_bar;
};

struct Commonotify_cfg {
    uint32_t device_feature_select;//RW
    uint32_t device_feature;//R
    uint32_t driver_feature_select;//RW
    uint32_t driver_feature;//RW

    uint16_t config_msix_vector;//RW
    uint16_t num_queues;//R

    uint8_t device_status;//RW
    uint8_t config_generation;//R

    uint16_t queue_select;//RW
    uint16_t queue_size;//RW
    uint16_t queue_msix_vector;//RW
    uint16_t queue_enable;//RW
    uint16_t queue_notify_off;//R

    uint64_t queue_desc;//RW
    uint64_t queue_driver;//RW
    uint64_t queue_device;//RW

    uint16_t queue_notif_config_data;//R
    uint16_t queue_reset;

    uint16_t admin_queue_index;//R
    uint16_t admin_queue_num;//R
};
struct NotifyCfg {
    struct VirtioCapabilitiesHeader cap;
    uint8_t pci_cfg_data[4];//data for BAR access
};
struct IsrCfg {
    uint8_t queue_interrupt: 1;
    uint8_t device_configuration_interrupt:1;
    uint32_t reserved: 30;
};

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer, struct BarInfo bar_list[6]) {
    if(header.vendor_id != 0x1AF4) HCF
    if(header.device_id < 0x1000 || header.device_id > 0x103F) HCF

    struct Commonotify_cfg common_cfg;
    struct NotifyCfg notify_cfg;
    struct IsrCfg isr_cfg;

    if((header.status & 0b10000) == 0) HCF//need capabilities list
    struct VirtioCapabilitiesHeader *capabilities = (struct VirtioCapabilitiesHeader*)(header_buffer + (header.capabilities_pointer & ~0b11));
    while(capabilities->next != 0) {
        capabilities = (struct VirtioCapabilitiesHeader*)(header_buffer + capabilities->next);
        kprintf("cap id: %d cap len: %d cap type: %d bar: %d offset: %d length: %d\n\n", capabilities->capability_id, capabilities->capability_length, capabilities->config_type, capabilities->bar, capabilities->offset_in_bar, capabilities->length_in_bar);
        
        struct BarInfo bar = bar_list[capabilities->bar];

        if(capabilities->capability_id != 0x09) continue;// only want vendor-specific
        switch(capabilities->config_type) {
            case 1:
            // COMMON_CFG
            //crash if the struct is too big according to the capabilities, or the BAR size isn't big enough for the capabilities
            if(sizeof(common_cfg) > capabilities->length_in_bar) HCF
            read_bar(bar, &common_cfg, capabilities->offset_in_bar, sizeof(common_cfg));

            break;

            case 2:
            // NOTIFY_CFG
            if(sizeof(notify_cfg) > capabilities->length_in_bar) HCF
            read_bar(bar, &notify_cfg, capabilities->offset_in_bar, sizeof(notify_cfg));

            break;
            case 3:
            // ISR_CFG
            if(sizeof(isr_cfg) > capabilities->length_in_bar) HCF
            read_bar(bar, &isr_cfg, capabilities->offset_in_bar, sizeof(isr_cfg));

            break;
            case 4:
            // DEVICE_CFG

            break;
            case 5:
            // PCI_CFG

            break;
            case 8:
            // SHARED_MEMORY_CFG

            break;

            case 9:
            // VENDOR_CFG - ignore this
            break;

            default:
            HCF//invalid?
        }
    }

    kprintf("\n=== CommonCfg ===\n");
    kprintf("  device_feature_select:    0x%08X (%u)\n", common_cfg.device_feature_select, common_cfg.device_feature_select);
    kprintf("  device_feature:           0x%08X (%d)\n", common_cfg.device_feature, common_cfg.device_feature);
    kprintf("  driver_feature_select:    0x%08X (%u)\n", common_cfg.driver_feature_select, common_cfg.driver_feature_select);
    kprintf("  driver_feature:           0x%08X (%u)\n", common_cfg.driver_feature, common_cfg.driver_feature);
    kprintf("  config_msix_vector:       0x%04X (%u)\n", common_cfg.config_msix_vector, common_cfg.config_msix_vector);
    kprintf("  num_queues:               0x%04X (%u)\n", common_cfg.num_queues, common_cfg.num_queues);
    kprintf("  device_status:            0x%02X (%u)\n", common_cfg.device_status, common_cfg.device_status);
    kprintf("  config_generation:        0x%02X (%u)\n", common_cfg.config_generation, common_cfg.config_generation);
    kprintf("  queue_select:             0x%04X (%u)\n", common_cfg.queue_select, common_cfg.queue_select);
    kprintf("  queue_size:               0x%04X (%u)\n", common_cfg.queue_size, common_cfg.queue_size);
    kprintf("  queue_msix_vector:        0x%04X (%u)\n", common_cfg.queue_msix_vector, common_cfg.queue_msix_vector);
    kprintf("  queue_enable:             0x%04X (%u)\n", common_cfg.queue_enable, common_cfg.queue_enable);
    kprintf("  queue_notify_off:         0x%04X (%u)\n", common_cfg.queue_notify_off, common_cfg.queue_notify_off);
    kprintf("  queue_desc:               0x%016llX (%llu)\n",
        (unsigned long long)common_cfg.queue_desc, (unsigned long long)common_cfg.queue_desc);
    kprintf("  queue_driver:             0x%016llX (%llu)\n",
        (unsigned long long)common_cfg.queue_driver, (unsigned long long)common_cfg.queue_driver);
    kprintf("  queue_device:             0x%016llX (%llu)\n",
        (unsigned long long)common_cfg.queue_device, (unsigned long long)common_cfg.queue_device);
    kprintf("  queue_notif_config_data:  0x%04X (%u)\n", common_cfg.queue_notif_config_data, common_cfg.queue_notif_config_data);
    kprintf("  queue_reset:              0x%04X (%u)\n", common_cfg.queue_reset, common_cfg.queue_reset);
    kprintf("  admin_queue_index:        0x%04X (%u)\n", common_cfg.admin_queue_index, common_cfg.admin_queue_index);
    kprintf("  admin_queue_num:          0x%04X (%u)\n", common_cfg.admin_queue_num, common_cfg.admin_queue_num);

    kprintf("\n=== NotifyCfg ===\n");
    kprintf("  cap.capability_id:    0x%02X (%u)\n", notify_cfg.cap.capability_id, notify_cfg.cap.capability_id);
    kprintf("  cap.next:             0x%02X (%u)\n", notify_cfg.cap.next, notify_cfg.cap.next);
    kprintf("  cap.capability_length:0x%02X (%u)\n", notify_cfg.cap.capability_length, notify_cfg.cap.capability_length);
    kprintf("  cap.config_type:      0x%02X (%u)\n", notify_cfg.cap.config_type, notify_cfg.cap.config_type);
    kprintf("  cap.bar:              0x%02X (%u)\n", notify_cfg.cap.bar, notify_cfg.cap.bar);
    kprintf("  cap.padding:          [0x%02X, 0x%02X, 0x%02X]\n",
        notify_cfg.cap.padding[0], notify_cfg.cap.padding[1], notify_cfg.cap.padding[2]);
    kprintf("  cap.offset_in_bar:    0x%08X (%u)\n", notify_cfg.cap.offset_in_bar, notify_cfg.cap.offset_in_bar);
    kprintf("  cap.length_in_bar:    0x%08X (%u)\n", notify_cfg.cap.length_in_bar, notify_cfg.cap.length_in_bar);
    kprintf("  pci_cfg_data:         [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n",
        notify_cfg.pci_cfg_data[0], notify_cfg.pci_cfg_data[1],
        notify_cfg.pci_cfg_data[2], notify_cfg.pci_cfg_data[3]);

    kprintf("\n=== IsrCfg ===\n");
    kprintf("  queue_interrupt:                %u\n", isr_cfg.queue_interrupt);
    kprintf("  device_configuration_interrupt: %u\n", isr_cfg.device_configuration_interrupt);

    switch(header.subsystem_id) {
        case 2:
        //block device

        default:
        return;
    }
}