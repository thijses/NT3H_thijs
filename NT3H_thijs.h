/*

link to NT3H1x01 datasheet (66 page version): https://www.mouser.com/datasheet/2/302/NT3H1101_1201-1127167.pdf
link to NT3H2x11 datasheet (77 page version): https://www.farnell.com/datasheets/2148700.pdf

IMPORTANT NOTE: this is the merger of my NT3H1x01 and NT3H2x11 libraries.
                some of the things i wrote below may only apply to he NT3H1x01, and the password feature documentation for the NT3H2x11 is still TODO


The IC has 2 means of communicating, RF/NFC and I2C.
both the RF interface (the actual NFC functionality) and the I2C interface are all about memory access.
There are 2 memory maps, with some significant overlapping areas, which can change depending on the mode (normal, pass-through, mirror)
for the comprehensive (and suprisingly legible) memory map tables, please look at page 12/13 (RF) and 14/15 (I2C) of the datasheet.
the datasheet talks about blocks (16 bytes) and pages (4 bytes). The I2C interface uses in blocks, the RF interface uses sectors and pages.

The IC has 2 types of memory; EEPROM and SRAM (see page 16 of the datasheet)
by default, the RF interface only has access to the EEPROM, but with Memory-Mirror mode or Pass-Through modes, it can access SRAM as well.
The EEPROM has a limited number of write(/read) cycles, so to keep that in mind
The SRAM has "unlimited" write/read cycles, but is only 64 bytes

The IC has a number of features/contents (which i explain in more detail further below):
- I2C address changing (by NFC host or I2C master)
- Serial number
- Static & Dynamic locking: turns memory to read-only (can only be set/reset by I2C host)
- Capability Container (CC): set (factory-programmed) according to the 'NFC Forum Type 2 Tag specification', used to indicate memory version, memory size and R/W access
- Configuration and Session registers: communication settings. These values are loaded into the session registers on Power-On-Reset (Conf. is stored in EEPROM, Sess. is read-only for RF)
- Pass-through mode: lets the NFC host and I2C master talk (almost) directly (through a 64byte buffer)
- Memory-Mirror mode: lets you map the (64byte) SRAM overtop a part of the EEPROM, to save write(/read) cycles
- Field Detection pin: can be set to react to certain events with a rising and/or falling edge. Pretty nice if you want to save I2C bus time (at the cost of 1 GPIO pin)
- Watchdog timer: in case the I2C master forgets to indicate that it's done using the memory, the WDT will clear the I2C_LOCKED flag (letting the RF access the memory)

The I2C interface of the NT3H is somewhat complex
The I2C address can be changed (only by I2C master) but by default it's:
0b1010101x = 0xAA / 0xAB  including the R/W bit,
0b01010101 = 0x55  in the unshifted (conventional) way.
you can modify the I2C address, but only through the I2C address (so you can have multiple on the same bus, but only after you change their addresses individually).
the I2C clock freq can be 100kHz or 400kHz (not sure about arbetrary values inbetween or below)

Both read and write operations are done in 16 byte (not bit!) blocks, with a MEMory Address (MEMA) byte prefix
meaning that there are 256 blocks of 16 bytes, for a total of 4096 bytes of addressable space.
for (normal) operations, the read/write structure must look like:
 for reads: you send the memory block address, then a stop and start conditions, then read 16 bytes
 for writes:  you send the memory block address, then 16 bytes of data
ONLY the registers block is accessed in the following 1-byte fashion:
 for reads: you send the 0xFE block, then 1 byte (which of the register bytes (0~7) you want), then a stop and start conditions, then read 1 byte
 for writes:  you send the 0xFE block, then 1 byte (which of the register bytes (0~7) you want), then a bitmask for which bits to affect, then the new register byte.
  (the mask byte is nice, cuz then you don't have to read the byte first, you can just exclusively target the settings you want without affecting others)
The IC includes an optional soft-reset feature, if you perform a repeated-start. The default is 0 (presumably that means disabled), so repeated starts don't reset the IC

Communication and arbitration between RF and I²C interface:
both the RF interface (the actual NFC functionality) and the I2C interface are all about memory access.
 However, ONLY 1 interface can have access at-a-time.
So, the I2C code should check whether the RF interface is using the memory, by checking RF_LOCKED bit in the NS_REG register



feature descriptions:

Serial Number (UID):
(the Serial Number bytes are found at (I2C)block:0x00,bytes:0~6  == (RF)sector:0,page:0x00,byte:0~page:0x01,byte:6 )
the serial number is a 7byte value at memory address 0x00~0x07, the first byte of which is always the NXP manufacturer ID, 0x04.
 This first constant byte SHARES a memory address with the I2C address byte, so reads and writes of that byte are entirely unrelated

ATQA and SAK:
(the SAK byte is found at (I2C)block:0x00,byte:7  == (RF)sector:0,page:0x01,byte:3 )
(the ATQA bytes are found at (I2C)block:0x00,bytes:8~9  == (RF)sector:0,page:0x02,bytes:0~1 )
These are some value to help identify the card (in this tag's case only(?) the Serial Number size)
Bits 6 and 7 (starting at 0, not 1) indicate the size of the Serial Number (7 bytes in this tag).
See page 40 of the datasheet for slightly more info (but not a lot... it's not exactly well-documented)

Static Locking (and block-locking):
(the static locking bytes are found at (I2C)block:0x00,bytes:10~11  == (RF)sector:0,page:0x02,bytes:2~3 )
memory space (I2C)block:0x01  == (RF)sector:0,page:0x03~0x0F can be locked as read-only memory using the Static locking bytes. (NOTE: total of 52 bytes)
(the rest of the memory can be locked using Dynamic Locking)
using bits[7:3] of the first static-lock-byte and bits[7:0] of the second byte, memory pages can be locked.
However, the static locking bytes themselves can also be locked ('block-locking'), by bits [2:0] of the first static-lock-byte.
this secondary locking seems a little excessive to me, personally, but whatever.
The RF interface cannot set locking bits back to 0, so Static Locking is a way for the I2C interface to limit the NFC host
For details on which bits block which memory pages, please refer to the helpful infograpic on page 17 of the datasheet

Dynamic Locking (and block-locking):
(the Dynamic locking bytes are found at (I2C)block:0x38/0x78,bytes:0~2  == (RF)sector:1,page:0xE2/0xE0,bytes:0~2)
Dynamic locking is the same as Static locking, just on a larger scale. It also has block-locking, for which the whole 3rd byte is used.
memory space to be locked as read-only memory using the Dynamic locking bytes. (NOTE: total of 840/1856) (datasheet has typo):
  on 1k variant: (I2C)block:0x02~block:0x56,bytes:0~7  == (RF)sector:0,page:0x10~0xE1                   == 210 pages == 52 blocks
  on 2k variant: (I2C)block:0x02~0x77                  == (RF)sector:0,page:0x10~sector:1,page:0xDF     == 464 pages == 116 blocks
(the first few bytes of the memory can be locked using Static Locking)
Usage is also very similar to Static Locking,
 EXCEPT that certain bits "marked with RFUI" must be set to 0 at all times.
Lastly, reading the 3rd byte will always return 0. No reason why, it's just true
For details on which bits block which memory pages, please refer to the helpful infograpic on page 18/19 of the datasheet

Capability Container (CC):  (also mentioned as 'NDEF management' in official NFC tag specification documents)
these are some bytes, defined by the NFC forum to help indicate what this particular tag can/can't do.
byte:0 is an absolute constant 0xE1  (to identify it as an official NFC forum complient card)
byte:1 indicates the version of the NFC specification the tag is built for.
  This particular IC uses version 0x10 (MSB nibble = major version, LSB nibble = minor version, V1.0 == 0b 0001 0000 )
byte:2 indicates the memory size of the tag (roughly?), multiply this byte with 8 to get size in bytes
byte:3 indicates read/write access.
  The MSB nibble is read access: 0x0 is default (unprotected), 0x1~7 are 'reserved for future use', 0x8~E are 'proprietary', 0xF i don't know
  The LSB nibble is write access: 0x_0 is default (unprotected), 0x_1~7 are 'reserved for future use', 0x_8~E are 'proprietary', 0x_F means NO WRITE ACCESS at all
the RF host can set certain bits, but it can't clear them (i think?). The I2C interface has more control (i think.)
There is no real need to mess with these bytes, except for indicating a lack of write-access (by writing 0x0F to the last byte)

Configuration and Session registers:
(the Configuration registers are found at (I2C)block:0x3A/0x7A,bytes:0~7  == (RF)sector:0/1,page:0xE8~0xE9)
(the Session registers are found at (I2C)block:0xFE,bytes:0~7  == (RF)sector:3,page:0xF8~0xF9) AND can only be accessed using a special read/write format (see pages 37~38 of datasheet)
Configuration registers are stored in EEPROM, and loaded into the Session registers at Power-On-Reset
Here is a list of the registers and their functions: {text in curvy{} brackets means ONLY for Session regs.}, [text in square[] brackets means only for Configuration regs.]
- NC_REG: I2C soft reset, Pass-Through mode, FD pin output, Memory-Mirror mode, data transfer direction (in Pass-Through mode) or RF write access (when not in Pass-Through mode)
- LAST_NDEF_BLOCK: address of last block (== 16 bytes == 4 pages) of user-memory that holds actual data. You set this to limit size that the RF host reads. (it basically indicates memory fullness)
- SRAM_MIRROR_BLOCK: address of first block of user-memory to be replaced (mapped over) by SRAM when Mirror-Mode is enabled
- WDT_LS and WDT_MS: watchdog time control register (NOTE: write _LS first??, becuase _MS activates something, i think)
- I2C_CLOCK_STR: I2C clock stretching enable/disable {NOTE: on NT3H2x11 also contains NEG_AUTH_REACHED, indicating whether AUTHLIM is reached}
- [REG_LOCK: disallow RF and/or I2C to write to the configuration bytes, NOTE: these bits BURN IN PERMANENTLY]
- {NS_REG: status flags for: RF done reading, memory arbitration (I2C vs RF), Pass-Through mode buffers filled, EEPROM write error or busy, RF field present}

Pass-Through mode explenation:
(Memory-Mirror must be OFF and requires external VCC)
when pass-through mode is enabled, the RF interface (NFC host) has access to the SRAM instead of(?) the EEPROM
the I2C interface always has access to the SRAM, so (by carefully coordinating reads and writes,) full custom communication between the RF host and I2C master is possible,
 using this IC's SRAM as a buffer inbetween. The SRAM buffer is 16 pages of 4 bytes, a.k.a. 64 bytes, which encompasses the entire SRAM block
using the SRAM requires external VCC (not only power harvesting), and must be re-enabled after every Power-On-Reset
pages 55~57 of the datasheet have some nice visuals for the state of the arbitration bits during pass-through mode.
...

Memory-Mirror mode explenation:
(pass-through must be OFF and requires external VCC)
While this mode is enabled, the SRAM is mapped overtop of a chunk of user-memory.
Bascially, if you want to vary one 64 byte chunk of the EEPROM very often, uses this mode.
The RF interface (and the I2C interface, i think) will interact with the SRAM instead of the EEPROM for a 64 byte chunk, thereby saving you precious EEPROM write(/read) cycles.
you can 'place' this chunk of anywhere in user-memory that you want, using SRAM_MIRROR_BLOCK in the Conf./Sess. registers

Field Detection pin: (they consider it 'active' LOW)
the FD pin can be activated (fall) by:
- RF field present
- start of communication (presumably triggers before anticollision (multiple tags)????)
- this tag being selected (not sure what the difference between this and communication start are...)
- (in Pass-Through mode) data in buffer: ready-to-be-read-by-I2C OR has-been-read-by-RF
and it can be deactivated (rise) by:
- RF field gone
- (RF field gone or) tag is set to HALT state
- (RF field gone or) last block of data has been read (defined by LAST_NDEF_BLOCK)
- (in Pass-Through mode and FD_ON set to indicate data buffer ready) (RF field gone or) data in buffer: has-been-read-by-I2C OR ready-to-be-read-by-RF

Watchdog timer:
(requires external VCC))
When the I2C master is using the memory, the I2C_LOCKED flag is TRUE
HOWEVER, the I2C master should also clear this flag once it's done all it wanted to.
So, in case the I2C master forgets to clear the flag, the WDT will do it for you.
The resolution is 9.43us, and the default value = 0x0848 == 2120*9.43us =~ 20ms
the maximum WDT value is 0xFFFF == 65535*9.43 =~ 618ms
if the WDT is triggered (timer is done), but the I2C interface is still actively communicating, it will clear the flag immediately afterwards

Energy harvesting:
The datasheets claim this IC can typically provide 5mA at 2V on the VOUT pin (when the NFC host is a phone).


the 2kb EEPROM can come from several places
file system (R/W) capable MCUs:
- ESP32
- STM32?
SD card (R/W) capable MCUs:
- all
big ass array (write only) capable MCUs:
- all (except MSP430 has very little ROM)
I'd like to make functions that properly encompass all of these, including:
suspending the WDT while writing such big files,
setting the LAST_NDEF_BLOCK to match the size of the loaded contents,


TO check out:
- BLE pairing through NFC (how does it work, can this IC do it, etc.)
- responds to I2C general call?
- does checking the SRAM_xxx_READY flags clear them?
- FD pin start of comm vs tag selection
- soft reset function

MUST WRITE for 2x11:
- password documentation above  //// NOTE: I2C_PROT does not apply to: Session registers, SRAM and 'configuration pages' including PWD config, but dependent on REG_LOCK_I2C
- update/remove memory locations in documentation above

TODO (general):
- _blockStruct for _oneBlockBuff, and some static casting maybe?
- _blockStruct get functions!
- find proper CC bytes for NT3H2211 (indicate size correctly) (2x11 datasheet only mentions 0x6D )
- expand the uses for _blockStruct, as i quite like it (personally)
- TEST: password stuff! (can I2C even do a PASS_AUTH attempt on its own? (or does it need an NFC reader to do it))
- TEST: full block read/writes (skip _oneBlockBuff)
- TEST: useCache (also, did i forget to apply it anywhere?)
- HW testing (STM32)
- HW testing (ESP32)
- HW testing (MSP430)
- HW testing (328p)
- continue writing missing functions (see list below)
- check Wire.h function return values for I2C errors (for all microcontrollers, to see what values are consistant)
- test if 'static' vars in the ESP32 functions actually are static (connect 2 sensors?)
- consider changing to more clearly named NS_REG get macro function names. e.g. SRAM_I2C_READY could be PTHRU_IN (because it indicates incoming data when in Pass-Through mode)
- generalized memory map struct (also for other libraries). Could just be an enum, i just don't love #define

TO write (specific functions):
- printConfig() (print contents of Session registers, LAST_NDEF_BLOCK, etc. in a LEGIBLE fashion)
- (arbetrary file writing funtion (mostly setting LAST_NDEF_BLOCK to indicate the size))
- block writing/reading to/from file/flash/EEPROM/idk   (2kB of data coming from somewhere, going to somewhere!)
- static lock functions (_setStaticLockBits, staticLockPage, _staticBlockLockPageToChunk, staticBlockLockChunks)
- dynamic lock functions (_setDynamicLockBits, _dynamicLockPageToChunk, dynamicLockChunks, _dynamicBlockLockPageToChunk, dynamicBlockLockChunks)
- generalized lock function: lockArea(startBlock,endBlock,roundUp=true)
- can_I_update_password() function (check AUTH0 and other shit), once i know what makes the password Write-safe
- Memory-Mirror mode functions
- Pass-Through mode functions
- check ATQA bytes to verify UID size (find what ATQA and SAK are defined as in NFC spec)


*/


