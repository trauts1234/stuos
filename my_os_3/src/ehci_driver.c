#include "ehci_driver.h"
#include "pci.h"
#include "uapi/stdint.h"
#include "kern_libc.h"
#include "debugging.h"

//https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf

//NOTE: R/WC bits, writing 0 does nothing, and writing 1 sets it to 0 ????!!!!!

// struct USBCommandReg {
//     uint32_t
//         run: 1,
//         host_controller_reset: 1,
//         programmable_frame_list_size: 2,
//         periodic_schedule_enable: 1,
//         async_schedule_enable: 1,
//         interrupt_on_async_advance_doorbell: 1,//allows software to trigger interrupt
//         light_host_controller_reset: 1,//resets the controller without affecting connected devices
//         async_schedule_park_mode_count: 2,
//         reserved_1: 1,
//         async_schedule_park_mode_enable: 1,
//         reserved_2: 4,
//         interrupt_threshold: 8,
//         reserved_3: 8;
// };
struct USBStatusReg {
    uint32_t
    usb_transfer_interrupt: 1,
    usb_error_interrupt: 1,
    port_change_detect: 1,
    frame_list_rollover: 1,
    host_system_error: 1,
    doorbell_interrupt: 1,
    reserved_1: 6,
    halted: 1,
    reclamation: 1,
    periodic_schedule_status: 1,
    async_schedule_status: 1,
    reserved_2: 16;
};
struct USBInterruptEnableReg {
    uint32_t
    usb_transfer_interrupt_enable: 1,
    usb_error_interrupt_enable: 1,
    port_change_interrupt_enable: 1,
    frame_list_rollover_interrupt_enable: 1,
    host_system_error_interrupt_enable: 1,
    async_advance_interrupt_enable: 1,
    reserved_1: 26;
};
struct PortStatusControlReg {
    uint32_t
    connected: 1,
    connect_changed: 1,
    port_enabled: 1,
    port_enabled_changed: 1,
    overcurrent: 1,
    overcurrent_change: 1,
    force_port_resume: 1,
    suspend: 1,
    port_reset: 1,
    reserved_1: 1,
    line_status: 2,
    port_power: 1,
    companion_port_control: 1,
    port_indicator_control: 2,
    port_test_control: 4,
    wake_on_connect_enable: 1,
    wake_on_disconnect_enable: 1,
    wake_on_overcurrent_enable: 1,
    reserved_2: 9;
};
struct StructuralParameters {
    uint32_t
        n_ports: 4,//N_PORTS
        port_power_control: 1,//PPC
        reserved_1: 2,
        port_routing_rules: 1,
        ports_per_companion_controller: 4,//N_PCC
        number_of_comparison_controllers: 4,//N_CC
        port_indicators: 1,//P_INDICATOR
        reserved_2: 3,
        debug_port_number: 4,
        reserved_3: 8;
};
struct CapabilityParameters {
    uint32_t
        is_64_bit: 1,
        programmable_frame_list_flag: 1,
        async_schedule_park_capability: 1,
        reserved_1: 1,
        isochronous_scheduling_threshold: 4,
        ehci_extended_capabilities_pointer: 8,//EECP
        reserved_2: 16;
};

struct USBLegSup {
    //USBLEGSUP - USB legacy support - in PCI config plus ECCP value
    uint32_t
        capability_id: 8,//R
        next_ehci_extended_capability_pointer: 8,//R, 0 is NULL
        bios_owned_semaphore: 1,//RW
        reserved_1: 7,
        os_owned_semaphore: 1,//RW
        reserved_2: 7;
};
struct USBLegControlStatus {
    //USBLEGCTLSTS - USB legacy support control + status - 0 by default
    uint32_t
        usb_smi_enable: 1,//RW
        smi_on_usb_error_enable: 1,//RW
        smi_on_port_change_enable: 1,//RW
        smi_on_frame_list_rollover_enable: 1,//RW
        smi_on_host_system_error_enable: 1,//RW
        smi_on_async_advance_enable: 1,//RW
        reserved_3: 7,
        smi_on_os_ownership_enable: 1,//RW
        smi_on_pci_command_enable: 1,//RW
        smi_on_bar_enable: 1,//RW
        smi_on_usb_complete: 1,//R
        smi_on_usb_error: 1,//R
        smi_on_port_change_detect: 1,//R
        smi_on_frame_list_rollover: 1,//R
        smi_on_host_system_error: 1,//R
        smi_on_async_advance: 1,//R
        reserved_4: 7,
        smi_on_os_ownership_change: 1,//R/WC
        smi_on_pci_command: 1,//R/WC
        smi_on_bar: 1;//R/WC
};

//offsets into BAR0

//capability register length (read lowest byte only)
#define CAPLENGTH_OFFSET 0
//structural params
#define HCSPARAMS_OFFSET 4
//capability params
#define HCCPARAMS 8

//offsets into BAR0+CAPLENGTH

#define USBCMD_OFFSET 0
#define USBSTS_OFFSET 4

