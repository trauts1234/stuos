#include "apic.h"
#include "debugging.h"
#include "idt.h"
#include "io.h"
#include "tty.h"
#include <uapi/stdint.h>
#include "kern_libc.h"
#include "ps2_driver.h"

#define SCANCODE_VERSION 2

#if SCANCODE_VERSION == 1
#define LEFT_SHIFT 0x2A
#define RIGHT_SHIFT 0x36
#define CAPS_LOCK 0x3A
//indexed by [keycode][is_shifted]
//if [keycode][1] == 0, then use [keycode][0] as there is no shifted key
//for caps OR shift, apply toupper() afterwards too
char lookup_nonextended[128][2] = {
    [0x02] = {'1', '!'},
    {'2', '"'},
    {'3'},
    {'4', '$'},
    {'5', '%'},
    {'6', '^'},
    {'7', '&'},
    {'8', '*'},
    {'9', '('},
    {'0', ')'},
    {'-','_'},
    {'=','+'},
    {'\b'},
    {'\t'},
    {'q'},
    {'w'},
    {'e'},
    {'r'},
    {'t'},
    {'y'},
    {'u'},
    {'i'},
    {'o'},
    {'p'},
    {'[', '{'},
    {']', '}'},
    {'\n'},
    [0x1E] = {'a'},
    {'s'},
    {'d'},
    {'f'},
    {'g'},
    {'h'},
    {'j'},
    {'k'},
    {'l'},
    {';', ':'},
    {'\'', '@'},
    {'`'},
    [0x2B] = {'\\', '|'},
    {'z'},
    {'x'},
    {'c'},
    {'v'},
    {'b'},
    {'n'},
    {'m'},
    {',', '<'},
    {'.', '>'},
    {'/', '?'},
    [0x39] = {' '}
};
#elif SCANCODE_VERSION == 2

#define LEFT_SHIFT 0x12
#define RIGHT_SHIFT 0x59
#define CAPS_LOCK 0x58

//indexed by [keycode][is_shifted]
//if [keycode][1] == 0, then use [keycode][0] as there is no shifted key
//for caps OR shift, apply toupper() afterwards too
char lookup_nonextended[128][2] = {
    [0x1C]= {'a'},
    [0x32]= {'b'},
    [0x21]= {'c'},
    [0x23]= {'d'},
    [0x24]= {'e'},
    [0x2b]= {'f'},
    [0x34]= {'g'},
    [0x33]= {'h'},
    [0x43]= {'i'},
    [0x3B]= {'j'},
    [0x42]= {'k'},
    [0x4b]= {'l'},
    [0x3a]= {'m'},
    [0x31]= {'n'},
    [0x44]= {'o'},
    [0x4d]= {'p'},

    [0x15]= {'q'},
    [0x2d]= {'r'},
    [0x1b]= {'s'},
    [0x2c]= {'t'},
    [0x3c]= {'u'},
    [0x2a]= {'v'},
    [0x1d]= {'w'},
    [0x22]= {'x'},
    [0x35]= {'y'},
    [0x1a]= {'z'},

    [0x16]= {'1', '!'},
    [0x1e]= {'2', '"'},
    [0x26]= {'3'},
    [0x25]= {'4', '$'},
    [0x2e]= {'5', '%'},
    [0x36]= {'6', '^'},
    [0x3d]= {'7', '!'},
    [0x3e]= {'8', '*'},
    [0x46]= {'9', '('},
    [0x45]= {'0', ')'},
    [0x4e]= {'-', '_'},

    [0x5a]= {'\n'},
    [0x66]= {'\b'},
    [0x29]= {' '},
    [0x4a]= {'/', '?'},
    [0x49]= {'.', '>'},
    [0x41]= {',', '<'},
    [0x5d]= {'\\', '|'},
    [0x4c]= {';', ':'},
    [0x52]= {'\'', '@'},
};

#endif

#define DATA_PORT 0x60
#define COMMAND_PORT 0x64