#ifndef NT3H_thijs_h
#define NT3H_thijs_h

#include "Arduino.h"


//#define NT3H_unlock_burning   // enable the permanent chip-burning features of the NT3H (e.g. REG_LOCK)

//#define NT3HdebugPrint(x)  Serial.println(x)
//#define NT3HdebugPrint(x)  log_d(x)   //using the ESP32 debug printing

#ifndef NT3HdebugPrint
  #define NT3HdebugPrint(x)  ;
#endif


//// NT3H constants:

#define NT3H_DEFAULT_I2C_ADDRESS 0x55

#define NT3H_BLOCK_SIZE 16 // the I2C interface only deals in blocks of 16 bytes (except for Session registers)

#define NT3H_I2C_ADDR_CHANGE_MEMA 0x00      // I2C address change memory block
#define NT3H_I2C_ADDR_CHANGE_MEMA_BYTE  0   // I2C address change covers byte 0 only (write only)

#define NT3H_SERIAL_NR_MEMA 0x00            // Serial Number memory block
#define NT3H_SERIAL_NR_MEMA_BYTES_START 0   // Serial Number covers bytes 0~6 (read only)
#define NT3H_SERIAL_NR_NXP_MF_ID 0x04       // Serial Number manufacturer number (of NXP). Should be the first byte of UID on all NXP tags

#define NT3H_SAK_MEMA 0x00                  // SAK memory block
#define NT3H_SAK_MEMA_BYTE         7        // SAK covers byte 7 (read only)
#define NT3H_ATQA_MEMA 0x00                 // ATQA memory block
#define NT3H_ATQA_MEMA_BYTES_START 8        // ATQA covers bytes 8~9 (read only)

#define NT3H_CAPA_CONT_MEMA 0x00            // Capability Container memory block
#define NT3H_CAPA_CONT_MEMA_BYTES_START 12  // Capability Container covers bytes 12~15
static const uint32_t NT3H_CAPA_CONT_DEFAULT_uint32_t[2] = {0xE1106D00, 0xE110EA00};  // Capability Container factory default value (as uint32_t) for the {1k, 2k} variants
static const uint8_t  NT3H_CAPA_CONT_DEFAULT[2][4] = {{0xE1,0x10,0x6D,0x00}, {0xE1,0x10,0xEA,0x00}}; // Capability Container factory default value (as individual bytes) for the {1k, 2k} variant
//// NOTE: Capability Container contents might be different for 1x01 and 2x11 versions, not sure. 1k variants appear to be the same, 2k TBD

#define NT3H_STAT_LOCK_MEMA 0x00            // Static Locking bytes memory block
#define NT3H_STAT_LOCK_MEMA_BYTES_START 10  // Static Locking covers bytes 10~11

#define NT3Hxxx1_DYNA_LOCK_MEMA 0x38        // Dynamic Locking bytes memory block (NOTE: 1x01 to 2x11 change)
#define NT3H1201_DYNA_LOCK_MEMA 0x78        // Dynamic Locking bytes memory block for the NT3H1201 only
#define NT3Hx101_DYNA_LOCK_RFUI_bits 0xFF0F3F00   // Dynamic Locking RFUI mask for the 1k variants (RFU = Reserved for Future Use)
#define NT3H1201_DYNA_LOCK_RFUI_bits 0xFF7FFF00   // Dynamic Locking RFUI mask for the NT3H1201 only
#define NT3H2211_DYNA_LOCK_RFUI_bits 0xFFFFFF00   // Dynamic Locking RFUI mask for the NT3H2211 only (NOTE: 1x01 to 2x11 change)

//// password stuff (NT3H2x11 only): (NOTE: 1x01 to 2x11 change)
#define NT3H2x11_AUTH0_MEMA     0x38            // AUTH0 byte memory block
#define NT3H2x11_AUTH0_MEMA_BYTE            15  // AUTH0 covers byte 15
#define NT3H2x11_AUTH0_DISABLE_THRESH 0xEB // if AUTH0 is <= this value, then the password protection is active (i think, haven't tested...)
#define NT3H2x11_ACCESS_MEMA    0x39            // ACCESS byte memory block
#define NT3H2x11_ACCESS_MEMA_BYTE           0   // ACCESS covers byte 0
#define NT3H2x11_PWD_MEMA       0x39            // PWD (password) memory block
#define NT3H2x11_PWD_MEMA_BYTES_START       4   // PWD covers bytes 4~7 (32bit number) (write only)
#define NT3H2x11_PACK_MEMA      0x39            // PACK (pwd acknowledge) memory block
#define NT3H2x11_PACK_MEMA_BYTES_START      8   // PACK covers bytes 8~9 (16bit ack thingy) (write only)
#define NT3H2x11_PT_I2C_MEMA    0x39            // PT_I2C memory block
#define NT3H2x11_PT_I2C_MEMA_BYTE           12  // PT_I2C covers byte 12


#define NT3Hxxx1_CONF_REGS_MEMA 0x3A            // Configuration registers memory block (NOTE: 1x01 to 2x11 change)
#define NT3H1201_CONF_REGS_MEMA 0x7A            // Configuration registers memory block for the 2k variant
#define NT3H_SESS_REGS_MEMA 0xFE            // Session registers memory block (NOTE: can only be accessed using special read/write operation, see pages 37~38 of 1x01 datasheet)

#define NT3H_INVALID_MEMA   0xFF // ther are several invalid memory block addresses, but this is the most recognisable one
// other invalid addresses include: 0x3B~(0xF7@1k~0x3F@2k), (0x80@2k~0xF7@2k), 0xFC~0xFD

enum NT3H_CONF_SESS_REGS_ENUM : uint8_t { // you could also do this with just defines, but it's slightly fancier this way
//// configuration/session registers common:
  NT3H_COMN_REGS_NC_REG_BYTE = 0,  // NC_REG register location in the conf/sess. registers
  NT3H_COMN_REGS_LAST_NDEF_BLOCK_BYTE = 1,  // LAST_NDEF_BLOCK register location in the conf/sess. registers
  NT3H_COMN_REGS_SRAM_MIRROR_BLOCK_BYTE = 2,  // SRAM_MIRROR_BLOCK register location in the conf/sess. registers
  NT3H_COMN_REGS_WDT_LS_BYTE = 3,  // WDT_LS register location in the conf/sess. registers
  NT3H_COMN_REGS_WDT_MS_BYTE = 4,  // WDT_MS register location in the conf/sess. registers
  NT3H_COMN_REGS_I2C_CLOCK_STR_BYTE = 5,  // I2C_CLOCK_STR register location in the conf/sess. registers
//// configuration registers only:
  NT3H_CONF_REGS_REG_LOCK_BYTE = 6,  // REG_LOCK register location in the conf/sess. registers
//// session registers only:
  NT3H_SESS_REGS_NS_REG_BYTE = 6  // NS_REG register location in the conf/sess. registers
};

//// default values of Configuration and Session registers, as defined in the datasheets (page 23~27 of 1x01, 27~29 of 2x11):
static const uint8_t NT3H_CONF_REGS_DEFAULT[7] = {0x01, 0x00, 0xF8, 0x48,
                                                      0x08, 0x01, 0x00}; // the 8th byte is a fixed 0
static const uint8_t NT3H_SESS_REGS_DEFAULT[7] = {0x01, 0x00, 0xF8, 0x48,
                                                      0x08, 0x01, 0x00};

static const float NT3H_WDT_RAW_TO_MICROSECONDS = 9.43; // multiply the (uint16_t) WDT regsiters value with this to get microseconds

enum NT3H_FD_ON_ENUM : uint8_t { // 2bit value to determine what triggers the FD pin to activate (sink LOW)
  NT3H_FD_ON_FIELD_PRESENCE = 0, // RF field present
  NT3H_FD_ON_START_OF_COMM  = 1, // start of communication (presumably triggers before anticollision (multiple tags)????)
  NT3H_FD_ON_TAG_SELECTED   = 2, // this tag being selected (not sure what the difference between this and communication start are...)
  NT3H_FD_ON_PTHRU_MODE     = 3  // (in Pass-Through mode) data in buffer: ready-to-be-read-by-I2C OR has-been-read-by-RF
};
enum NT3H_FD_OFF_ENUM : uint8_t { // 2bit value to determine what triggers the FD pin to deactivate (pull HIGH)
  NT3H_FD_OFF_FIELD_PRESENCE = 0, // RF field gone
  NT3H_FD_OFF_HALT           = 1, // (RF field gone or) tag is set to HALT state
  NT3H_FD_OFF_LAST_NDEF_READ = 2, // (RF field gone or) last block of data has been read (defined by LAST_NDEF_BLOCK)
  NT3H_FD_OFF_PTHRU_MODE     = 3  // (in Pass-Through mode and FD_ON set to NT3H_FD_ON_PTHRU_MODE) (RF field gone or) data in buffer: has-been-read-by-I2C OR ready-to-be-read-by-RF
};

//// NC_REG common:
#define NT3H_NC_REG_SIL_SRST_bits   0b10000000 // NFCS_I2C_RST_ON_OFF enables (NFC-silence (NT3H2x11 only) AND) soft-reset-through-repeated-I2C-starts (very cool, slightly niche)
#define NT3H_NC_REG_PTHRU_bits      0b01000000 // PTHRU_ON_OFF enables Pass-Through mode
#define NT3H_NC_REG_FD_OFF_bits     0b00110000 // FD_OFF determines the behaviour of the FD pin (falling)
#define NT3H_NC_REG_FD_ON_bits      0b00001100 // FD_ON determines the behaviour of the FD pin (rising)
#define NT3H_NC_REG_MIRROR_bits     0b00000010 // SRAM_MIRROR_ON_OFF enables Memory-Mirror mode
#define NT3H_NC_REG_DIR_bits        0b00000001 // TRANSFER_DIR/PTHRU_DIR determines the direction of data in Pass-Through mode, or can disable RF write-access otherwise
//// NOTE: NT3H1x01 had RFUs for NT3H_NC_REG_PTHRU_bits and NT3H_NC_REG_MIRROR_bits in conf registers.
////       Also, in the Nt3H2x11 those bits reset to 0 in the session registers under certain circumstances. See page 29 of 2x11 datasheet
#define NT3H1x01_NC_REG_RFU_bits    0b01000010 // = (NT3H1x01_NC_REG_PTHRU_bits | NT3H1x01_NC_REG_MIRROR_bits) // in NC_REG of Configuration register, for the NT3H1x01 these 2 bits must be kept as 0
//// REG_LOCK (Configuration only): (there are one-time-program bits, a.k.a. burn bits, and can be set to 1 ONLY ONCE, disabling the changing of the Configuration bytes forever)
#define NT3H_NC_REG_LOCK_I2C_bits   0b00000010 // PTHRU_ON_OFF enables Pass-Through mode
#define NT3H_NC_REG_LOCK_RF_bits    0b00000001 // SRAM_MIRROR_ON_OFF enables Memory-Mirror mode
//// NS_REG (Session only):
#define NT3H_NS_REG_NDEF_READ_bits  0b10000000 // NDEF_DATA_READ flag is 1 once the RF interface has read the data at address LAST_NDEF_BLOCK (if set). Reading clears flag
#define NT3H_NS_REG_I2C_LOCKED_bits 0b01000000 // I2C_LOCKED is 1 if I2C has control of memory (arbitration). Should be cleared once the I2C interaction is completely done, may be cleared by WDT
#define NT3H_NS_REG_RF_LOCKED_bits  0b00100000 // RF_LOCKED is 1 if RF has control of memory (arbitration)
#define NT3H_NS_REG_PTHRU_IN_bits   0b00010000 // SRAM_I2C_READY is 1 if data is ready in SRAM buffer to be READ by I2C (i'm not sure if checking this flag changes it)
#define NT3H_NS_REG_PTHRU_OUT_bits  0b00001000 // SRAM_RF_READY is 1 if data is ready in SRAM buffer to be READ by RF (the I2C should not need to check this flag, and i'm not sure if checking it clears it)
#define NT3H_NS_REG_EPR_WR_ERR_bits 0b00000100 // EEPROM_WR_ERR is 1 if there was a (High Voltage?) error during EEPROM write. Flag needs to be manually cleared
#define NT3H_NS_REG_EPR_WR_BSY_bits 0b00000010 // EEPROM_WR_BUSY is 1 if EEPROM writing is in progress (access is disabled while writing)
#define NT3H_NS_REG_RF_FIELD_bits   0b00000001 // RF_FIELD_PRESENT is 1 if an RF field is detected

//// password stuff (NT3H2x11 only): (NOTE: 1x01 to 2x11 change)
//// I2C_CLOCK_STR byte (Session only):
#define NT3H2x11_NEG_AUTH_REACHED_bits    0b00000010 // NEG_AUTH_REACHED is a bit in the I2C_CLOCK_STR byte, only in the session registers, indicating whether the AUTHLIM has been reached
//// ACCESS byte:
#define NT3H2x11_ACCESS_NFC_PROT_bits     0b10000000 // NFC_PROT determines whether read-access is ALSO password protected (instead of only write-access) (from NFC)
#define NT3H2x11_ACCESS_NFC_DIS_SEC1_bits 0b00100000 // NFC_DIS_SEC1 disables sector 1 (NFC), effectively making the 2k IC the same as the 1k
#define NT3H2x11_ACCESS_AUTHLIM_bits      0b00000111 // AUTHLIM limits the number of failed password auth attempts. (3 bit value, 0 = disabled, otherwise max_attemps = 2^(bits) = 1<<bits)
//// PT_I2C byte:
#define NT3H2x11_PT_I2C_2K_PROT_bits      0b00001000 // 2K_PROT enables password protection for sector 1 (only on 2k version)
#define NT3H2x11_PT_I2C_SRAM_PROT_bits    0b00000100 // SRAM_PROT enables password protection for SRAM (for pass-through and mirror mode) (does not apply to I2C?)
#define NT3H2x11_PT_I2C_I2C_PROT_bits     0b00000011 // I2C_PROT determines R/W access to protected areas from I2C. 0=no_restrictions, 1=R_only, 2|3=no_access
//// NOTE: I2C_PROT does not apply to: Session registers, SRAM and 'configuration pages' including PWD config, but dependent on REG_LOCK_I2C