#define USBLEGSUP_BIOS_OWNED_SEMAPHORE (1 << 16)
#define USBLEGSUP_OS_OWNED_SEMAPHORE (1 << 24)
#define USBSTS_HCHALTED (1 << 12)
#define USBCMD_HCRESET (1 << 1)

// struct CapabilityRegisters {
//     uint8_t capability_register_length;//CAPLENGTH
//     uint8_t reserved_1;
//     uint16_t interface_version_number;//HCIVERSION
//     struct StructuralParameters structural_parameters;//HCSPARAMS
//     struct CapabilityParameters capability_parameters;//HCCPARAMS
//     uint32_t companion_port_route_description;//HCSP-PORTROUTE (only valid if structural_parameters.port_routing_rules)
// };

// struct OperationRegisters {
//     struct USBCommandReg usb_command;//USBCMD
//     struct USBStatusReg usb_status;//USBSTS
//     struct USBInterruptEnableReg usb_interrupt_enable;//USBINTR
//     uint32_t usb_frame_index;//FRINDEX
//     uint32_t segment_selector_4g;//CTRLDSSEGMENT
//     uint32_t frame_list_base_address;//PERIODICLISTBASE
//     uint32_t next_async_list_address;//ASYNCLISTADDR
//     uint8_t padding[36];
//     uint32_t configured_flag_register;//CONFIGFLAG
//     struct PortStatusControlReg port_status_control_register[];//PORTSC - index 0,1,2,...,N_PORTS-1

// };

void initialise_ehci(struct PciDevice dev, struct BarInfo bar) {
    uint8_t cap_length = read_bar_32(bar, CAPLENGTH_OFFSET);
    uint32_t capability_parameters = read_bar_32(bar, HCCPARAMS);

    // assert(cap_regs.structural_parameters.reserved_1 == 0);
    // assert(cap_regs.structural_parameters.reserved_2 == 0);
    // assert(cap_regs.structural_parameters.reserved_3 == 0);

    // assert(cap_regs.capability_parameters.reserved_1 == 0);
    // assert(cap_regs.capability_parameters.reserved_2 == 0);

    // assert(op_regs.usb_command.reserved_1 == 0);
    // assert(op_regs.usb_command.reserved_2 == 0);
    // assert(op_regs.usb_command.reserved_3 == 0);

    // assert(op_regs.usb_status.reserved_1 == 0);
    // assert(op_regs.usb_status.reserved_2 == 0);

    // assert(op_regs.usb_interrupt_enable.reserved_1 == 0);

    // for(int i=0; i<36; i++) {
    //     assert(op_regs.padding[i] == 0);
    // }

    // for(int port_idx=0; port_idx < cap_regs.structural_parameters.n_ports; port_idx++) {
    //     assert(op_regs.port_status_control_register[port_idx].reserved_1 == 0);
    //     printf("%d: %X\n", port_idx, op_regs.port_status_control_register[port_idx].reserved_2);
    //     assert(op_regs.port_status_control_register[port_idx].reserved_2 == 0);
    // }

    uint8_t pci_config[256];
    read_header(dev, pci_config);
    
    for(uint32_t ptr = (capability_parameters >> 8) & 0xFF; ptr >= 0x40; ) {
        uint32_t *usblegsup = (uint32_t*)(pci_config + ptr);
        if(*usblegsup & USBLEGSUP_BIOS_OWNED_SEMAPHORE) {
            //EHCI controller is under the ownership of the BIOS
            *usblegsup |= USBLEGSUP_OS_OWNED_SEMAPHORE;
            int timeout = 100;
            while(timeout && *usblegsup & USBLEGSUP_BIOS_OWNED_SEMAPHORE) {
                // mmm some tasty race conditions
                write_header(dev, pci_config);
                for(int i=0; i<1000; i++) __asm("nop");
                read_header(dev, pci_config);
                timeout--;
            }
            if(timeout == 0) {
                printf("timed out whilst trying to take ownership of EHCI");
                HCF
            }
        }
        //extract the next pointer
        ptr = (*usblegsup >> 8) & 0xFF;
    }

    //TODO
    // if(cap_regs.capability_parameters.is_64_bit) {
        
    // } else {
    //     assert(op_regs.segment_selector_4g == 0);
    // }

    //halt the EHCI chip
    write_bar_32(bar, 0, cap_length + USBCMD_OFFSET);
    uint32_t usb_sts;
    do {
        usb_sts = read_bar_32(bar, cap_length + USBSTS_OFFSET);
    } while((usb_sts & USBSTS_HCHALTED) == 0);

    printf("success!\n");

    //reset the EHCI chip
    write_bar_32(bar, USBCMD_HCRESET, cap_length + USBCMD_OFFSET);
    uint32_t usb_cmd;
    do {
        usb_cmd = read_bar_32(bar, cap_length + USBCMD_OFFSET);
    } while(usb_cmd & USBCMD_HCRESET);

    printf("success2!\n");

}