//To send a command to the controller, write the command byte to IO port 0x64.
// If the command is 2 bytes long, then the next byte needs to be written to IO Port 0x60 after making sure that the controller is ready for it (by making sure bit 1 of the Status Register is clear).
// If there is a response byte, then the response byte needs to be read from IO Port 0x60 after making sure it has arrived (by making sure bit 0 of the Status Register is set).
enum ControllerCommand {
    READ_CONTROLLER_CONFIGURATION=0x20,
    WRITE_CONTROLLER_CONFIGURATION=0x60,
    
    DISABLE_SECOND_PORT=0xA7,
    ENABLE_SECOND_PORT=0xA8,
    DISABLE_FIRST_PORT=0xAD,
    ENABLE_FIRST_PORT=0xAE,

    TEST_CONTROLLER=0xAA,

    TEST_FIRST_PORT=0xAB,
    TEST_SECOND_PORT=0xA9,

    DIVERT_BYTE_TO_SECOND_PORT=0xD4,
};

// To send a command to the keyboard, send to DATA_PORT
enum KeyboardCommand {
    KB_SET_GET_SCAN_CODE=0xF0,
    KB_ENABLE_SCANNING=0xF4,
    KB_RESTART_AND_TEST=0xFF,
};

//The Status Register contains various flags that show the state of the PS/2 controller
union StatusRegister {
    uint8_t byte;

    struct {
        uint8_t output_buffer_status: 1;// (0 = empty, 1 = full) must be set before attempting to read data from DATA_PORT
        uint8_t input_buffer_status: 1;//  (0 = empty, 1 = full) must be clear before attempting to write data to DATA_PORT OR COMMAND_REGISTER_W
        uint8_t system_flag: 1;//should be clear if system passes POST
        uint8_t command_data: 1;// 0 = data written to input buffer is data for PS/2 device, 1 = data written to input buffer is data for PS/2 controller command
        uint8_t unknown: 2;
        uint8_t timeout_error: 1;// 0 = no error, 1 = time-out error
        uint8_t parity_error: 1; //0 = no error, 1 = parity error
    };
};

union ControllerConfiguration {
    uint8_t byte;

    struct {
        uint8_t first_ps2_interrupt : 1; // First PS/2 port interrupt enabled
        uint8_t second_ps2_interrupt : 1; // Second PS/2 port interrupt enabled
        uint8_t system_flag : 1; // should be set if system passes POST
        uint8_t zero_A : 1; // Must be zero
        uint8_t first_ps2_port_clock : 1; // (1 = disabled, 0 = enabled)
        uint8_t second_ps2_port_clock : 1; // (1 = disabled, 0 = enabled)
        uint8_t first_ps2_translation : 1; // (1 = enabled, 0 = disabled)
        uint8_t zero_B : 1; // Must be zero
    };
};

static void silly_delay() {
    for(int i=0; i<10000000; i++) {
        __asm("nop");
    }
}

//IO COMMAND_PORT
static union StatusRegister read_status_register() {
    return (union StatusRegister) {.byte = in8(COMMAND_PORT)};
}
static void send_command(enum ControllerCommand command) {
    if(read_status_register().input_buffer_status) HCF
    out8(COMMAND_PORT, command);
    silly_delay();
}
//IO DATA_PORT
static uint8_t blocking_read_data() {
    while(!read_status_register().output_buffer_status);//wait for full buffer
    return in8(DATA_PORT);
}
static void blocking_write_data(uint8_t data) {
    while(read_status_register().input_buffer_status);// wait for empty buffer
    out8(DATA_PORT, data);
    silly_delay();
}

static void send_byte_to_first_ps2(uint8_t data) {
    blocking_write_data(data);
}
// static void send_byte_to_second_ps2(uint8_t data) {
//     send_command(DIVERT_BYTE_TO_SECOND_PORT);
//     blocking_write_data(data);
// }

static void wait_for_fa_aa() {
    bool first_fa=false, first_aa=false;
    while(!first_fa || !first_aa) {
        uint8_t input = blocking_read_data();
        if(input == 0xFA) {
            first_fa = true;
        }else if(input == 0xAA) {
            first_aa = true;
        } else {
            printf("invalid byte when getting fa aa: %X", input);
            HCF
        }
    }
}

union BufferData {
    uint64_t data;
    struct {
        //least significant
        uint8_t first_byte;
        //middle byte
        uint8_t second_byte;
        //most significant
        uint8_t third_byte;
    };
};