//// you could also store whole blocks in a dedicated struct, and only send them once they're fully ready
//// the main advantage of these blocks over my existing/old useCache system, is that the blockStruct uses memory references pretty efficiently (see example code)
// template<uint8_t arraySize> // template not needed, as all blocks are the same size; NT3H_BLOCK_SIZE
struct _blockStruct { // a nice, consistant packet structure
  uint8_t _data[NT3H_BLOCK_SIZE];
  _blockStruct() { for(uint8_t i=0; i<NT3H_BLOCK_SIZE; i++) { _data[i] = 0; } } // initalize to 0s
  // _blockStruct(uint8_t* _dataInput) {for(uint8_t i=0;i<sizeof(_data);i++){_data[i]=_dataInput[i];}}
  // _blockStruct(uint8_t* _dataInput) { _data = _dataInput; }
  inline uint8_t* operator&() {return(_data);} //any attempts to retrieve the address of this object should be met with a byte pointer to _data (it's the same address, just an implicit typecast)
  inline uint8_t& operator[](size_t index) {return(_data[index]);} //i'm hoping the compiler will just know what i mean
};
// struct blockZero : public _blockStruct { // a struct specifically for the block at MEMA 0x00, which includes the I2C_addr, UID, static_lock and CC)
//// TODO: make struct
// };
// struct confRegBlock : public _blockStruct { // a struct spefically for the Configuration Register block
//   inline uint8_t& NC_REG() {return(_data[NT3H_COMN_REGS_NC_REG_BYTE]);} // the (whole) NC_REG byte
//   inline uint8_t& LAST_NDEF_BLOCK() {return(_data[NT3H_COMN_REGS_LAST_NDEF_BLOCK_BYTE]);} // the SRAM_MIRROR_BLOCK byte
//   inline uint8_t& SRAM_MIRROR_BLOCK() {return(_data[NT3H_COMN_REGS_SRAM_MIRROR_BLOCK_BYTE]);} // the SRAM_MIRROR_BLOCK byte
//   inline uint8_t& WDTraw(uint8_t index) {return(_data[index + NT3H_COMN_REGS_WDT_LS_BYTE]);} // the WatchDogTimer bytes, raw bytes
//   inline uint16_t& WDT_uint16_t() {return((uint16_t&)_data[NT3H_COMN_REGS_WDT_LS_BYTE]);} // the WatchDogTimer as a uint16_t
//   inline uint8_t& I2C_CLOCK_STR() {return(_data[NT3H_COMN_REGS_I2C_CLOCK_STR_BYTE]);} // the I2C_CLOCK_STR byte
//   #ifdef NT3H_unlock_burning
//     inline uint8_t& REG_LOCK() {return(_data[NT3H_CONF_REGS_REG_LOCK_BYTE]);} // the (whole) REG_LOCK byte
//   #endif
//   //// i didn't make a constructor for this one, as there are simply too many one-bit options in NC_REG, and because REG_LOCK is frightful
//   //// set bit functions:
//   void setNC_FD_OFF(NT3H_FD_OFF_ENUM newVal) { NC_REG() &= ~NT3H_NC_REG_FD_OFF_bits; NC_REG() |= ((static_cast<uint8_t>(newVal) << 4) & NT3H_NC_REG_FD_OFF_bits); }
//   void setNC_FD_ON(NT3H_FD_ON_ENUM newVal) { NC_REG() &= ~NT3H_NC_REG_FD_ON_bits; NC_REG() |= ((static_cast<uint8_t>(newVal) << 2) & NT3H_NC_REG_FD_ON_bits); }
//   void setNC_NFCS_I2C_RST(bool newVal) { NC_REG() &= ~NT3H_NC_REG_SIL_SRST_bits; NC_REG() |= (newVal ? NT3H_NC_REG_SIL_SRST_bits : 0); }
//   void setNC_PTHRU(bool newVal) { NC_REG() &= ~NT3H_NC_REG_PTHRU_bits; NC_REG() |= (newVal ? NT3H_NC_REG_PTHRU_bits : 0); }
//   void setNC_MIRROR(bool newVal) { NC_REG() &= ~NT3H_NC_REG_MIRROR_bits; NC_REG() |= (newVal ? NT3H_NC_REG_MIRROR_bits : 0); }
//   void setNC_DIR(bool newVal) { NC_REG() &= ~NT3H_NC_REG_DIR_bits; NC_REG() |= (newVal ? NT3H_NC_REG_DIR_bits : 0); }
//   void setWDT_float(float newVal) { WDT_uint16_t() = constrain(newVal,0,(((float)0xFFFF)*NT3H_WDT_RAW_TO_MICROSECONDS)) / NT3H_WDT_RAW_TO_MICROSECONDS; } // the WatchDogTimer as a float
//   #ifdef NT3H_unlock_burning
//     void burnRegLockI2C() { REG_LOCK() |= NT3H_NC_REG_LOCK_I2C_bits; }
//     void burnRegLockRF() { REG_LOCK() |= NT3H_NC_REG_LOCK_RF_bits; }
//   #endif
//   //// get bit functions:
//   NT3H_FD_OFF_ENUM getNC_FD_OFF() { return(static_cast<NT3H_FD_OFF_ENUM>((NC_REG() & NT3H_NC_REG_FD_OFF_bits) >> 4)); }
//   NT3H_FD_ON_ENUM getNC_FD_ON() { return(static_cast<NT3H_FD_ON_ENUM>((NC_REG() & NT3H_NC_REG_FD_ON_bits) >> 2)); }
//   bool getNC_NFCS_I2C_RST() { return((NC_REG() & NT3H_NC_REG_SIL_SRST_bits) != 0); }
//   bool getNC_PTHRU() { return((NC_REG() & NT3H_NC_REG_PTHRU_bits) != 0); }
//   bool getNC_MIRROR() { return((NC_REG() & NT3H_NC_REG_MIRROR_bits) != 0); }
//   bool getNC_DIR() { return((NC_REG() & NT3H_NC_REG_DIR_bits) != 0); }
//   float getWDT_float() {return(((float)WDT_uint16_t()) * NT3H_WDT_RAW_TO_MICROSECONDS); } // the WatchDogTimer as a float
// };
struct passwordBlock : public _blockStruct { // (NT3H2x11 only) a struct specifically for the password block, just to make sure it's all written correctly (and you don't lock yourself out)
  inline uint8_t& ACCESS() {return(_data[NT3H2x11_ACCESS_MEMA_BYTE]);} // the (whole) ACCESS byte
  inline uint8_t& PWD(uint8_t index) {return(_data[index + NT3H2x11_PWD_MEMA_BYTES_START]);} // the password as bytes
  inline uint32_t& PWD_uint32_t() {return((uint32_t&)_data[NT3H2x11_PWD_MEMA_BYTES_START]);} // the password as a uint32_t (little-endian-ness ensures the LSByte order required)
  inline uint8_t& PACK(uint8_t index) {return(_data[index + NT3H2x11_PACK_MEMA_BYTES_START]);} // the PACK (password acknowledge) as bytes
  inline uint16_t& PACK_uint16_t() {return((uint16_t&)_data[NT3H2x11_PACK_MEMA_BYTES_START]);} // the PACK (password acknowledge) as a uint16_t (little-endian-ness ensures the LSByte order required)
  inline uint8_t& PT_I2C() {return(_data[NT3H2x11_PT_I2C_MEMA_BYTE]);} // the (whole) PT_I2C byte
  passwordBlock() = default; // should call _blockStruct::_blockStruct()
  passwordBlock(uint8_t ACCESS_in, uint32_t PWD_in, uint16_t PACK_in, uint8_t PI_I2C_in) // constructor with values
  { for(uint8_t i=0;i<NT3H_BLOCK_SIZE;i++){_data[i]=0;}  ACCESS() = ACCESS_in;  PWD_uint32_t() = PWD_in;  PACK_uint16_t() = PACK_in;  PT_I2C() = PI_I2C_in; } // (just a macro)
  passwordBlock(uint8_t ACCESS_in, uint8_t PWD_buff_in[], uint8_t PACK_buff_in[], uint8_t PI_I2C_in) // constructor with byte arrays
  { for(uint8_t i=0;i<NT3H_BLOCK_SIZE;i++){_data[i]=0;}  ACCESS() = ACCESS_in;  for(uint8_t i=0;i<4;i++){PWD(i)=PWD_buff_in[i];}  for(uint8_t i=0;i<2;i++){PACK(i)=PACK_buff_in[i];}  PT_I2C() = PI_I2C_in; }
  //// set bit functions:
  void setACCESS_NFC_PROT(bool newVal) { ACCESS() &= ~NT3H2x11_ACCESS_NFC_PROT_bits; ACCESS() |= (newVal ? NT3H2x11_ACCESS_NFC_PROT_bits : 0); }
  void setACCESS_NFC_DIS_SEC1(bool newVal) { ACCESS() &= ~NT3H2x11_ACCESS_NFC_DIS_SEC1_bits; ACCESS() |= (newVal ? NT3H2x11_ACCESS_NFC_DIS_SEC1_bits : 0); }
  void setACCESS_AUTHLIMraw(uint8_t newVal) { ACCESS() &= ~NT3H2x11_ACCESS_AUTHLIM_bits; ACCESS() |= (newVal & NT3H2x11_ACCESS_AUTHLIM_bits); }
  void setPT_I2C_2K_PROT(bool newVal) { PT_I2C() &= ~NT3H2x11_PT_I2C_2K_PROT_bits; PT_I2C() |= (newVal ? NT3H2x11_PT_I2C_2K_PROT_bits : 0); }
  void setPT_I2C_SRAM_PROT(bool newVal) { PT_I2C() &= ~NT3H2x11_PT_I2C_SRAM_PROT_bits; PT_I2C() |= (newVal ? NT3H2x11_PT_I2C_SRAM_PROT_bits : 0); }
  void setPT_I2C_I2C_PROT(uint8_t newVal) { PT_I2C() &= ~NT3H2x11_PT_I2C_I2C_PROT_bits; PT_I2C() |= (newVal & NT3H2x11_PT_I2C_I2C_PROT_bits); }
  //// get bit functions:
  bool getACCESS_NFC_PROT() { return((ACCESS() & NT3H2x11_ACCESS_NFC_PROT_bits) != 0); }
  bool getACCESS_NFC_DIS_SEC1() { return((ACCESS() & NT3H2x11_ACCESS_NFC_DIS_SEC1_bits) != 0); }
  uint8_t getACCESS_AUTHLIMraw() { return(ACCESS() & NT3H2x11_ACCESS_AUTHLIM_bits); }
  uint8_t getACCESS_AUTHLIM() { uint8_t AUTHLIMraw=getACCESS_AUTHLIMraw(); return(AUTHLIMraw ? (1<<AUTHLIMraw) : 0); }
  bool getPT_I2C_2K_PROT(bool newVal) { return((PT_I2C() & NT3H2x11_PT_I2C_2K_PROT_bits) != 0); }
  bool getPT_I2C_SRAM_PROT(bool newVal) { return((PT_I2C() & NT3H2x11_PT_I2C_SRAM_PROT_bits) != 0); }
  uint8_t getPT_I2C_I2C_PROT(uint8_t newVal) { return(PT_I2C() & NT3H2x11_PT_I2C_I2C_PROT_bits); }
};



#include "_NT3H_thijs_base.h" // this file holds all the nitty-gritty low-level stuff (I2C implementations (platform optimizations))
/**
 * An I2C interfacing library for the NT3H NFC IC
 * 
 */
class NT3H_thijs : public _NT3H_thijs_base
{
  public:
  //private:
  uint8_t _oneBlockBuff[NT3H_BLOCK_SIZE]; // used for user-friendly functions. A cache of 1 block of memory, to be used whenever less-than-a-whole-block is to be changed (less memory assignment overhead)
  // _blockStruct _oneBlockBuff; // used for user-friendly functions. A cache of 1 block of memory, to be used whenever less-than-a-whole-block is to be changed (less memory assignment overhead)
  uint8_t _oneBlockBuffAddress = NT3H_INVALID_MEMA; // indicates the memory address the _oneBlockBuff stores. Use with great caution, and only if speed is an absolute necessity!
  uint8_t _passwordAndPackBuff[6] = {0}; // (NT3H2x11 only) the password and PACK (password acknowledge) cannot be Read, so overwriting anything in block 0x39 will overwrite the password and PACK
  bool _passwordAndPackStored = false; // (NT3H2x11 only) useful for generating debug print messages (in case you're overwriting a password unintentionally)
  public:
  using _NT3H_thijs_base::_NT3H_thijs_base;
  /*
  This class only contains the higher level functions.
   for the base functions, please refer to _NT3H_thijs_base.h
  here is a brief list of all the lower-level functions:
  - init()
  - requestMemBlock()
  - requestSessRegByte()
  - _onlyReadBytes()
  - writeMemBlock()
  - writeSessRegByte()
  */
  //// the following functions are abstract enough that they'll work for either architecture
  
  /**
   * (just a macro) check whether an NT3H_ERR_RETURN_TYPE (which may be one of several different types) is fine or not 
   * @param err (bool or esp_err_t or i2c_status_e, see on defines at top)
   * @return whether the error is fine
   */
  bool _errGood(NT3H_ERR_RETURN_TYPE err) { return(err == NT3H_ERR_RETURN_TYPE_OK); }
  //   #if defined(NT3H_return_esp_err_t)     // these few commented lines were replaced with the one above, but still serve to show how the error system works:
  //     return(err == ESP_OK);
  //   #elif defined(NT3H_return_i2c_status_e)
  //     return(err == I2C_OK);
  //   #else
  //     return(err);
  //   #endif
  // }
  
  // I'd love to template these _get and _set functions with some kind of pre-processor directive that makes the debugPrint message include the name of the individual function (efficiently)
  // but alas, i have yet to determine how one would do such a thing (in an even remotely legible way)
  /**
   * (private) retrieve an arbetrary set of bytes (into a provided buffer) from a block
   * @param blockAddress MEMory Address (MEMA) of the block
   * @param bytesInBlockStart where in the block the relevant data starts (see defines up top)
   * @param bytesToRead how many bytes are of interest (size of readBuff)
   * @param readBuff buffer of size (bytesToRead) to put the results in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE _getBytesFromBlock(uint8_t blockAddress, uint8_t bytesInBlockStart, uint8_t bytesToRead, uint8_t readBuff[], bool useCache=false) {
    if((bytesInBlockStart+bytesToRead) > NT3H_BLOCK_SIZE) { NT3HdebugPrint("_getBytesFromBlock() MISUSE!, you're trying to read bytes outside of the buffer"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
        // bytesInBlockStart=(bytesInBlockStart%NT3H_BLOCK_SIZE); bytesToRead=min((uint8_t)(NT3H_BLOCK_SIZE-bytesInBlockStart), bytesToRead); } // safety measures instead of return()
    uint8_t* whichBuffToUse = _oneBlockBuff; // default to class member block
    if((bytesToRead == NT3H_BLOCK_SIZE) && (bytesInBlockStart == 0)) { whichBuffToUse = readBuff; } // ONLY IF readBuff is actually a full block's worth of data, then you can read directly to it (skip copying)
    NT3H_ERR_RETURN_TYPE err = NT3H_ERR_RETURN_TYPE_OK;
    if( ! (useCache && (blockAddress == _oneBlockBuffAddress) && (whichBuffToUse == _oneBlockBuff))) { // normally true
      err = requestMemBlock(blockAddress, whichBuffToUse); // fetch the whole block
      if(whichBuffToUse == _oneBlockBuff) { _oneBlockBuffAddress = blockAddress; } // remember which block is in the chache for later
      if(!_errGood(err)) { NT3HdebugPrint("_getBytesFromBlock() read/write error!"); _oneBlockBuffAddress = NT3H_INVALID_MEMA; return(err); }
    }
    if(whichBuffToUse == _oneBlockBuff) { for(uint8_t i=0; i<bytesToRead; i++) { readBuff[i] = _oneBlockBuff[i+bytesInBlockStart]; } } // copy data (if readBuff does not contain whole block)
    return(err); // err should always be OK, if it makes it to this point
  }
  /**
   * (private) retrieve an arbetrary set of bytes from a block and force into a (little-endian) value of class T
   * @tparam T type of data to retrieve
   * @param blockAddress MEMory Address (MEMA) of the block
   * @param bytesInBlockStart where in the block the relevant data starts (see defines up top)
   * @param readMSBfirst whether to read the MSByte first or the LSByte first (Big/Little-endian respectively)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return value (or garbage, depending on the unkowable success of the write/read)
   */
  template<typename T> 
  T _getValFromBlock(uint8_t blockAddress, uint8_t bytesInBlockStart, bool readMSBfirst=true, bool useCache=false) {
    T returnVal;
    if((bytesInBlockStart+sizeof(T)) > NT3H_BLOCK_SIZE) { NT3HdebugPrint("_getValFromBlock() MISUSE!, you're trying to read bytes outside of the buffer"); return(NT3H_ERR_RETURN_TYPE_FAIL);  }
    NT3H_ERR_RETURN_TYPE err = NT3H_ERR_RETURN_TYPE_OK;
    if( ! (useCache && (blockAddress == _oneBlockBuffAddress))) { // normally true
      err = requestMemBlock(blockAddress, _oneBlockBuff); _oneBlockBuffAddress = blockAddress; // fetch the whole block
      if(!_errGood(err)) { NT3HdebugPrint("_getValFromBlock<>() read/write error!"); _oneBlockBuffAddress = NT3H_INVALID_MEMA; return(returnVal); } // note: returns random value, as returnVal was not zero-initialized
    }
    uint8_t* bytePtrToReturnVal = (uint8_t*) &returnVal;
    for(uint8_t i=0; i<sizeof(T); i++) { bytePtrToReturnVal[readMSBfirst ? (sizeof(T)-1-i) : i] = _oneBlockBuff[i+bytesInBlockStart]; } // (little-endian)
    return(returnVal);
  }

