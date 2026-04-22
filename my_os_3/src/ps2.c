#include "debugging.h"
#include "io.h"
#include "tty.h"
#include "uapi/stdint.h"
#include "kern_libc.h"
#include "ps2.h"

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

//IO COMMAND_PORT
static union StatusRegister read_status_register() {
    return (union StatusRegister) {.byte = in_byte(COMMAND_PORT)};
}
static void send_command(enum ControllerCommand command) {
    if(read_status_register().input_buffer_status) HCF
    out_byte(COMMAND_PORT, command);
}
//IO DATA_PORT
static uint8_t blocking_read_data() {
    while(!read_status_register().output_buffer_status);//wait for full buffer
    return in_byte(DATA_PORT);
}
static void blocking_write_data(uint8_t data) {
    while(read_status_register().input_buffer_status);// wait for empty buffer
    out_byte(DATA_PORT, data);
}

static void send_byte_to_first_ps2(uint8_t data) {
    blocking_write_data(data);
}
static void send_byte_to_second_ps2(uint8_t data) {
    send_command(DIVERT_BYTE_TO_SECOND_PORT);
    blocking_write_data(data);
}

static void wait_for_fa_aa() {
    bool first_fa=false, first_aa=false;
    while(!first_fa || !first_aa) {
        uint8_t input = blocking_read_data();
        if(input == 0xFA) {
            first_fa = true;
        }else if(input == 0xAA) {
            first_aa = true;
        } else HCF
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

//returns NULL on an invalid buffer
static struct KeyEvent parse_full_buffer(union BufferData buffer) {
    static bool capslock = false;
    static bool shift = false;

    if (buffer.data == 0xE1'14'77'E1'F0'14'E0'77) {
        return (struct KeyEvent) {.event_type=KE_PAUSE};
    }
    if (buffer.data == 0xE0'12'E0'7C) {
        return (struct KeyEvent) {.event_type=KE_PRINTSCR};
    }
    if (buffer.data == 0xE0'F0'7C'E0'F0'12) {
        return (struct KeyEvent) {.event_type=KE_PRINTSCR, .is_break=true};
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

    if(!is_extended && buffer.first_byte == 0x58) {
        //capslock toggle
        if(!is_break) capslock ^= true;//toggle capslock on press

        return (struct KeyEvent) {.event_type=KE_CAPS, .is_break=is_break};
    }
    if((buffer.first_byte == 0x12 || buffer.first_byte == 0x59) && !is_extended) {
        //shift enable/disable
        shift = !is_break;

        return (struct KeyEvent) {.event_type=KE_SHIFT, .is_break=is_break};
    }

    //parse the remaining byte
    if(buffer.first_byte >= 128) HCF//must be in range

    char value = lookup_nonextended[buffer.first_byte][shift];
    if(value == 0) value = lookup_nonextended[buffer.first_byte][0];//if there is no value, try the non-shift version
    if(value == 0) return (struct KeyEvent) {.event_type=KE_NULL};//still no value, give up

    if(shift || capslock) {
        value = toupper(value);
    }

    return (struct KeyEvent) {.event_type=KE_ASCII, .character=value, .is_break=is_break};

}

void handle_incoming_byte() {
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

void initialise_ps2() {

    //disable devices
    send_command(DISABLE_FIRST_PORT);
    send_command(DISABLE_SECOND_PORT);

    //flush output buffer
    while(read_status_register().output_buffer_status) {
        blocking_read_data();
    }

    {
        //modify controller configuration byte
        send_command(READ_CONTROLLER_CONFIGURATION);
        union ControllerConfiguration config = {.byte= blocking_read_data()};

        //don't call interrupts
        config.first_ps2_interrupt = 0;
        config.second_ps2_interrupt = 0;
        // otherwise the port is disabled
        config.first_ps2_port_clock = 0;
        config.second_ps2_port_clock = 0;
        //not sure...
        config.first_ps2_translation = 0;

        send_command(WRITE_CONTROLLER_CONFIGURATION);
        blocking_write_data(config.byte);
    }

    //controller self-test
    send_command(TEST_CONTROLLER);
    if(blocking_read_data() != 0x55) HCF

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

    //enable scanning (maybe)
    send_byte_to_first_ps2(KB_ENABLE_SCANNING);
    uint8_t response = blocking_read_data();
    if(response != 0xFA) HCF

    //ensure port is populated and that the controller has a second port first!
    // send_byte_to_second_ps2(0xFF);
    // wait_for_fa_aa();
    // while(read_status_register().input_buffer_status) {
    //     blocking_read_data();
    // }

    //enable ps2 interrupts
    {
        send_command(READ_CONTROLLER_CONFIGURATION);
        union ControllerConfiguration config = {.byte= blocking_read_data()};

        config.first_ps2_interrupt = 1;
        config.second_ps2_interrupt = 0;//TODO uses a different irq

        send_command(WRITE_CONTROLLER_CONFIGURATION);
        blocking_write_data(config.byte);
    }
}