#if SCANCODE_VERSION == 1
static char parse_full_buffer(union BufferData buffer) {
    static bool capslock = false;
    static bool shift = false;

    if (buffer.data == 0xE11D45E19DC5) {
        return 0;//pause
    }
    if (buffer.data == 0xE02AE037) {
        return 0;//printscr
    }
    if (buffer.data == 0xE0B7E0AA) {
        return 0;//printscr up
    }
    

    //only 0xXX, 0xE0XX exist
    assert(buffer.second_byte == 0xE0 || buffer.second_byte == 0);
    assert((buffer.data & ~0xFFFF) == 0);

    //highest bit is the "is break" flag
    bool is_break = buffer.first_byte & 0x80;
    bool is_extended = buffer.second_byte == 0xE0;
    uint8_t scan_code = buffer.first_byte & 0x7F;

    
    if(!is_extended && scan_code == CAPS_LOCK) {
        //capslock toggle
        if(!is_break) capslock ^= true;//toggle capslock on press
        return 0;
    }
    if((scan_code == LEFT_SHIFT || scan_code == RIGHT_SHIFT) && !is_extended) {
        //shift enable/disable
        shift = !is_break;
        return 0;
    }

    if(is_break) return 0;
    
    //parse the remaining byte
    char value = lookup_nonextended[scan_code][shift];
    if(value == 0) value = lookup_nonextended[scan_code][0];//if there is no value, try the non-shift version
    if(value == 0) return 0;//still no value, give up

    if(shift || capslock) {
        value = toupper(value);
    }

    return value;

}

void handle_incoming_byte(int) {
    static int expected_number_of_bytes = 1;
    static union BufferData buffer;

    while(read_status_register().output_buffer_status) {
        uint8_t first = blocking_read_data();
        buffer.data = (buffer.data << 8) | first;
        printf("got 0x%x, now have 0x%llx\n", first, buffer.data);
        if(first == 0xE0 || first == 0xF0) {
            //extended code or break code, need at least one more byte
            expected_number_of_bytes++;
        }
        if(first == 0xE1) {
            //pause key is weird
            expected_number_of_bytes = 7;
        }
        expected_number_of_bytes--;

        if(expected_number_of_bytes == 0) {
            //finished one keypress, handle it and reset
            expected_number_of_bytes = 1;
            tty_provide_stdin(parse_full_buffer(buffer));
            buffer.data = 0;
            continue;
        }
    }
}
#elif SCANCODE_VERSION == 2
static char parse_full_buffer(union BufferData buffer) {
    static bool capslock = false;
    static bool shift = false;

    if (buffer.data == 0xE11477E1F014E077) {
        return 0;//pause
    }
    if (buffer.data == 0xE012E07C) {
        return 0;//printscreen
    }
    if (buffer.data == 0xE0F07CE0F012) {
        return 0; //printscreen break
    }
    

    //only 0xF0XX, 0xXX, 0xE0F0XX, 0xE0XX exist

    bool is_break = false, is_extended = false;

    if(buffer.third_byte == 0xE0) {
        //must be 0xE0F0XX
        if(buffer.second_byte != 0xF0) HCF
        is_break = true;
        is_extended = true;
    } else {
        if(buffer.third_byte) HCF // since not 0xE0F0XX, third byte must be 0

        //detect what second byte is used
        switch (buffer.second_byte) {
            case 0xF0:
            is_break = true;
            break;
            
            case 0xE0:
            is_extended = true;
            break;

            case 0:
            break;

            default:
            HCF//second byte wasn't valid
        }
    }

    if(!is_extended && buffer.first_byte == CAPS_LOCK) {
        //capslock toggle
        if(!is_break) capslock ^= true;//toggle capslock on press
        return 0;
    }
    if((buffer.first_byte == LEFT_SHIFT || buffer.first_byte == RIGHT_SHIFT) && !is_extended) {
        //shift enable/disable
        shift = !is_break;
        return 0;
    }

    if(is_break) return 0;

    //parse the remaining byte
    assert(buffer.first_byte < 128);//must be in range

    char value = lookup_nonextended[buffer.first_byte][shift];
    if(value == 0) value = lookup_nonextended[buffer.first_byte][0];//if there is no value, try the non-shift version
    if(value == 0) return 0;//still no value, give up

    if(shift || capslock) {
        value = toupper(value);
    }

    return value;

}