  /**
   * (private) overwrite an arbetrary set of bytes in a block (with bytes from a provided buffer)
   * @param blockAddress MEMory Address (MEMA) of the block
   * @param bytesInBlockStart where in the block the relevant data starts (see defines up top)
   * @param bytesToWrite how many bytes are of interest (size of writeBuff)
   * @param writeBuff buffer of size (bytesToRead) to write into block
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE _setBytesInBlock(uint8_t blockAddress, uint8_t bytesInBlockStart, uint8_t bytesToWrite, uint8_t writeBuff[], bool useCache=false) {
    uint8_t* whichBuffToUse = _oneBlockBuff; // default to class member block
    if((bytesToWrite == NT3H_BLOCK_SIZE) && (bytesInBlockStart == 0)) { whichBuffToUse = writeBuff; } // ONLY IF readBuff is actually a full block's worth of data, then you can read directly to it (skip copying)
    NT3H_ERR_RETURN_TYPE err = NT3H_ERR_RETURN_TYPE_OK;
    if(whichBuffToUse == _oneBlockBuff) { // if the writeBuff is not an entire block worth of data, you must first read the block (to fill in the gaps when you send it back)
      if( ! (useCache && (blockAddress == _oneBlockBuffAddress))) { // normally true
        err = requestMemBlock(blockAddress, _oneBlockBuff); _oneBlockBuffAddress = blockAddress; // fetch the whole block
        if(!_errGood(err)) { NT3HdebugPrint("_setBytesInBlock() read/write error!"); _oneBlockBuffAddress = NT3H_INVALID_MEMA; return(err); }
      }
      if((blockAddress == NT3H_I2C_ADDR_CHANGE_MEMA) && (bytesInBlockStart != NT3H_I2C_ADDR_CHANGE_MEMA_BYTE)) { _oneBlockBuff[NT3H_I2C_ADDR_CHANGE_MEMA_BYTE] = (slaveAddress<<1); } // I2C address byte reads as manufacturer ID
      if(isNewVersion && (blockAddress == NT3H2x11_PWD_MEMA)) { // (same as NT3H2x11_PACK_MEMA) the password and PACK read as all 0's (for obvious reasons)
        if((bytesInBlockStart == NT3H2x11_PWD_MEMA_BYTES_START) && (bytesToWrite == 6)) { // if the user intends to write PWD and PACK at once (replacing all the unreadable bytes by definition)
          storePWD_and_PACK(&_oneBlockBuff[NT3H2x11_PWD_MEMA_BYTES_START], &_oneBlockBuff[NT3H2x11_PACK_MEMA_BYTES_START]); // save the PWD and PACK, to make sure they don't get overwritten again later
        } else {
          if(!_passwordAndPackStored) { NT3HdebugPrint("_setBytesInBlock() overwrote password/PACK with all 0's! use storePWD_and_PACK() or setPWD_and_PACK() first!"); } // only warn, don't actually prevent writing
          for(uint8_t i=0; i<6; i++) { _oneBlockBuff[i+NT3H2x11_PWD_MEMA_BYTES_START] = _passwordAndPackBuff[i]; } // NOTE: 6 is made possible by the fact that PWD and PACK are contiguous!
          // for(uint8_t i=0; i<4; i++) { _oneBlockBuff[i+NT3H2x11_PWD_MEMA_BYTES_START] = _passwordAndPackBuff[i]; } // if PWD and PACK weren't contiguous
          // for(uint8_t i=0; i<2; i++) { _oneBlockBuff[i+NT3H2x11_PACK_MEMA_BYTES_START] = _passwordAndPackBuff[i+4]; }  // if PWD and PACK weren't contiguous
        }
        //if((AUTH0 <= NT3H2x11_AUTH0_DISABLE_THRESH) || (PT_I2C != 0)) { /* warn about the fact that password writing is going to fail (because it's actively used) */ }
      }
      for(uint8_t i=0; i<bytesToWrite; i++) { _oneBlockBuff[i+bytesInBlockStart] = writeBuff[i]; } // overwrite only the desired bytes
    }
    err = writeMemBlock(blockAddress, whichBuffToUse);
    if(!_errGood(err)) { NT3HdebugPrint("_setBytesInBlock() write error!"); }
    return(err); // err should always be OK, if it makes it to this point
  }
  /**
   * (private) overwrite an arbetrary set of bytes in a block (with a (little-endian) value)
   * @tparam T type of data to write
   * @param blockAddress MEMory Address (MEMA) of the block
   * @param bytesInBlockStart where in the block the relevant data starts (see defines up top)
   * @param newVal value (of type T) to write into the block
   * @param writeMSBfirst whether to write the MSByte first or the LSByte first (Big/Little-endian respectively)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  template<typename T> 
  NT3H_ERR_RETURN_TYPE _setValInBlock(uint8_t blockAddress, uint8_t bytesInBlockStart, T newVal, bool writeMSBfirst=true, bool useCache=false) {
    NT3H_ERR_RETURN_TYPE err = NT3H_ERR_RETURN_TYPE_OK;
    if( ! (useCache && (blockAddress == _oneBlockBuffAddress))) { // normally true
      err = requestMemBlock(blockAddress, _oneBlockBuff); _oneBlockBuffAddress = blockAddress; // fetch the whole block
      if(!_errGood(err)) { NT3HdebugPrint("_setValInBlock() read/write error!"); _oneBlockBuffAddress = NT3H_INVALID_MEMA; return(err); }
    }
    if((blockAddress == NT3H_I2C_ADDR_CHANGE_MEMA) && (bytesInBlockStart != NT3H_I2C_ADDR_CHANGE_MEMA_BYTE)) { _oneBlockBuff[NT3H_I2C_ADDR_CHANGE_MEMA_BYTE] = (slaveAddress<<1); } // I2C address byte reads as manufacturer ID
    if(isNewVersion && (blockAddress == NT3H2x11_PWD_MEMA)) { // (same as NT3H2x11_PACK_MEMA) the password and PACK read as all 0's (for obvious reasons)
      //// there is no (reasonable) way to set PWD and PACk at the same time (replacing all unreadable bytes) using multi-byte values. So, the _passwordAndPackBuff is always used
      if(!_passwordAndPackStored) { NT3HdebugPrint("_setBytesInBlock() overwrote password/PACK with all 0's! use storePWD_and_PACK() or setPWD_and_PACK() first!"); } // only warn, don't actually prevent writing
      for(uint8_t i=0; i<6; i++) { _oneBlockBuff[i+NT3H2x11_PWD_MEMA_BYTES_START] = _passwordAndPackBuff[i]; } // NOTE: 6 is made possible by the fact that PWD and PACK are contiguous!
      //if((AUTH0 <= NT3H2x11_AUTH0_DISABLE_THRESH) || (PT_I2C != 0)) { /* warn about the fact that password writing is going to fail (because it's actively used) */ }
    }
    uint8_t* bytePtrToNewVal = (uint8_t*) &newVal;
    for(uint8_t i=0; i<sizeof(T); i++) { _oneBlockBuff[i+bytesInBlockStart] = bytePtrToNewVal[writeMSBfirst ? (sizeof(T)-1-i) : i]; } // (little-endian)
    err = writeMemBlock(blockAddress, _oneBlockBuff);
    if(!_errGood(err)) { NT3HdebugPrint("_setValInBlock() write error!"); }
    return(err);
  }
  /**
   * (private) overwrite a portion (mask) of an arbetrary byte in an arbetrary block (same as _setValInBlock<uint8_t> BUT with a bitmask option)
   * @param blockAddress MEMory Address (MEMA) of the block
   * @param byteInBlock where in the block the relevant data starts (see defines up top)
   * @param newVal value (partial byte) to write into the block
   * @param mask which bits to affect (manual code, NOT inherent part of I2C format like with session registers)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE _setBitsInBlock(uint8_t blockAddress, uint8_t byteInBlock, uint8_t newVal, uint8_t mask, bool useCache=false) {
    NT3H_ERR_RETURN_TYPE err = NT3H_ERR_RETURN_TYPE_OK;
    if( ! (useCache && (blockAddress == _oneBlockBuffAddress))) { // normally true
      err = requestMemBlock(blockAddress, _oneBlockBuff); _oneBlockBuffAddress = blockAddress; // fetch the whole block
      if(!_errGood(err)) { NT3HdebugPrint("_setBitsInBlock() read/write error!"); _oneBlockBuffAddress = NT3H_INVALID_MEMA; return(err); }
    }
    if((blockAddress == NT3H_I2C_ADDR_CHANGE_MEMA) && (byteInBlock != NT3H_I2C_ADDR_CHANGE_MEMA_BYTE)) { _oneBlockBuff[NT3H_I2C_ADDR_CHANGE_MEMA_BYTE] = (slaveAddress<<1); } // I2C address byte reads as manufacturer ID
    if(isNewVersion && (blockAddress == NT3H2x11_PWD_MEMA)) { // (same as NT3H2x11_PACK_MEMA) the password and PACK read as all 0's (for obvious reasons)
      //// there is no (reasonable) way to set PWD and PACk at the same time (replacing all unreadable bytes) using multi-byte values. So, the _passwordAndPackBuff is always used
      if(!_passwordAndPackStored) { NT3HdebugPrint("_setBytesInBlock() overwrote password/PACK with all 0's! use storePWD_and_PACK() or setPWD_and_PACK() first!"); } // only warn, don't actually prevent writing
      for(uint8_t i=0; i<6; i++) { _oneBlockBuff[i+NT3H2x11_PWD_MEMA_BYTES_START] = _passwordAndPackBuff[i]; } // NOTE: 6 is made possible by the fact that PWD and PACK are contiguous!
      //if((AUTH0 <= NT3H2x11_AUTH0_DISABLE_THRESH) || (PT_I2C != 0)) { /* warn about the fact that password writing is going to fail (because it's actively used) */ }
    }
    _oneBlockBuff[byteInBlock] &= ~mask;            // excise old data
    _oneBlockBuff[byteInBlock] |= (newVal & mask);  // insert new data
    err = writeMemBlock(blockAddress, _oneBlockBuff);
    if(!_errGood(err)) { NT3HdebugPrint("_setBitsInBlock() write error!"); }
    return(err);
  }