void handle_incoming_byte(int) {
    static int expected_number_of_bytes = 1;
    static union BufferData buffer;

    while(read_status_register().output_buffer_status) {
        uint8_t first = blocking_read_data();
        buffer.data = (buffer.data << 8) | first;
        if(first == 0xE0 || first == 0xF0) {
            //extended code or break code, need at least one more byte
            expected_number_of_bytes++;
        }
        if(first == 0xE1) {
            //pause key is weird
            expected_number_of_bytes = 7;
        }
        expected_number_of_bytes--;

        if(expected_number_of_bytes == 0) {
            //finished one keypress, handle it and reset
            expected_number_of_bytes = 1;
            tty_provide_stdin(parse_full_buffer(buffer));
            buffer.data = 0;
            continue;
        }
    }
}
#endif

void initialise_ps2() {

    //disable devices
    send_command(DISABLE_FIRST_PORT);
    send_command(DISABLE_SECOND_PORT);

    //flush output buffer
    while(read_status_register().output_buffer_status) {
        blocking_read_data();
    }

    //modify controller configuration byte
    send_command(READ_CONTROLLER_CONFIGURATION);
    union ControllerConfiguration config = {.byte= blocking_read_data()};

    //call interrupts
    config.first_ps2_interrupt = 1;
    config.second_ps2_interrupt = 0;
    // otherwise the port is disabled
    config.first_ps2_port_clock = 0;
    config.second_ps2_port_clock = 0;
    //not sure...
    config.first_ps2_translation = 0;

    send_command(WRITE_CONTROLLER_CONFIGURATION);
    blocking_write_data(config.byte);

    //controller self-test
    send_command(TEST_CONTROLLER);
    assert(blocking_read_data() == 0x55);

    //TODO detect if the second PS2 port exists:
    /*To determine if the controller is a dual channel one, send a command 0xA8 to enable the second PS/2 port and read the Controller Configuration Byte (command 0x20). Bit 5 of the Controller Configuration Byte should be clear - if it's set then it can't be a dual channel PS/2 controller, because the second PS/2 port should be enabled. If it is a dual channel device, send a command 0xA7 to disable the second PS/2 port again and clear bits 1 and 5 of the Controller Configuration Byte to disable IRQs and enable the clock for port 2 (You need not worry about disabling translation, because it is never supported by the second port).
    */

    //port self-test
    send_command(TEST_FIRST_PORT);
    if(blocking_read_data()) HCF
    send_command(TEST_SECOND_PORT);
    if(blocking_read_data()) HCF

    //enable ports
    send_command(ENABLE_FIRST_PORT);
    send_command(ENABLE_SECOND_PORT);

    //reset devices
    send_byte_to_first_ps2(KB_RESTART_AND_TEST);
    wait_for_fa_aa();
    while(read_status_register().output_buffer_status) {
        blocking_read_data();
    }

    //set scan code
    send_byte_to_first_ps2(KB_SET_GET_SCAN_CODE);
    assert(blocking_read_data() == 0xFA);
    blocking_write_data(SCANCODE_VERSION);
    assert(blocking_read_data() == 0xFA);
    //check that the scan code was set (broken on laptop due to hardware bug?)
    // send_byte_to_first_ps2(KB_SET_GET_SCAN_CODE);
    // assert(blocking_read_data() == 0xFA);
    // blocking_write_data(0);
    // assert(blocking_read_data() == 0xFA);
    // printf("got ack\n");
    // if(blocking_read_data() != 1) HCF
    // printf("got response\n");

    //enable scanning (maybe)
    send_byte_to_first_ps2(KB_ENABLE_SCANNING);
    assert(blocking_read_data() == 0xFA);

    //ensure port is populated and that the controller has a second port first!
    // send_byte_to_second_ps2(0xFF);
    // wait_for_fa_aa();
    // while(read_status_register().input_buffer_status) {
    //     blocking_read_data();
    // }

    int vector = allocate_free_idt_entry();
    initialise_idt_entry(vector, handle_incoming_byte);
    map_ioapic_interrupt(1, vector);
}