/////////////////////////////////////////////////////////////////////////////////////// set functions: //////////////////////////////////////////////////////////

  /**
   * set the I2C address. NOTE: I2C address is stored in EEPROM, so effects are semi-premanent. If you lose your device, just do an I2C scan, it will ACK a START condition on the new address
   * @param newAddress is the new 7bit address (so before shifting and adding R/W bit, just 7 bits)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE setI2Caddress(uint8_t newAddress, bool useCache=false) { // does not seem to work (yet)
    NT3H_ERR_RETURN_TYPE err = _setValInBlock<uint8_t>(NT3H_I2C_ADDR_CHANGE_MEMA, NT3H_I2C_ADDR_CHANGE_MEMA_BYTE, newAddress<<1, true, useCache);
    if(_errGood(err)) { slaveAddress = newAddress; } // update this object's address byte ONLY IF the transfer seemed to go as intended
    else { NT3HdebugPrint("setI2Caddress() failed!"); }
    return(err);
  }

  /**
   * overwrite the Capability Container (also mentioned as NDEF thingy) with bytes
   * @param writeBuff 4 byte buffer to write to the CC bytes (i stronly recommend NT3H_CAPA_CONT_DEFAULT[is2kVariant])
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE setCC(uint8_t writeBuff[], bool useCache = false) { return(_setBytesInBlock(NT3H_CAPA_CONT_MEMA, NT3H_CAPA_CONT_MEMA_BYTES_START, 4, writeBuff, useCache)); }
  /**
   * overwrite the Capability Container (also mentioned as NDEF thingy) with a 4byte value
   * @param newVal 4 byte value (little-endian) to write to the CC bytes (i stronly recommend NT3H_CAPA_CONT_DEFAULT_uint32_t[is2kVariant])
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE setCC(uint32_t newVal, bool useCache = false) { return(_setValInBlock<uint32_t>(NT3H_CAPA_CONT_MEMA, NT3H_CAPA_CONT_MEMA_BYTES_START, newVal, true, useCache)); }

  ///////////////////////////////////// Session register set functions: /////////////////////////////////////
  /**
   * overwrite the (whole) NC_REG Session register
   * @param newVal (see NT3H_NC_REG_xxx_bits defines at top for contents)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_NC_REG(uint8_t newVal) { return(writeSessRegByte(NT3H_COMN_REGS_NC_REG_BYTE, newVal)); }
  /**
   * overwrite FD_OFF bits from the NC_REG Session register
   * @param newVal FD_OFF determines the behaviour of the FD pin (falling)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_NC_FD_OFF(NT3H_FD_OFF_ENUM newVal) { return(writeSessRegByte(NT3H_COMN_REGS_NC_REG_BYTE, static_cast<uint8_t>(newVal) << 4, NT3H_NC_REG_FD_OFF_bits)); }
  /**
   * overwrite FD_ON bits from the NC_REG Session register
   * @param newVal FD_ON determines the behaviour of the FD pin (rising)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_NC_FD_ON(NT3H_FD_ON_ENUM newVal) { return(writeSessRegByte(NT3H_COMN_REGS_NC_REG_BYTE, static_cast<uint8_t>(newVal) << 2, NT3H_NC_REG_FD_ON_bits)); }
  /**
   * (private) overwrite one arbetrary bit from the NC_REG Session register
   * @param mask which bit to target (see NT3H_NC_REG_xxx_bits defines at top for contents)
   * @param newBitVal LSBit boolean, to be shifted by the mask to the correct position
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE _setSess_NC_oneBit(uint8_t mask, bool newBitVal) { return(writeSessRegByte(NT3H_COMN_REGS_NC_REG_BYTE, newBitVal ? mask : 0, mask)); }
  /**
   * overwrite NFCS_I2C_RST_ON_OFF bit from the NC_REG Session register
   * @param newVal NFCS_I2C_RST_ON_OFF enables (NFC-silence (NT3H2x11 only) AND) soft-reset-through-repeated-I2C-starts (very cool, slightly niche)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_NC_NFCS_I2C_RST(bool newVal) { return(_setSess_NC_oneBit(NT3H_NC_REG_SIL_SRST_bits, newVal)); } // (just a macro)
  /**
   * overwrite PTHRU_ON_OFF bit from the NC_REG Session register
   * @param newVal PTHRU_ON_OFF bit (bool)     PTHRU_ON_OFF enables Pass-Through mode
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_NC_PTHRU(bool newVal) { return(_setSess_NC_oneBit(NT3H_NC_REG_PTHRU_bits, newVal)); } // (just a macro)
  /**
   * overwrite SRAM_MIRROR_ON_OFF bit from the NC_REG Session register
   * @param newVal SRAM_MIRROR_ON_OFF bit (bool)     SRAM_MIRROR_ON_OFF enables Memory-Mirror mode
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_NC_MIRROR(bool newVal) { return(_setSess_NC_oneBit(NT3H_NC_REG_MIRROR_bits, newVal)); } // (just a macro)
  /**
   * overwrite TRANSFER_DIR/PTHRU_DIR bit from the NC_REG Session register
   * @param newVal TRANSFER_DIR/PTHRU_DIR bit (bool)     TRANSFER_DIR/PTHRU_DIR determines the direction of data in Pass-Through mode, or can disable RF write-access otherwise
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_NC_DIR(bool newVal) { return(_setSess_NC_oneBit(NT3H_NC_REG_DIR_bits, newVal)); } // (just a macro)

  /**
   * overwrite the LAST_NDEF_BLOCK byte in the Session registers
   * @param newVal address of last block (== 16 bytes == 4 pages) of user-memory that holds actual data
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_LAST_NDEF_BLOCK(uint8_t newVal) { return(writeSessRegByte(NT3H_COMN_REGS_LAST_NDEF_BLOCK_BYTE, newVal)); }
  /**
   * overwrite the SRAM_MIRROR_BLOCK byte in the Session registers
   * @param newVal address of first block of user-memory to be replaced (mapped over) by SRAM when Mirror-Mode is enabled
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_SRAM_MIRROR_BLOCK(uint8_t newVal) { return(writeSessRegByte(NT3H_COMN_REGS_SRAM_MIRROR_BLOCK_BYTE, newVal)); }
  /**
   * overwrite the WatchDog Timer threshold (raw) (from a byte buffer) in the Session registers
   * @param writeBuff 2 byte buffer to write to WDT_LS and WDT_MS (in that order)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_WDTraw(uint8_t writeBuff[]) {
    NT3H_ERR_RETURN_TYPE err = writeSessRegByte(NT3H_COMN_REGS_WDT_MS_BYTE, writeBuff[0]);
    if(!_errGood(err)) { return(err); } // if the first one failed, return that error
    return(writeSessRegByte(NT3H_COMN_REGS_WDT_MS_BYTE, writeBuff[1]));
  }
  /**
   * overwrite the WatchDog Timer threshold (raw) in the Session registers
   * @param newVal the WatchDog Timer threshold as a uint16_t, (multiply with NT3H_WDT_RAW_TO_MICROSECONDS to get microseconds)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_WDTraw(uint16_t newVal) {
    uint8_t* bytePtrToNewVal = (uint8_t*) &newVal;
    return(setSess_WDTraw(bytePtrToNewVal));
  }
  /**
   * overwrite the WatchDog Timer threshold in the Session registers
   * @param newVal the WatchDog Timer threshold in microseconds
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setSess_WDT(float newVal) { return(setSess_WDTraw(constrain(newVal,0,(((float)0xFFFF)*NT3H_WDT_RAW_TO_MICROSECONDS)) / NT3H_WDT_RAW_TO_MICROSECONDS)); } // (just a macro)

  // /**                    ///////////// only 2 bits are R/W, the other 6 are Read-only. This function doesn't really make sense /////////////
  //  * overwrite the (whole) NS_REG Session register
  //  * @param newVal (see NT3H_NS_REG_xxx_bits defines at top for contents)
  //  * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
  //  */
  // NT3H_ERR_RETURN_TYPE setNS_REG(uint8_t newVal) { return(writeSessRegByte(NT3H_SESS_REGS_NS_REG_BYTE, newVal, 0b01000100)); }
  /**
   * (private) overwrite one arbetrary bit from the NS_REG Session register. NOTE: only 2 bits are R/W, the rest is R-only: mask=0b01000100
   * @param mask which bit to target (see NT3H_NS_REG_xxx_bits defines at top for contents)
   * @param newBitVal LSBit boolean, to be shifted by the mask to the correct position
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE _setNS_oneBit(uint8_t mask, bool newBitVal) { return(writeSessRegByte(NT3H_SESS_REGS_NS_REG_BYTE, newBitVal ? mask : 0, mask)); }
  /**
   * overwrite I2C_LOCKED bit from the NS_REG Session register
   * @param newVal I2C_LOCKED is 1 if I2C has control of memory (arbitration). Should be cleared once the I2C interaction is completely done, may be cleared by WDT
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setNS_I2C_LOCKED(bool newVal) { return(_setNS_oneBit(NT3H_NS_REG_I2C_LOCKED_bits, newVal)); } // (just a macro)
  /**
   * clear EEPROM_WR_ERR flag from the NS_REG Session register. EEPROM_WR_ERR is 1 if there was a (High Voltage?) error during EEPROM write. Flag needs to be manually cleared
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE clear_EEPROM_WR_ERR() { return(_setNS_oneBit(NT3H_NS_REG_EPR_WR_ERR_bits, 0)); } // (just a macro)

  ///////////////////////////////////// Configuration register set functions: /////////////////////////////////////
  /**
   * (private) overwrite an arbetrary value in the Configuration registers
   * @tparam T type of data to write
   * @param bytesInBlockStart which register (use NT3H_CONF_SESS_REGS_ENUM enum)
   * @param newVal the new value (in format T)
   * @param writeMSBfirst whether to write the MSByte first or the LSByte first (Big/Little-endian respectively)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  template<typename T> 
  NT3H_ERR_RETURN_TYPE _setConfRegVal(NT3H_CONF_SESS_REGS_ENUM bytesInBlockStart, T newVal, bool writeMSBfirst=false, bool useCache=false)
    { return(_setValInBlock<T>(((!isNewVersion) && is2kVariant) ? NT3H1201_CONF_REGS_MEMA : NT3Hxxx1_CONF_REGS_MEMA, bytesInBlockStart, newVal, writeMSBfirst, useCache)); } // (just a macro)
  /**
   * (private) overwrite a portion (mask) of an arbetrary byte in the Configuration registers (same as _setConfRegVal<uint8_t> BUT with a bitmask option)
   * @param byteInBlock where in the block the relevant data starts (see defines up top)
   * @param newVal value (partial byte) to write into the block
   * @param mask which bits to affect (manual code, NOT inherent part of I2C format like with session registers)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE _setConfRegBits(NT3H_CONF_SESS_REGS_ENUM byteInBlock, uint8_t newVal, uint8_t mask, bool useCache=false) {
    return(_setBitsInBlock(((!isNewVersion) && is2kVariant) ? NT3H1201_CONF_REGS_MEMA : NT3Hxxx1_CONF_REGS_MEMA, byteInBlock, newVal, mask, useCache)); // (just a macro) (NOTE: 1x01 to 2x11 change)
  }

  /**
   * overwrite the (whole) NC_REG Configuration register
   * @param newVal (see NT3H_NC_REG_xxx_bits defines at top for contents)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_NC_REG(uint8_t newVal, bool useCache=false) {
    return(_setConfRegVal<uint8_t>(NT3H_COMN_REGS_NC_REG_BYTE, isNewVersion ? newVal : (newVal & (~NT3H1x01_NC_REG_RFU_bits)), false, useCache)); } // (NOTE: 1x01 to 2x11 change)
  /**
   * overwrite FD_OFF bits from the NC_REG Configuration register
   * @param newVal FD_OFF determines the behaviour of the FD pin (falling)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_NC_FD_OFF(NT3H_FD_OFF_ENUM newVal, bool useCache=false) {
    return(_setConfRegBits(NT3H_COMN_REGS_NC_REG_BYTE, static_cast<uint8_t>(newVal) << 4, NT3H_NC_REG_FD_OFF_bits, useCache)); }
  /**
   * overwrite FD_ON bits from the NC_REG Configuration register
   * @param newVal FD_ON determines the behaviour of the FD pin (rising)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_NC_FD_ON(NT3H_FD_ON_ENUM newVal, bool useCache=false) {
    return(_setConfRegBits(NT3H_COMN_REGS_NC_REG_BYTE, static_cast<uint8_t>(newVal) << 2, NT3H_NC_REG_FD_ON_bits, useCache)); }
  /**
   * (private) overwrite one arbetrary bit from the NC_REG Configuration register
   * @param mask which bit to target (see NT3H1x01_NC_REG_xxx_bits defines at top for contents)
   * @param newBitVal LSBit boolean, to be shifted by the mask to the correct position
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE _setConf_NC_oneBit(uint8_t mask, bool newBitVal, bool useCache=false) {
    return(_setConfRegBits(NT3H_COMN_REGS_NC_REG_BYTE, newBitVal ? mask : 0, mask, useCache)); }
  /**
   * overwrite NFCS_I2C_RST_ON_OFF bit from the NC_REG Configuration register
   * @param newVal NFCS_I2C_RST_ON_OFF enables (NFC-silence (NT3H2x11 only) AND) soft-reset-through-repeated-I2C-starts (very cool, slightly niche)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_NC_NFCS_I2C_RST(bool newVal, bool useCache=false) { return(_setConf_NC_oneBit(NT3H_NC_REG_SIL_SRST_bits, newVal, useCache)); } // (just a macro)
  /**
   * overwrite PTHRU_ON_OFF bit from the NC_REG Configuration register (NT3H2x11 only)
   * @param newVal PTHRU_ON_OFF bit (bool)     PTHRU_ON_OFF enables Pass-Through mode
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_NC_PTHRU(bool newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("setConf_NC_PTHRU() only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); } // On NT3H1x01 this bit is RFU and should be kept 0
    return(_setConf_NC_oneBit(NT3H_NC_REG_PTHRU_bits, newVal, useCache)); } // (just a macro)
  /**
   * overwrite SRAM_MIRROR_ON_OFF bit from the NC_REG Configuration register (NT3H2x11 only)
   * @param newVal SRAM_MIRROR_ON_OFF bit (bool)     SRAM_MIRROR_ON_OFF enables Memory-Mirror mode
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_NC_MIRROR(bool newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("setConf_NC_PTHRU() only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); } // On NT3H1x01 this bit is RFU and should be kept 0
    return(_setConf_NC_oneBit(NT3H_NC_REG_MIRROR_bits, newVal, useCache)); } // (just a macro)
  /**
   * overwrite TRANSFER_DIR/PTHRU_DIR bit from the NC_REG Configuration register
   * @param newVal TRANSFER_DIR/PTHRU_DIR bit (bool)     TRANSFER_DIR/PTHRU_DIR determines the direction of data in Pass-Through mode, or can disable RF write-access otherwise
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_NC_DIR(bool newVal, bool useCache=false) { return(_setConf_NC_oneBit(NT3H_NC_REG_DIR_bits, newVal, useCache)); } // (just a macro)

  /**
   * overwrite the LAST_NDEF_BLOCK byte in the Configuration registers
   * @param newVal address of last block (== 16 bytes == 4 pages) of user-memory that holds actual data
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_LAST_NDEF_BLOCK(uint8_t newVal, bool useCache=false) { return(_setConfRegVal<uint8_t>(NT3H_COMN_REGS_LAST_NDEF_BLOCK_BYTE, newVal, false, useCache)); }
  /**
   * overwrite the SRAM_MIRROR_BLOCK byte in the Configuration registers
   * @param newVal address of first block of user-memory to be replaced (mapped over) by SRAM when Mirror-Mode is enabled
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_SRAM_MIRROR_BLOCK(uint8_t newVal, bool useCache=false) { return(_setConfRegVal<uint8_t>(NT3H_COMN_REGS_SRAM_MIRROR_BLOCK_BYTE, newVal, false, useCache)); }
  /**
   * overwrite the WatchDog Timer threshold (raw) (from a byte buffer) in the Configuration registers
   * @param writeBuff 2 byte buffer to write to WDT_LS and WDT_MS (in that order)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_WDTraw(uint8_t writeBuff[], bool useCache=false) {
    return(_setBytesInBlock(((!isNewVersion) && is2kVariant) ? NT3H1201_CONF_REGS_MEMA : NT3Hxxx1_CONF_REGS_MEMA, NT3H_COMN_REGS_WDT_LS_BYTE, 2, writeBuff, useCache)); } // (just a macro)
  /**
   * overwrite the WatchDog Timer threshold (raw) in the Configuration registers
   * @param newVal the WatchDog Timer threshold as a uint16_t (written LSByte-first), (multiply with NT3H_WDT_RAW_TO_MICROSECONDS to get microseconds)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_WDTraw(uint16_t newVal, bool useCache=false) { return(_setConfRegVal<uint16_t>(NT3H_COMN_REGS_WDT_LS_BYTE, newVal, false, useCache)); } // (just a macro)
  /**
   * overwrite the WatchDog Timer threshold in the Configuration registers
   * @param newVal the WatchDog Timer threshold in microseconds
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setConf_WDT(float newVal, bool useCache=false) { return(setConf_WDTraw(constrain(newVal,0,(((float)0xFFFF)*NT3H_WDT_RAW_TO_MICROSECONDS)) / NT3H_WDT_RAW_TO_MICROSECONDS, useCache)); } // (just a macro)
  /**
   * overwrite the I2C_CLOCK_STR byte in the Configuration registers. NOTE: only available for Conf., in Sess. regs the datasheets say I2C_CLOCK_STR is Read-only
   * @param newVal I2C clock stretching enable/disable
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE set_I2C_CLOCK_STR(bool newVal, bool useCache=false) { return(_setConfRegBits(NT3H_COMN_REGS_I2C_CLOCK_STR_BYTE, newVal, 0x01, useCache)); } // (just a macro)

  #ifdef NT3H_unlock_burning // functions that can have a permanent consequence)
    #warning("burnRegLockI2C() and burnRegLockRF() are untested!")
    /**
     * disables writing to the Configuration register bytes from I2C PERMANENTLY
     * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
     * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
     */
    NT3H_ERR_RETURN_TYPE burnRegLockI2C(bool useCache=false) { return(_setConfRegVal<uint8_t>(NT3H_CONF_REGS_REG_LOCK_BYTE, NT3H_NC_REG_LOCK_I2C_bits, false, useCache)); }
    /**
     * disables writing to the Configuration register bytes from RF PERMANENTLY
     * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
     * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
     */
    NT3H_ERR_RETURN_TYPE burnRegLockRF(bool useCache=false) { return(_setConfRegVal<uint8_t>(NT3H_CONF_REGS_REG_LOCK_BYTE, NT3H_NC_REG_LOCK_RF_bits, false, useCache)); }
  #endif // NT3H_unlock_burning

  ///////////////////////////////////// password (related) set functions (NT3H2x11 only): /////////////////////////////////////
  //// NOTE: you cannot read the password and PACK values, so the functions below that set() partial blocks (all of them), are going to overwrite the password as well!
  ////        therefore, use either storePWD_and_PACK() or setPWD_and_PACK() before any other functions, or just use a passwordBlock struct and writePasswordBlock() to write the whole block in one go

  /**
   * overwrite the AUTH0 byte. NOTE: make absolutely sure you set a password (PWD) (and also set a PACK?) before(!) making this value <= 0xEB (which activates password protection) (NT3H2x11 only)
   * @param newVal address of first block to be protected by password. > 0xEB disables password protection, minimum value is TBD, but assume 0x02 for now (as per page 16 of 2x11 datasheet)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setAUTH0(uint8_t newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setValInBlock<uint8_t>(NT3H2x11_AUTH0_MEMA, NT3H2x11_AUTH0_MEMA_BYTE, newVal, true, useCache)); }
  /**
   * overwrite the (whole) ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param newVal (see NT3H2x11_ACCESS_xxx_bits defines at top for contents)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setACCESS(uint8_t newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setValInBlock<uint8_t>(NT3H2x11_ACCESS_MEMA, NT3H2x11_ACCESS_MEMA_BYTE, newVal, true, useCache)); }
  /**
   * overwrite NFC_PROT bit in the ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param newVal NFC_PROT determines whether read-access is ALSO password protected (instead of only write-access) (from NFC)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setACCESS_NFC_PROT(bool newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBitsInBlock(NT3H2x11_ACCESS_MEMA, NT3H2x11_ACCESS_MEMA_BYTE, newVal ? NT3H2x11_ACCESS_NFC_PROT_bits : 0, NT3H2x11_ACCESS_NFC_PROT_bits, useCache)); } // (just a macro)
  /**
   * overwrite NFC_DIS_SEC1 bit in the ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param newVal NFC_DIS_SEC1 disables sector 1 (NFC), effectively making the 2k IC the same as the 1k
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setACCESS_NFC_DIS_SEC1(bool newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBitsInBlock(NT3H2x11_ACCESS_MEMA, NT3H2x11_ACCESS_MEMA_BYTE, newVal ? NT3H2x11_ACCESS_NFC_DIS_SEC1_bits : 0, NT3H2x11_ACCESS_NFC_DIS_SEC1_bits, useCache)); } // (just a macro)
  /**
   * overwrite AUTHLIM bits in the ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param newVal AUTHLIM limits the number of failed password auth attempts. (3 bit value, 0 = disabled, otherwise max_attemps = 2^(bits) = 1<<bits)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setACCESS_AUTHLIMraw(uint8_t newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBitsInBlock(NT3H2x11_ACCESS_MEMA, NT3H2x11_ACCESS_MEMA_BYTE, newVal, NT3H2x11_ACCESS_AUTHLIM_bits, useCache)); } // (just a macro)
  //// TODO: non-raw AUTHLIM set function (would only accept powers of 2 as input)
  /**
   * disable AUTHLIM (the limit on password auth attempts, which (silently) locks(/destroys????) data when password was wrong too many times) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setACCESS_AUTHLIM_disabled(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(setACCESS_AUTHLIMraw(0, useCache)); } // (just a macro)
  /**
   * overwrite the PWD (password) (if allowed) (NT3H2x11 only)
   * @param writeBuff 4 byte buffer containing new password. Password should be LSByte first, see page 36 of 2x11 datasheet
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPWD(uint8_t writeBuff[], bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBytesInBlock(NT3H2x11_PWD_MEMA, NT3H2x11_PWD_MEMA_BYTES_START, 4, writeBuff, useCache)); } // (just a macro)
  /**
   * overwrite the PWD (password) (if allowed) (NT3H2x11 only)
   * @param newVal new password as a 32bit value. Written LSByte first, but just to be sure, try using the buffer version instead
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPWD(uint32_t newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setValInBlock<uint32_t>(NT3H2x11_PWD_MEMA, NT3H2x11_PWD_MEMA_BYTES_START, newVal, false, useCache)); } // (just a macro)
  /**
   * overwrite the PACK (password acknowledge) (if allowed) (NT3H2x11 only)
   * @param writeBuff 2 byte buffer containing password acknowledge value. I'm currently not 100% sure what this does...
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPACK(uint8_t writeBuff[], bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBytesInBlock(NT3H2x11_PACK_MEMA, NT3H2x11_PACK_MEMA_BYTES_START, 2, writeBuff, useCache)); } // (just a macro)
  /**
   * (not recommended, use setPWD_and_PACK instead) overwrite the PACK (password acknowledge) (if allowed) (NT3H2x11 only)
   * @param newVal password acknowledge value as a 16bit value. Written LSByte first, but just to be sure, try using the buffer version instead
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPACK(uint16_t newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setValInBlock<uint16_t>(NT3H2x11_PACK_MEMA, NT3H2x11_PACK_MEMA_BYTES_START, newVal, false, useCache)); } // (just a macro)
  
  /**
   * (private) write the contents of _passwordAndPackBuff to the device. Setting the password (if AUTH0 is >0xEB of course) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE _writePWD_and_PACK(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBytesInBlock(NT3H2x11_PWD_MEMA, NT3H2x11_PWD_MEMA_BYTES_START, 6, _passwordAndPackBuff, useCache));
  }
  /**
   * overwrite the password and PACK (password acknowledge) at the same time (to avoid the complications of chaching one value while writing partial blocks) (NT3H2x11 only)
   * @param passwordBuff 4 byte buffer containing new password. Password should be LSByte first, see page 36 of 2x11 datasheet
   * @param PACKbuff 2 byte buffer containing password acknowledge value. I'm currently not 100% sure what this does...
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPWD_and_PACK(uint8_t passwordBuff[], uint8_t PACKbuff[], bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    storePWD_and_PACK(passwordBuff, PACKbuff); // saves password and PACK to _passwordAndPackBuff (note: this is technically redundant, as _setBytesInBlock() will also call this function)
    return(_writePWD_and_PACK(useCache)); // write _passwordAndPackBuff to device
  } // (just a macro)
  /**
   * overwrite the password and PACK (password acknowledge) at the same time (to avoid the complications of chaching one value while writing partial blocks) (NT3H2x11 only)
   * @param password new password as a 32bit value. Written LSByte first, but just to be sure, try using the buffer version instead
   * @param PACK password acknowledge value as a 16bit value. Written LSByte first, but just to be sure, try using the buffer version instead
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPWD_and_PACK(uint32_t password, uint16_t PACK, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    storePWD_and_PACK(password, PACK); // saves password and PACK to _passwordAndPackBuff (note: this is technically redundant, as _setBytesInBlock() will also call (the buffer version of) this function)
    return(_writePWD_and_PACK(useCache)); // write _passwordAndPackBuff to device
  } // (just a macro)
  /**
   * overwrite whole block 0x39 (which holds the password stuff) at once. This is the best way to ensure all settings are written correctly (NT3H2x11 only)
   * @param blockToWrite is a special struct which will ensure the whole block gets written all at once
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE writePasswordBlock(passwordBlock& blockToWrite) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    storePWD_and_PACK(&(blockToWrite.PWD(0)), &(blockToWrite.PACK(0))); // saves password and PACK to _passwordAndPackBuff
    // storePWD_and_PACK(blockToWrite.PWD_uint32_t(), blockToWrite.PACK_uint16_t()); // same thing as line above, but perhaps more legible
    return(writeMemBlock(NT3H2x11_PWD_MEMA, &blockToWrite)); // note: &passwordBlock returns a uint8_t&, as explicitely defined in the _blockStruct class
    // return(_writeBlockStruct(NT3H2x11_PWD_MEMA, blockToWrite)); // excessive macro
  }
  
  /**
   * overwrite the (whole) PT_I2C byte (for password stuff) (NT3H2x11 only)
   * @param newVal (see NT3H2x11_PT_I2C_xxx_bits defines at top for contents)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPT_I2C(uint8_t newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setValInBlock<uint8_t>(NT3H2x11_PT_I2C_MEMA, NT3H2x11_PT_I2C_MEMA_BYTE, newVal, true, useCache)); }
  /**
   * overwrite 2K_PROT bit in the PT_I2C byte (for password stuff) (NT3H2x11 only)
   * @param newVal 2K_PROT enables password protection for sector 1 (only on 2k version)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPT_I2C_2K_PROT(bool newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBitsInBlock(NT3H2x11_PT_I2C_MEMA, NT3H2x11_PT_I2C_MEMA_BYTE, newVal ? NT3H2x11_PT_I2C_2K_PROT_bits : 0, NT3H2x11_PT_I2C_2K_PROT_bits, useCache)); } // (just a macro)
  /**
   * overwrite SRAM_PROT bit in the PT_I2C byte (for password stuff) (NT3H2x11 only)
   * @param newVal SRAM_PROT enables password protection for SRAM (for pass-through and mirror mode) (does not apply to I2C?)
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPT_I2C_SRAM_PROT(bool newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBitsInBlock(NT3H2x11_PT_I2C_MEMA, NT3H2x11_PT_I2C_MEMA_BYTE, newVal ? NT3H2x11_PT_I2C_SRAM_PROT_bits : 0, NT3H2x11_PT_I2C_SRAM_PROT_bits, useCache)); } // (just a macro)
  /**
   * overwrite I2C_PROT bits in the PT_I2C byte (for password stuff) (NT3H2x11 only)
   * @param newVal I2C_PROT determines R/W access to protected areas from I2C. 2bit value, 0=no_restrictions, 1=R_only, 2|3=no_access
   * @param useCache (optional!, not recommended, use at own discretion) use data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE setPT_I2C_I2C_PROT(uint8_t newVal, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_setBitsInBlock(NT3H2x11_PT_I2C_MEMA, NT3H2x11_PT_I2C_MEMA_BYTE, newVal, NT3H2x11_PT_I2C_I2C_PROT_bits, useCache)); } // (just a macro)


/////////////////////////////////////////////////////////////////////////////////////// get functions: //////////////////////////////////////////////////////////

  /**
   * retrieve the Serial Number, a.k.a. UID
   * @param readBuff 7 byte buffer to put the results in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getUID(uint8_t readBuff[], bool useCache=false) { return(_getBytesFromBlock(NT3H_SERIAL_NR_MEMA, NT3H_SERIAL_NR_MEMA_BYTES_START, 7, readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the Capability Container (also mentioned as NDEF thingy) (this version of the function lets you check for I2C errors)
   * @param readBuff 4 byte buffer to put the results in (results should match NT3H_CAPA_CONT_DEFAULT)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getCC(uint8_t readBuff[], bool useCache=false) { return(_getBytesFromBlock(NT3H_CAPA_CONT_MEMA, NT3H_CAPA_CONT_MEMA_BYTES_START, 4, readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the Capability Container (also mentioned as NDEF thingy) (this version DOES NOT let you check for I2C errors)
   * @param readMSBfirst whether to read the MSByte first or the LSByte first (Big/Little-endian respectively)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the Capability Container as a uint32_t. result should match NT3H_CAPA_CONT_DEFAULT_uint32_t
   */
  uint32_t getCC(bool readMSBfirst=true, bool useCache=false) { return(_getValFromBlock<uint32_t>(NT3H_CAPA_CONT_MEMA, NT3H_CAPA_CONT_MEMA_BYTES_START, readMSBfirst, useCache)); } // (just a macro)
  /**
   * retrieve the ATQA bytes NOTE: 1x01 datasheet mentions theses bytes are stored LSB first (this version of the function lets you check for I2C errors)
   * @param readBuff 2 byte buffer to put the results in (result should match 0x44,0x00)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getATQA(uint8_t readBuff[], bool useCache=false) { return(_getBytesFromBlock(NT3H_ATQA_MEMA, NT3H_ATQA_MEMA_BYTES_START, 2, readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the ATQA bytes as uint16_t (this version DOES NOT let you check for I2C errors)
   * @param readMSBfirst whether to read the MSByte first or the LSByte first (Big/Little-endian respectively)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return ATQA bytes as a uint16_t.
   */
  uint16_t getATQA(bool readMSBfirst=true, bool useCache=false) { return(_getValFromBlock<uint16_t>(NT3H_ATQA_MEMA, NT3H_ATQA_MEMA_BYTES_START, readMSBfirst, useCache)); } // (just a macro)
  /**
   * retrieve the SAK byte (this version of the function lets you check for I2C errors)
   * @param readBuff byte pointer to put the result in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getSAK(uint8_t readBuff[], bool useCache=false) { return(_getBytesFromBlock(NT3H_SAK_MEMA, NT3H_SAK_MEMA_BYTE, 1, readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the SAK byte (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the SAK byte
   */
  uint8_t getSAK(bool useCache=false) { return(_getValFromBlock<uint8_t>(NT3H_SAK_MEMA, NT3H_SAK_MEMA_BYTE, true, useCache)); } // (just a macro)


  ///////////////////////////////////// Session register get functions: /////////////////////////////////////
  /**
   * retrieve the (whole) NC_REG Session register (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in (see NT3H_NC_REG_xxx_bits defines at top for contents)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getSess_NC_REG(uint8_t& readBuff) { return(requestSessRegByte(NT3H_COMN_REGS_NC_REG_BYTE, readBuff)); } // (just a macro)
  /**
   * retrieve the (whole) NC_REG Session register (this version DOES NOT let you check for I2C errors)
   * @return the (whole) NC_REG (see NT3H_NC_REG_xxx_bits defines at top for contents)
   */
  uint8_t getSess_NC_REG() {
    uint8_t readBuff;   NT3H_ERR_RETURN_TYPE err = getSess_NC_REG(readBuff);
    if(!_errGood(err)) { NT3HdebugPrint("getSess_NC_REG() read/write error!"); }
    return(readBuff);
  }
  /**
   * retrieve NFCS_I2C_RST_ON_OFF bit from the NC_REG Session register
   * @return the NFCS_I2C_RST_ON_OFF bit (bool)     NFCS_I2C_RST_ON_OFF enables (NFC-silence (NT3H2x11 only) AND) soft-reset-through-repeated-I2C-starts (very cool, slightly niche)
   */
  bool getSess_NC_NFCS_I2C_RST() { return((getSess_NC_REG() & NT3H_NC_REG_SIL_SRST_bits) != 0); } // (just a macro)
  /**
   * retrieve PTHRU_ON_OFF bit from the NC_REG Session register
   * @return the PTHRU_ON_OFF bit (bool)     PTHRU_ON_OFF enables Pass-Through mode
   */
  bool getSess_NC_PTHRU() { return((getSess_NC_REG() & NT3H_NC_REG_PTHRU_bits) != 0); } // (just a macro)
  /**
   * retrieve FD_OFF bits from the NC_REG Session register
   * @return the FD_OFF bits (2)     FD_OFF determines the behaviour of the FD pin (falling)
   */
  NT3H_FD_OFF_ENUM getSess_NC_FD_OFF() { return(static_cast<NT3H_FD_OFF_ENUM>((getSess_NC_REG() & NT3H_NC_REG_FD_OFF_bits) >> 4)); } // (just a macro)
  /**
   * retrieve FD_ON bits from the NC_REG Session register
   * @return the FD_ON bits (2)     FD_ON determines the behaviour of the FD pin (rising)
   */
  NT3H_FD_ON_ENUM getSess_NC_FD_ON() { return(static_cast<NT3H_FD_ON_ENUM>((getSess_NC_REG() & NT3H_NC_REG_FD_ON_bits) >> 2)); } // (just a macro)
  /**
   * retrieve SRAM_MIRROR_ON_OFF bit from the NC_REG Session register
   * @return the SRAM_MIRROR_ON_OFF bit (bool)     SRAM_MIRROR_ON_OFF enables Memory-Mirror mode
   */
  bool getSess_NC_MIRROR() { return((getSess_NC_REG() & NT3H_NC_REG_MIRROR_bits) != 0); } // (just a macro)
  /**
   * retrieve TRANSFER_DIR/PTHRU_DIR bit from the NC_REG Session register
   * @return the TRANSFER_DIR/PTHRU_DIR bit (bool)     TRANSFER_DIR/PTHRU_DIR determines the direction of data in Pass-Through mode, or can disable RF write-access otherwise
   */
  bool getSess_NC_DIR() { return((getSess_NC_REG() & NT3H_NC_REG_DIR_bits) != 0); } // (just a macro)

  /**
   * retrieve the LAST_NDEF_BLOCK byte from the Session registers (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getSess_LAST_NDEF_BLOCK(uint8_t& readBuff) { return(requestSessRegByte(NT3H_COMN_REGS_LAST_NDEF_BLOCK_BYTE, readBuff)); } // (just a macro)
  /**
   * retrieve the LAST_NDEF_BLOCK byte from the Session registers (this version DOES NOT let you check for I2C errors)
   * @return the LAST_NDEF_BLOCK byte
   */
  uint8_t getSess_LAST_NDEF_BLOCK() {
    uint8_t readBuff;   NT3H_ERR_RETURN_TYPE err = getSess_LAST_NDEF_BLOCK(readBuff);
    if(!_errGood(err)) { NT3HdebugPrint("getSess_LAST_NDEF_BLOCK() read/write error!"); }
    return(readBuff);
  }
  /**
   * retrieve the SRAM_MIRROR_BLOCK byte from the Session registers (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getSess_SRAM_MIRROR_BLOCK(uint8_t& readBuff) { return(requestSessRegByte(NT3H_COMN_REGS_SRAM_MIRROR_BLOCK_BYTE, readBuff)); } // (just a macro)
  /**
   * retrieve the SRAM_MIRROR_BLOCK byte from the Session registers (this version DOES NOT let you check for I2C errors)
   * @return the SRAM_MIRROR_BLOCK byte
   */
  uint8_t getSess_SRAM_MIRROR_BLOCK() {
    uint8_t readBuff;   NT3H_ERR_RETURN_TYPE err = getSess_SRAM_MIRROR_BLOCK(readBuff);
    if(!_errGood(err)) { NT3HdebugPrint("getSess_SRAM_MIRROR_BLOCK() read/write error!"); }
    return(readBuff);
  }
  /**
   * retrieve the WatchDog Timer threshold (raw) from the Session registers (this version of the function lets you check for I2C errors)
   * @param readBuff 2 byte buffer to put the results in (first byte is LSB)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getSess_WDTraw(uint8_t readBuff[]) {
    NT3H_ERR_RETURN_TYPE err = requestSessRegByte(NT3H_COMN_REGS_WDT_LS_BYTE, readBuff[0]);
    if(!_errGood(err)) { return(err); } // if the first one failed, return that error
    return(requestSessRegByte(NT3H_COMN_REGS_WDT_MS_BYTE, readBuff[1]));
  }
  /**
   * retrieve the WatchDog Timer threshold (raw) from the Session registers (this version DOES NOT let you check for I2C errors)
   * @return the WatchDog Timer threshold as a uint16_t, multiply with NT3H_WDT_RAW_TO_MICROSECONDS to get microseconds
   */
  uint16_t getSess_WDTraw() {
    uint16_t returnVal;   uint8_t* bytePtrToReturnVal = (uint8_t*) &returnVal; // assembly the 16bit number inherently by reading into it as a byte array
    NT3H_ERR_RETURN_TYPE err = getSess_WDTraw(bytePtrToReturnVal);
    if(!_errGood(err)) { NT3HdebugPrint("getSess_WDTraw() read/write error!"); }
    return(returnVal);
  }
  /**
   * retrieve the WatchDog Timer threshold from the Session registers (this version DOES NOT let you check for I2C errors)
   * @return the WatchDog Timer threshold in microseconds
   */
  float getSess_WDT() { return((float) getSess_WDTraw() * NT3H_WDT_RAW_TO_MICROSECONDS); } // (just a macro)
  /**
   * retrieve the (whole) I2C_CLOCK_STR byte from the Session registers (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getSess_I2C_CLOCK_STR(uint8_t& readBuff) { return(requestSessRegByte(NT3H_COMN_REGS_I2C_CLOCK_STR_BYTE, readBuff)); } // (just a macro)
  /**
   * retrieve the (whole) I2C_CLOCK_STR byte from the Session registers (this version DOES NOT let you check for I2C errors)
   * @return the (whole) I2C_CLOCK_STR byte
   */
  uint8_t getSess_I2C_CLOCK_STR() {
    uint8_t readBuff;   NT3H_ERR_RETURN_TYPE err = getSess_I2C_CLOCK_STR(readBuff);
    if(!_errGood(err)) { NT3HdebugPrint("getSess_I2C_CLOCK_STR() read/write error!"); }
    return(readBuff); // only the LSBit is used, the other 7 bits are RFU
  }

  //// password stuff (NT3H2x11 only): (NOTE: 1x01 to 2x11 change)
  /**
   * retrieve NEG_AUTH_REACHED bit from the I2C_CLOCK_STR Session register
   * @return the NEG_AUTH_REACHED bit (bool)     NEG_AUTH_REACHED indicates whether the AUTHLIM has been reached
   */
  bool getSess_NEG_AUTH_REACHED() {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return((getSess_I2C_CLOCK_STR() & NT3H2x11_NEG_AUTH_REACHED_bits) != 0); } // (just a macro)


  /**
   * retrieve the (whole) NS_REG Session register (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in (see NT3H_NS_REG_xxx_bits defines at top for contents)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getNS_REG(uint8_t& readBuff) { return(requestSessRegByte(NT3H_SESS_REGS_NS_REG_BYTE, readBuff)); } // (just a macro)
  /**
   * retrieve the (whole) NS_REG Session register (this version DOES NOT let you check for I2C errors)
   * @return the (whole) NS_REG (see NT3H_NS_REG_xxx_bits defines at top for contents)
   */
  uint8_t getNS_REG() {
    uint8_t readBuff;   NT3H_ERR_RETURN_TYPE err = getNS_REG(readBuff);
    if(!_errGood(err)) { NT3HdebugPrint("getNS_REG() read/write error!"); }
    return(readBuff);
  }
  /**
   * retrieve NDEF_DATA_READ bit from the NS_REG Session register
   * @return the NDEF_DATA_READ bit (bool)     NDEF_DATA_READ flag is 1 once the RF interface has read the data at address LAST_NDEF_BLOCK (if set). Reading clears flag
   */
  bool getNS_NDEF_DATA_READ() { return((getNS_REG() & NT3H_NS_REG_NDEF_READ_bits) != 0); } // (just a macro)
  /**
   * retrieve I2C_LOCKED bit from the NS_REG Session register
   * @return the I2C_LOCKED bit (bool)     I2C_LOCKED is 1 if I2C has control of memory (arbitration). Should be cleared once the I2C interaction is completely done, may be cleared by WDT
   */
  bool getNS_I2C_LOCKED() { return((getNS_REG() & NT3H_NS_REG_I2C_LOCKED_bits) != 0); } // (just a macro)
  /**
   * retrieve RF_LOCKED bit from the NS_REG Session register
   * @return the RF_LOCKED bit (bool)     RF_LOCKED is 1 if RF has control of memory (arbitration)
   */
  bool getNS_RF_LOCKED() { return((getNS_REG() & NT3H_NS_REG_RF_LOCKED_bits) != 0); } // (just a macro)
  /**
   * retrieve SRAM_I2C_READY bit from the NS_REG Session register
   * @return the SRAM_I2C_READY bit (bool)     SRAM_I2C_READY is 1 if data is ready in SRAM buffer to be READ by I2C (i'm not sure if checking this flag changes it)
   */
  bool getNS_SRAM_I2C_READY() { return((getNS_REG() & NT3H_NS_REG_PTHRU_IN_bits) != 0); } // (just a macro)
  /**
   * retrieve SRAM_RF_READY bit from the NS_REG Session register
   * @return the SRAM_RF_READY bit (bool)     SRAM_RF_READY is 1 if data is ready in SRAM buffer to be READ by RF (the I2C should not need to check this flag, and i'm not sure if checking it clears it)
   */
  bool getNS_SRAM_RF_READY() { return((getNS_REG() & NT3H_NS_REG_PTHRU_OUT_bits) != 0); } // (just a macro)
  /**
   * retrieve EEPROM_WR_ERR bit from the NS_REG Session register
   * @return the EEPROM_WR_ERR bit (bool)     EEPROM_WR_ERR is 1 if there was a (High Voltage?) error during EEPROM write. Flag needs to be manually cleared
   */
  bool getNS_EEPROM_WR_ERR() { return((getNS_REG() & NT3H_NS_REG_EPR_WR_ERR_bits) != 0); } // (just a macro)
  /**
   * retrieve EEPROM_WR_BUSY bit from the NS_REG Session register
   * @return the EEPROM_WR_BUSY bit (bool)     EEPROM_WR_BUSY is 1 if EEPROM writing is in progress (access is disabled while writing)
   */
  bool getNS_EEPROM_WR_BUSY() { return((getNS_REG() & NT3H_NS_REG_EPR_WR_BSY_bits) != 0); } // (just a macro)
  /**
   * retrieve RF_FIELD_PRESENT bit from the NS_REG Session register
   * @return the RF_FIELD_PRESENT bit (bool)     RF_FIELD_PRESENT is 1 if an RF field is detected
   */
  bool getNS_RF_FIELD_PRESENT() { return((getNS_REG() & NT3H_NS_REG_RF_FIELD_bits) != 0); } // (just a macro)

  ///////////////////////////////////// Configuration register get functions: /////////////////////////////////////
  /**
   * (private) retrieve an arbetrary value from the Configuration registers (this version of the function lets you check for I2C errors)
   * @param bytesInBlockStart which register (use NT3H_CONF_SESS_REGS_ENUM enum)
   * @param bytesToRead how many bytes are of interest (size of readBuff)
   * @param readBuff buffer of size (bytesToRead) to put the results in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE _getConfRegBytes(NT3H_CONF_SESS_REGS_ENUM bytesInBlockStart, uint8_t bytesToRead, uint8_t readBuff[], bool useCache = false)
    { return(_getBytesFromBlock(((!isNewVersion) && is2kVariant) ? NT3H1201_CONF_REGS_MEMA : NT3Hxxx1_CONF_REGS_MEMA, bytesInBlockStart, bytesToRead, readBuff, useCache)); }
  /**
   * (private) retrieve an arbetrary value from the Configuration registers (this version DOES NOT let you check for I2C errors)
   * @tparam T type of data to write
   * @param bytesInBlockStart which register (use NT3H_CONF_SESS_REGS_ENUM enum)
   * @param readMSBfirst whether to read the MSByte first or the LSByte first (Big/Little-endian respectively)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return retrieved value (type T), regardless of whether read was successful
   */
  template<typename T> 
  T _getConfRegVal(NT3H_CONF_SESS_REGS_ENUM bytesInBlockStart, bool readMSBfirst=false, bool useCache=false)
    { return(_getValFromBlock<T>(((!isNewVersion) && is2kVariant) ? NT3H1201_CONF_REGS_MEMA : NT3Hxxx1_CONF_REGS_MEMA, bytesInBlockStart, readMSBfirst, useCache)); }

  /**
   * retrieve the (whole) NC_REG Configuration register (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in (see NT3H_NC_REG_xxx_bits defines at top for contents)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getConf_NC_REG(uint8_t& readBuff, bool useCache=false) { return(_getConfRegBytes(NT3H_COMN_REGS_NC_REG_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the (whole) NC_REG Configuration register (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the (whole) NC_REG (see NT3H_NC_REG_xxx_bits defines at top for contents)
   */
  uint8_t getConf_NC_REG(bool useCache=false) { return(_getConfRegVal<uint8_t>(NT3H_COMN_REGS_NC_REG_BYTE, true, useCache)); } // (just a macro)
  /**
   * retrieve NFCS_I2C_RST_ON_OFF bit from the NC_REG Configuration register
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the NFCS_I2C_RST_ON_OFF bit (bool)     NFCS_I2C_RST_ON_OFF enables (NFC-silence (NT3H2x11 only) AND) soft-reset-through-repeated-I2C-starts (very cool, slightly niche)
   */
  bool getConf_NC_NFCS_I2C_RST(bool useCache=false) { return((getConf_NC_REG(useCache) & NT3H_NC_REG_SIL_SRST_bits) != 0); } // (just a macro)
  /**
   * retrieve PTHRU_ON_OFF bit from the NC_REG Configuration register (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the PTHRU_ON_OFF bit (bool)     PTHRU_ON_OFF enables Pass-Through mode
   */
  bool getConf_NC_PTHRU(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("getConf_NC_PTHRU() only available on NT3H2x11"); return(false); } // On NT3H1x01 this bit is RFU and should be kept 0
    return((getConf_NC_REG(useCache) & NT3H_NC_REG_PTHRU_bits) != 0); } // (just a macro)
  /**
   * retrieve FD_OFF bits from the NC_REG Configuration register
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the FD_OFF bits (2)     FD_OFF determines the behaviour of the FD pin (falling)
   */
  NT3H_FD_OFF_ENUM getConf_NC_FD_OFF(bool useCache=false) { return(static_cast<NT3H_FD_OFF_ENUM>((getConf_NC_REG(useCache) & NT3H_NC_REG_FD_OFF_bits) >> 4)); } // (just a macro)
  /**
   * retrieve FD_ON bits from the NC_REG Configuration register
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the FD_ON bits (2)     FD_ON determines the behaviour of the FD pin (rising)
   */
  NT3H_FD_ON_ENUM getConf_NC_FD_ON(bool useCache=false) { return(static_cast<NT3H_FD_ON_ENUM>((getConf_NC_REG(useCache) & NT3H_NC_REG_FD_ON_bits) >> 2)); } // (just a macro)
  /**
   * retrieve SRAM_MIRROR_ON_OFF bit from the NC_REG Configuration register (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the SRAM_MIRROR_ON_OFF bit (bool)     SRAM_MIRROR_ON_OFF enables Memory-Mirror mode
   */
  bool getConf_NC_MIRROR(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("getConf_NC_PTHRU() only available on NT3H2x11"); return(false); } // On NT3H1x01 this bit is RFU and should be kept 0
    return((getConf_NC_REG(useCache) & NT3H_NC_REG_MIRROR_bits) != 0); } // (just a macro)
  /**
   * retrieve TRANSFER_DIR/PTHRU_DIR bit from the NC_REG Configuration register
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the TRANSFER_DIR/PTHRU_DIR bit (bool)     TRANSFER_DIR/PTHRU_DIR determines the direction of data in Pass-Through mode, or can disable RF write-access otherwise
   */
  bool getConf_NC_DIR(bool useCache=false) { return((getConf_NC_REG(useCache) & NT3H_NC_REG_DIR_bits) != 0); } // (just a macro)

  /**
   * retrieve the LAST_NDEF_BLOCK byte from the Configuration registers (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getConf_LAST_NDEF_BLOCK(uint8_t& readBuff, bool useCache=false) { return(_getConfRegBytes(NT3H_COMN_REGS_LAST_NDEF_BLOCK_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the LAST_NDEF_BLOCK byte from the Configuration registers (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the LAST_NDEF_BLOCK byte
   */
  uint8_t getConf_LAST_NDEF_BLOCK(bool useCache=false) { return(_getConfRegVal<uint8_t>(NT3H_COMN_REGS_NC_REG_BYTE, true, useCache)); } // (just a macro)
  /**
   * retrieve the SRAM_MIRROR_BLOCK byte from the Configuration registers (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getConf_SRAM_MIRROR_BLOCK(uint8_t& readBuff, bool useCache=false) { return(_getConfRegBytes(NT3H_COMN_REGS_SRAM_MIRROR_BLOCK_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the SRAM_MIRROR_BLOCK byte from the Configuration registers (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the SRAM_MIRROR_BLOCK byte
   */
  uint8_t getConf_SRAM_MIRROR_BLOCK(bool useCache=false) { return(_getConfRegVal<uint8_t>(NT3H_COMN_REGS_SRAM_MIRROR_BLOCK_BYTE, true, useCache)); } // (just a macro)
  /**
   * retrieve the WatchDog Timer threshold (raw) from the Configuration registers (this version of the function lets you check for I2C errors)
   * @param readBuff 2 byte buffer to put the results in (first byte is LSB)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getConf_WDTraw(uint8_t readBuff[], bool useCache=false) { return(_getConfRegBytes(NT3H_COMN_REGS_WDT_LS_BYTE, 2, readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the WatchDog Timer threshold (raw) from the Configuration registers (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the WatchDog Timer threshold as a uint16_t, multiply with NT3H_WDT_RAW_TO_MICROSECONDS to get microseconds
   */
  uint16_t getConf_WDTraw(bool useCache=false) { return(_getConfRegVal<uint16_t>(NT3H_COMN_REGS_WDT_LS_BYTE, false, useCache)); }
  /**
   * retrieve the WatchDog Timer threshold from the Configuration registers (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the WatchDog Timer threshold in microseconds
   */
  float getConf_WDT(bool useCache=false) { return((float) getConf_WDTraw(useCache) * NT3H_WDT_RAW_TO_MICROSECONDS); } // (just a macro)
  /**
   * retrieve the I2C clock stretching bit from the Configuration registers (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in (may be bool?)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getConf_I2C_CLOCK_STR(uint8_t& readBuff, bool useCache=false) { return(_getConfRegBytes(NT3H_COMN_REGS_I2C_CLOCK_STR_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the I2C clock stretching bit from the Configuration registers (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the I2C clock stretching bit (bool)
   */
  bool getConf_I2C_CLOCK_STR(bool useCache=false) { return(_getConfRegVal<uint8_t>(NT3H_COMN_REGS_I2C_CLOCK_STR_BYTE, true, useCache)); } // (templated as uint8_t then convert to bool) (just a macro)
  /**
   * retrieve the REG_LOCK byte from the Configuration registers (this version of the function lets you check for I2C errors)
   * @param readBuff byte reference to put the result in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getREG_LOCK(uint8_t& readBuff, bool useCache=false) { return(_getConfRegBytes(NT3H_CONF_REGS_REG_LOCK_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the I2C clock stretching bit from the Configuration registers (this version DOES NOT let you check for I2C errors)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the REG_LOCK byte (see NT3H_NC_REG_LOCK_xxx_bits defines up top for contents)
   */
  uint8_t getREG_LOCK(bool useCache=false) { return(_getConfRegVal<uint8_t>(NT3H_CONF_REGS_REG_LOCK_BYTE, true, useCache)); } // (just a macro)


  ///////////////////////////////////// password (related) get functions (NT3H2x11 only): /////////////////////////////////////
  /**
   * retrieve the AUTH0 byte (the address of first block to be protected by password) (this version of the function lets you check for I2C errors) (NT3H2x11 only)
   * @param readBuff byte reference to put the result in
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getAUTH0(uint8_t& readBuff, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_getBytesFromBlock(NT3H2x11_AUTH0_MEMA, NT3H2x11_AUTH0_MEMA_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the AUTH0 byte (the address of first block to be protected by password) (this version DOES NOT let you check for I2C errors) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the address of first block to be protected by password. > 0xEB disables password protection, minimum value is TBD, but assume 0x02 for now (as per page 16 of 2x11 datasheet)
   */
  uint8_t getAUTH0(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return(_getValFromBlock<uint8_t>(NT3H2x11_AUTH0_MEMA, NT3H2x11_AUTH0_MEMA_BYTE, true, useCache)); } // (just a macro)
  /**
   * retrieve the (whole) ACCESS byte (for password stuff) (this version of the function lets you check for I2C errors) (NT3H2x11 only)
   * @param readBuff byte reference to put the result in (see NT3H2x11_ACCESS_xxx_bits defines at top for contents)
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getACCESS(uint8_t& readBuff, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_getBytesFromBlock(NT3H2x11_ACCESS_MEMA, NT3H2x11_ACCESS_MEMA_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the (whole) ACCESS byte (for password stuff) (this version DOES NOT let you check for I2C errors) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the (whole) ACCESS byte (for password stuff) (see NT3H2x11_ACCESS_xxx_bits defines at top for contents)
   */
  uint8_t getACCESS(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return(_getValFromBlock<uint8_t>(NT3H2x11_ACCESS_MEMA, NT3H2x11_ACCESS_MEMA_BYTE, true, useCache)); } // (just a macro)
  /**
   * retrieve NFC_PROT bit from the ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the NFC_PROT bit (bool)     NFC_PROT determines whether read-access is ALSO password protected (instead of only write-access) (from NFC)
   */
  bool getACCESS_NFC_PROT(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return((getACCESS(useCache) & NT3H2x11_ACCESS_NFC_PROT_bits) != 0); } // (just a macro)
  /**
   * retrieve NFC_DIS_SEC1 bit from the ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the NFC_DIS_SEC1 bit (bool)     NFC_DIS_SEC1 disables sector 1 (NFC), effectively making the 2k IC the same as the 1k
   */
  bool getACCESS_NFC_DIS_SEC1(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return((getACCESS(useCache) & NT3H2x11_ACCESS_NFC_DIS_SEC1_bits) != 0); } // (just a macro)
  /**
   * retrieve AUTHLIM bits from the ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the AUTHLIM bits (3)     AUTHLIM limits the number of failed password auth attempts. (3 bit value, 0 = disabled, otherwise max_attemps = 2^(bits) = 1<<bits)
   */
  uint8_t getACCESS_AUTHLIMraw(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return(getACCESS(useCache) & NT3H2x11_ACCESS_AUTHLIM_bits); } // (just a macro)
  /**
   * retrieve AUTHLIM (value) from the ACCESS byte (for password stuff) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the AUTHLIM value, extracted from the 3bit value (3)     AUTHLIM limits the number of failed password auth attempts. 0 = disabled, 1~128 = number of attempts
   */
  uint8_t getACCESS_AUTHLIM(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    uint8_t AUTHLIMraw = getACCESS_AUTHLIMraw(useCache);
    return(AUTHLIMraw ? (1 << AUTHLIMraw) : 0); // see page 37 of 2x11 datasheet
  }
  /**
   * retrieve the (whole) PT_I2C byte (for password stuff) (this version of the function lets you check for I2C errors) (NT3H2x11 only)
   * @param readBuff byte reference to put the result in (see NT3H2x11_PT_I2C_xxx_bits defines at top for contents)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote/read successfully
   */
  NT3H_ERR_RETURN_TYPE getPT_I2C(uint8_t& readBuff, bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
    return(_getBytesFromBlock(NT3H2x11_PT_I2C_MEMA, NT3H2x11_PT_I2C_MEMA_BYTE, 1, &readBuff, useCache)); } // (just a macro)
  /**
   * retrieve the (whole) PT_I2C byte (for password stuff) (this version DOES NOT let you check for I2C errors) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the (whole) PT_I2C byte (for password stuff) (see NT3H2x11_PT_I2C_xxx_bits defines at top for contents)
   */
  uint8_t getPT_I2C(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return(_getValFromBlock<uint8_t>(NT3H2x11_PT_I2C_MEMA, NT3H2x11_PT_I2C_MEMA_BYTE, true, useCache)); } // (just a macro)
  /**
   * retrieve  bit from the PT_I2C byte (for password stuff) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the  bit (bool)     
   */
  bool getPT_I2C_2K_PROT(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return((getPT_I2C(useCache) & NT3H2x11_PT_I2C_2K_PROT_bits) != 0); } // (just a macro)
  /**
   * retrieve  bit from the PT_I2C byte (for password stuff) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the  bit (bool)     
   */
  bool getPT_I2C_SRAM_PROT(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return((getPT_I2C(useCache) & NT3H2x11_PT_I2C_SRAM_PROT_bits) != 0); } // (just a macro)
  /**
   * retrieve  bits from the PT_I2C byte (for password stuff) (NT3H2x11 only)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return the  bits (2)     
   */
  uint8_t getPT_I2C_I2C_PROT(bool useCache=false) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return(0); }
    return(getPT_I2C(useCache) & NT3H2x11_PT_I2C_I2C_PROT_bits); } // (just a macro)

/////////////////////////////////////////////////////////////////////////////////////// debug functions: //////////////////////////////////////////////////////////

  /**
   * checks if retrieving the UID works without errors, AND if the first byte matches the manufacturer ID
   * @return true if reading was successful and ...
   */
  bool connectionCheck() {
    uint8_t UID[7];
    NT3H_ERR_RETURN_TYPE err = getUID(UID); // fetch the Serial Number a.k.a. UID, the first byte of which should always be the manufacturer ID
    if(!_errGood(err)) { return(false); } // at this point it will have already printed several debug messages, no need to print another
    return(UID[0] == NT3H_SERIAL_NR_NXP_MF_ID);
  }
  /**
   * checks whether the reported memory size matches the expected memory size byte
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return true if reading was successful and ...
   */
  bool variantCheck(bool useCache=false) {
    uint8_t readBuff[4];
    NT3H_ERR_RETURN_TYPE err = getCC(readBuff, useCache); // fetch the Serial Number a.k.a. UID, the 3rd byte of which indicates the size of the tag
    if(!_errGood(err)) { NT3HdebugPrint("variantCheck() read/write error!"); return(false); }
    bool returnBool = (readBuff[2] == NT3H_CAPA_CONT_DEFAULT[is2kVariant][2]);
    if(!returnBool) { NT3HdebugPrint("variantCheck(), CC/NDEF memory size byte did NOT match expectation!"); }
    return(returnBool);
  }

  // uint8_t UIDsizeCheck() { uint8_t tempArr[2]; getATQA(tempArr); return((tempArr[0] >= 0x40) ? 7 : 4); } // getATQA and interpret the 2 bits that indicate the size of the UID. Works, but logic is guesswork


  // /**
  //  * print out a number of current settings (Session registers and a few other things), in a legible/explanatory fashion
  //  * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
  //  */
  // NT3H_ERR_RETURN_TYPE printConfig() {
  //   // TODO!
  // }

  /**
   * write the defualt values (according to the datasheets) to the Configuration registers. NOTE: no longer possible after burnRegLockI2C
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE resetConfiguration(bool useCache=false) {
    uint8_t tempArr[6];  for(uint8_t i=0;i<6;i++) { tempArr[i]=NT3H_CONF_REGS_DEFAULT[i]; } // this copy is needed, becuase DEFAULT array is const, and the function only likes non-const data...
    return(_setBytesInBlock(((!isNewVersion) && is2kVariant) ? NT3H1201_CONF_REGS_MEMA : NT3Hxxx1_CONF_REGS_MEMA, NT3H_COMN_REGS_NC_REG_BYTE, 6, tempArr, useCache)); // set all bytes (except REG_LOCK) to their default value
  }
  /**
   * save the current Session registers in EEPROM (by writing them to the Configuration registers). NOTE: setConf_I2C_CLOCK_STR() must be called seperately, as it's a Read-only part of the Session registers
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE saveSessionToConfiguration(bool useCache=false) {
    //// read from Sess:
    uint8_t tempArr[5];   NT3H_ERR_RETURN_TYPE err;
    for(uint8_t i=0;i<5;i++) {
      err = requestSessRegByte(static_cast<NT3H_CONF_SESS_REGS_ENUM>(NT3H_COMN_REGS_NC_REG_BYTE + i), tempArr[i]); // NOTE: if enum values were not contiguous, this would not work
      if(!_errGood(err)) { NT3HdebugPrint("saveSessionToConfiguration() read error!"); return(err); }
    }
    if(!isNewVersion) { tempArr[0] &= ~NT3H1x01_NC_REG_RFU_bits; } // some bits are Reserved for Future Use, and must be kept at 0 (according to the datasheet)
    //// write to Conf:
    return(_setBytesInBlock(((!isNewVersion) && is2kVariant) ? NT3H1201_CONF_REGS_MEMA : NT3Hxxx1_CONF_REGS_MEMA, NT3H_COMN_REGS_NC_REG_BYTE, 5, tempArr, useCache));
  }
  /**
   * copy first 6 bytes (and set 7th to default) from Configuration registers to Session registers. This is done at boot, this function just repeats it manually (alternatively, just reset IC)
   * @param useCache (optional!, not recommended, use at own discretion) fetch data from _oneBlockBuff cache (if possible) instead of actually reading it from I2C (to save a little time).
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE reloadConfiguration(bool useCache=false) {
    //// read from Sess:
    uint8_t tempArr[6];  NT3H_ERR_RETURN_TYPE err = _getConfRegBytes(NT3H_COMN_REGS_NC_REG_BYTE, 6, tempArr, useCache);
    if(!_errGood(err)) { NT3HdebugPrint("reloadConfiguration() read error!"); return(err); }
    //// write to Conf:
    for(uint8_t i=0;i<5;i++) {
      err = writeSessRegByte(static_cast<NT3H_CONF_SESS_REGS_ENUM>(NT3H_COMN_REGS_NC_REG_BYTE + i), tempArr[i]);
      if(!_errGood(err)) { NT3HdebugPrint("reloadConfiguration() write error!"); return(err); }
    }
    return(err); // returns an OK value, if it made it to this point
  }

  /**
   * write the defualt value (according to the datasheets) to the CC bytes, indicating the size of the card (make sure to initialize class object appropriately (is2kVariant))
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE resetCC() { return(setCC(NT3H_CAPA_CONT_DEFAULT_uint32_t[is2kVariant])); } // write it as uint16_t (just a macro)
  // NT3H_ERR_RETURN_TYPE resetCC() { uint8_t tempArr[4]; for(uint8_t i=0;i<4;i++){tempArr[i]=NT3H_CAPA_CONT_DEFAULT[is2kVariant][i];} return(setCC(tempArr)); } // write individual bytes



  //// password stuff (NT3H2x11 only): (NOTE: 1x01 to 2x11 change)
  /**
   * store the password and PACK (password acknowledge) in a buffer. This is required to safely write to only-parts-of block 0x39 (which also contains ACCESS and PT_I2C, for example) (NT3H2x11 only)
   * @param passwordBuff 4 byte buffer containing new password. Password should be LSByte first, see page 36 of 2x11 datasheet
   * @param PACKbuff 2 byte buffer containing password acknowledge value. I'm currently not 100% sure what this does...
   */
  void storePWD_and_PACK(uint8_t passwordBuff[], uint8_t PACKbuff[]) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return; }
    for(uint8_t i=0; i<4; i++) { _passwordAndPackBuff[i] = passwordBuff[i]; }
    for(uint8_t i=0; i<2; i++) { _passwordAndPackBuff[i+4] = PACKbuff[i]; }
    _passwordAndPackStored = true; // indicate (to _setBytesInBlock()) that the password is initialized (whether it's correct is irrelevant, as long as it's set by the user)
  }
  /**
   * store the password and PACK (password acknowledge) in a buffer. This is required to safely write to only-parts-of block 0x39 (which also contains ACCESS and PT_I2C, for example) (NT3H2x11 only)
   * @param password new password as a 32bit value. Stored LSByte first, but just to be sure, try using the buffer version instead
   * @param PACK password acknowledge value as a 16bit value. Stored LSByte first, but just to be sure, try using the buffer version instead
   */
  void storePWD_and_PACK(uint32_t password, uint16_t PACK) {
    if(!isNewVersion) { NT3HdebugPrint("password functions are only available on NT3H2x11"); return; }
    //// i really hope i did the LSByte stuff correctly (and that it stays correct, regarding big/little-endian microcontrollers)
    uint8_t* bytePtrToNewVal = (uint8_t*) &password;
    for(uint8_t i=0; i<4; i++) { _passwordAndPackBuff[i] = bytePtrToNewVal[i]; } // (little-endian(?))
    bytePtrToNewVal = (uint8_t*) &PACK;
    for(uint8_t i=0; i<2; i++) { _passwordAndPackBuff[i+4] = bytePtrToNewVal[i]; }
  }
};

#endif  // NT3H_thijs_h


/*
configuration registers, as read through RF (hex): 10.00.F8.48.08.01.00.00
i was not able to switch sectors from the NFC app i used, so the session registers remain a mystery (from RF)

*/