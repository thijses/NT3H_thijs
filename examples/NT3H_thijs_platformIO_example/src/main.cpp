/*

*/

#include <Arduino.h>


// #define NT3H_useWireLib  // force the use of Wire.h (instead of platform-optimized code (if available))

#define NT3HdebugPrint(x)  Serial.println(x)    //you can undefine these printing functions no problem, they are only for printing I2C errors
//#define NT3HdebugPrint(x)  log_d(x)  // ESP32 style logging

// #define NT3H_unlock_burning

#include <NT3H_thijs.h>

// NT3H_thijs NFCtag(  false  ,  false  ); // initialize NT3H1101 (1k variant of old IC)
// NT3H_thijs NFCtag(  false  ,  true  ); // initialize NT3H1201 (1k variant of old IC)
NT3H_thijs NFCtag(  true  ,  false  ); // initialize NT3H2111 (1k variant of new IC)
// NT3H_thijs NFCtag(  true  ,  true  ); // initialize NT3H2211 (1k variant of new IC)


#ifdef ARDUINO_ARCH_ESP32  // on the ESP32, almost any pin can become an I2C pin
  const uint8_t NT3H_SDApin = 26; // 'defualt' is 21 (but this is just some random value Arduino decided.)
  const uint8_t NT3H_SCLpin = 27; // 'defualt' is 22 
#endif
#ifdef ARDUINO_ARCH_STM32   // on the STM32, each I2C peripheral has several pin options
  const uint8_t NT3H_SDApin = SDA; // default pin, on the STM32WB55 (nucleo_wb55rg_p) that's pin PB9
  const uint8_t NT3H_SCLpin = SCL; // default pin, on the STM32WB55 (nucleo_wb55rg_p) that's pin PB8
  /* Here is a handy little table of I2C pins on the STM32WB55 (nucleo_wb55rg_p):
      I2C1: SDA: PA10, PB7, PB9
            SCL: PA9, PB6, PB8
      I2C3: SDA: PB4, PB11, PB14, PC1
            SCL: PA7, PB10, PB13, PC0      */
  
  #define LEDpin   PB1  // PB1 is one of the LEDs on the STM32WB55 Nucleo-64 board, as well as my custom PCB
#endif

#ifdef NT3H_useWireLib // (currently) only implemented with Wire.h
  bool checkI2Caddress(uint8_t address) {
    Wire.beginTransmission(address);
    return(Wire.endTransmission() == 0);
  }

  void I2CdebugScan() {
    Serial.println("I2C debug scan...");
    for(uint8_t address = 1; address<127; address++) {
      if(checkI2Caddress(address)) {
        Serial.print("got ACK at address: 0x"); Serial.println(address, HEX);
      }
      delay(1);
    }
    Serial.println("scanning done");
  }
#endif

void setup() 
{
  // delay(2000);
  #ifdef LEDpin
    pinMode(LEDpin, OUTPUT);
  #endif


  Serial.setRx(PA10);  Serial.setTx(PA9); // PCB R01



  Serial.begin(115200);  delay(50); Serial.println();
  #ifdef NT3H_useWireLib // the slow (but pretty universally compatible) way
    NFCtag.init(100000); // NOTE: it's up to the user to find a frequency that works well.
  #elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__) // TODO: test 328p processor defines! (also, this code may be functional on other AVR hw as well?)
    pinMode(SDA, INPUT_PULLUP); //A4    NOT SURE IF THIS INITIALIZATION IS BEST (external pullups are strongly recommended anyways)
    pinMode(SCL, INPUT_PULLUP); //A5
    NFCtag.init(100000); // your average atmega328p should do 800kHz
  #elif defined(ARDUINO_ARCH_ESP32)
//    pinMode(NT3H_SDApin, INPUT_PULLUP); //not needed, as twoWireSetup() does the pullup stuff for you
//    pinMode(NT3H_SCLpin, INPUT_PULLUP);
    esp_err_t initErr = NFCtag.init(100000, NT3H_SDApin, NT3H_SCLpin, 0); //on the ESP32 (almost) any pins can be I2C pins
    if(initErr != ESP_OK) { Serial.print("I2C init fail. error:"); Serial.println(esp_err_to_name(initErr)); Serial.println("while(1){}..."); while(1) {} }
    //note: on the ESP32 the actual I2C frequency is lower than the set frequency (by like 20~40% depending on pullup resistors, 1.5kOhm gets you about 800kHz)
  #elif defined(__MSP430FR2355__) //TBD: determine other MSP430 compatibility: || defined(ENERGIA_ARCH_MSP430) || defined(__MSP430__)
    // not sure if MSP430 needs pinMode setting for I2C, but it seems to work without just fine.
    NFCtag.init(100000); // TODO: test what the limit of this poor microcontroller are ;)
    delay(50);
  #elif defined(ARDUINO_ARCH_STM32)
    // not sure if STM32 needs pinMode setting for I2C
    NFCtag.init(100000, SDA, SCL, false); // TODO: test what the limits of this poor microcontroller are ;)
  #else
    #error("should never happen, NT3H_useWireLib should have automatically been selected if your platform isn't one of the optimized ones")
  #endif

  #ifdef NT3H_useWireLib
    I2CdebugScan();
    Serial.println(); // seperator
  #endif

  //// first, some basic checks. NOTE: will halt entire sketch if something's wrong
  if(!NFCtag.connectionCheck()) { Serial.println("NT3H connection check failed!");    while(1);    } else { Serial.println("connection good"); }
  Serial.print("UID: "); uint8_t UID[7]; NFCtag.getUID(UID); for(uint8_t i=0; i<7; i++) { Serial.print(UID[i], HEX); Serial.print(' '); } Serial.println();
  //if(!NFCtag.variantCheck()) { Serial.println("resetting CC to factory default..."); NFCtag.resetCC(); } // you can only change the CC bytes from I2C (not RF)
  Serial.print("CC as bytes: "); uint8_t CC[4]; NFCtag.getCC(CC); for(uint8_t i=0; i<4; i++) { Serial.print(CC[i], HEX); Serial.print(' '); }
  Serial.print("  should be: "); Serial.println(NT3H_CAPA_CONT_DEFAULT_uint32_t[NFCtag.is2kVariant], HEX);
  if(!NFCtag.variantCheck()) { Serial.println("NT3H variant check failed!");    while(1);     } else  { Serial.println("variant good"); }
  Serial.println(); // seperator

  // //// address change test:
  // #ifdef NT3H_useWireLib
  //   I2CdebugScan();
  //   uint8_t newAddress = NT3H_DEFAULT_I2C_ADDRESS;
  //   Serial.print("\t attempting to change I2C address to: 0x"); Serial.println(newAddress, HEX);
  //   NFCtag.setI2Caddress(newAddress); delay(20);
  //   I2CdebugScan();
  //   if(!NFCtag.connectionCheck()) { Serial.println("post-address-change NT3H connection check failed!"); while(1);} else { Serial.println("post-address-change connection good"); }
  // #endif

  //// ATQA & SAK tests:
  Serial.print("getATQA: 0x"); Serial.println(NFCtag.getATQA(), HEX);
  Serial.print("getSAK: 0x"); Serial.println(NFCtag.getSAK(), HEX);
  Serial.println(); // seperator

  //// Session register tests:
  Serial.print("getSess_NC_REG: 0b"); Serial.print(NFCtag.getSess_NC_REG(), BIN); Serial.print("  default: 0b"); Serial.println(NT3H_SESS_REGS_DEFAULT[0], BIN);
  Serial.print("getSess_NC_NFCS_I2C_RST: "); Serial.println(NFCtag.getSess_NC_NFCS_I2C_RST());
  Serial.print("getSess_NC_PTHRU: "); Serial.println(NFCtag.getSess_NC_PTHRU());
  Serial.print("getSess_NC_FD_OFF: "); Serial.println(NFCtag.getSess_NC_FD_OFF());
  Serial.print("getSess_NC_FD_ON: "); Serial.println(NFCtag.getSess_NC_FD_ON());
  Serial.print("getSess_NC_MIRROR: "); Serial.println(NFCtag.getSess_NC_MIRROR());
  Serial.print("getSess_NC_DIR: "); Serial.println(NFCtag.getSess_NC_DIR());
  Serial.println(); // seperator
  Serial.print("getSess_LAST_NDEF_BLOCK: 0x"); Serial.print(NFCtag.getSess_LAST_NDEF_BLOCK(), HEX); Serial.print("  default: 0x"); Serial.println(NT3H_SESS_REGS_DEFAULT[1], HEX);
  Serial.print("getSess_SRAM_MIRROR_BLOCK: 0x"); Serial.print(NFCtag.getSess_SRAM_MIRROR_BLOCK(), HEX); Serial.print("  default: 0x"); Serial.println(NT3H_SESS_REGS_DEFAULT[2], HEX);
  // Serial.print("getSess_WDTraw: 0x"); Serial.println(NFCtag.getSess_WDTraw(), HEX);
  Serial.print("getSess_WDT: "); Serial.print(NFCtag.getSess_WDT()); Serial.println("us  default is ~20ms");
  Serial.print("getSess_I2C_CLOCK_STR: "); Serial.print(NFCtag.getSess_I2C_CLOCK_STR()); Serial.print("  default: "); Serial.println(NT3H_SESS_REGS_DEFAULT[5]); // (NOTE: 1x01 to 2x11 change)
  if(NFCtag.isNewVersion) { Serial.print("getSess_NEG_AUTH_REACHED: "); Serial.println(NFCtag.getSess_NEG_AUTH_REACHED()); }
  Serial.println(); // seperator
  Serial.print("getNS_REG: 0b"); Serial.print(NFCtag.getNS_REG(), BIN); Serial.print("  default: 0b"); Serial.println(NT3H_SESS_REGS_DEFAULT[6], BIN);
  Serial.print("getNS_NDEF_DATA_READ: "); Serial.println(NFCtag.getNS_NDEF_DATA_READ());
  Serial.print("getNS_I2C_LOCKED: "); Serial.println(NFCtag.getNS_I2C_LOCKED());
  Serial.print("getNS_RF_LOCKED: "); Serial.println(NFCtag.getNS_RF_LOCKED());
  Serial.print("getNS_SRAM_I2C_READY: "); Serial.println(NFCtag.getNS_SRAM_I2C_READY());
  Serial.print("getNS_SRAM_RF_READY: "); Serial.println(NFCtag.getNS_SRAM_RF_READY());
  Serial.print("getNS_EEPROM_WR_ERR: "); Serial.println(NFCtag.getNS_EEPROM_WR_ERR());
  Serial.print("getNS_EEPROM_WR_BUSY: "); Serial.println(NFCtag.getNS_EEPROM_WR_BUSY());
  Serial.print("getNS_RF_FIELD_PRESENT: "); Serial.println(NFCtag.getNS_RF_FIELD_PRESENT());
  Serial.println(); // seperator

  //// Configuration register tests:
  Serial.print("getConf_NC_REG: 0b"); Serial.print(NFCtag.getConf_NC_REG(), BIN); Serial.print("  default: 0b"); Serial.println(NT3H_CONF_REGS_DEFAULT[0], BIN);
  Serial.print("getConf_NC_NFCS_I2C_RST: "); Serial.println(NFCtag.getConf_NC_NFCS_I2C_RST());
  if(NFCtag.isNewVersion) { Serial.print("getConf_NC_PTHRU: "); Serial.println(NFCtag.getConf_NC_PTHRU()); } // (NOTE: 1x01 to 2x11 change)
  Serial.print("getConf_NC_FD_OFF: "); Serial.println(NFCtag.getConf_NC_FD_OFF());
  Serial.print("getConf_NC_FD_ON: "); Serial.println(NFCtag.getConf_NC_FD_ON());
  if(NFCtag.isNewVersion) { Serial.print("getConf_NC_MIRROR: "); Serial.println(NFCtag.getConf_NC_MIRROR()); } // (NOTE: 1x01 to 2x11 change)
  Serial.print("getConf_NC_DIR: "); Serial.println(NFCtag.getConf_NC_DIR());
  Serial.println(); // seperator
  Serial.print("getConf_LAST_NDEF_BLOCK: 0x"); Serial.print(NFCtag.getConf_LAST_NDEF_BLOCK(), HEX); Serial.print("  default: 0x"); Serial.println(NT3H_CONF_REGS_DEFAULT[1], HEX);
  Serial.print("getConf_SRAM_MIRROR_BLOCK: 0x"); Serial.print(NFCtag.getConf_SRAM_MIRROR_BLOCK(), HEX); Serial.print("  default: 0x"); Serial.println(NT3H_CONF_REGS_DEFAULT[2], HEX);
  // Serial.print("getConf_WDTraw: 0x"); Serial.println(NFCtag.getConf_WDTraw(), HEX);
  Serial.print("getConf_WDT: "); Serial.print(NFCtag.getConf_WDT()); Serial.println("us  default is ~20ms");
  Serial.print("getConf_I2C_CLOCK_STR: "); Serial.print(NFCtag.getConf_I2C_CLOCK_STR()); Serial.print("  default: "); Serial.println(NT3H_CONF_REGS_DEFAULT[5]);
  Serial.println(); // seperator
  Serial.print("getREG_LOCK: 0b"); Serial.print(NFCtag.getREG_LOCK(), BIN); Serial.print("  default: 0b"); Serial.println(NT3H_CONF_REGS_DEFAULT[6], BIN);
  Serial.println(); // seperator
  
  //// password stuff (NT3H2x11 only): (NOTE: 1x01 to 2x11 change)
  if(NFCtag.isNewVersion) { // NT3H2x11 only!
    Serial.print("getAUTH0: 0x"); Serial.print(NFCtag.getAUTH0(), HEX); \
      Serial.print((NFCtag.getAUTH0() <= NT3H2x11_AUTH0_DISABLE_THRESH) ? " (enabled)" : " (disabled)"); \
      Serial.println("  default: 0xEC~0xFF (disabled)");
    Serial.print("getACCESS: 0b"); Serial.println(NFCtag.getACCESS(), BIN);
    Serial.print("getACCESS_NFC_PROT: "); Serial.println(NFCtag.getACCESS_NFC_PROT());
    Serial.print("getACCESS_NFC_DIS_SEC1: "); Serial.println(NFCtag.getACCESS_NFC_DIS_SEC1());
    Serial.print("getACCESS_AUTHLIMraw: 0b"); Serial.println(NFCtag.getACCESS_AUTHLIMraw(), BIN);
    Serial.print("getACCESS_AUTHLIM: "); Serial.println(NFCtag.getACCESS_AUTHLIM());
    Serial.print("getPT_I2C: 0b"); Serial.println(NFCtag.getPT_I2C(), BIN);
    Serial.print("getPT_I2C_2K_PROT: "); Serial.println(NFCtag.getPT_I2C_2K_PROT());
    Serial.print("getPT_I2C_SRAM_PROT: "); Serial.println(NFCtag.getPT_I2C_SRAM_PROT());
    Serial.print("getPT_I2C_I2C_PROT: "); Serial.println(NFCtag.getPT_I2C_I2C_PROT());
    Serial.println();

  //   //// setting the password is slightly tricky
  //   //// I am not sure how the authentication works from I2C (only), and i'm not sure when the password is considered 'active'
  //   //// I have my traditional partial-block writing functions:
  //   uint32_t password = 0x04030201;  uint16_t PACK = 0x0605;
  //   //// the class needs to know the password and pack, sothat it knows what to overwrite the block with
  //   NFCtag.storePWD_and_PACK(password, PACK); // use storePWD_and_PACK() to make sure partial-block writes are possible
  //   //// or you can just write it (and the class will make sure to store the stuff automatically)
  // //  NFCtag.setPWD_and_PACK(password, PACK); // equivalent to:  NFCtag.storePWD_and_PACK(password, PACK); NFCtag._writePWD_and_PACK();
  //   //// but I suspect it's probably best practice to write the whole block once.
  //   ////  to do that, i re-used a struct i once used for a communication protocol (_commStruct), to make the passwordBlock struct
  //   //// you can initialize empty, then fill it with data
  //   passwordBlock testOne;  testOne.PWD(0) = 0x01; testOne.PWD(1) = 0x02; testOne.PWD(2) = 0x03; testOne.PWD(3) = 0x04;   testOne.PACK(0) = 0x05; testOne.PACK(1) = 0x06; // struct reference setting values directly
  //   passwordBlock testTwo;  testTwo.PWD_uint32_t() = password;  testTwo.PACK_uint16_t() = PACK; // struct reference copying from values
  //   passwordBlock testThree = {0x00, NFCtag._passwordAndPackBuff, &NFCtag._passwordAndPackBuff[4], 0x00}; // stuct construct from buffer
  //   passwordBlock testFour = {0x00, password, PACK, 0x00}; // stuct construct from values
  //   //// there's also some quick functions to set the individual bits:
  //   testFour.setACCESS_NFC_PROT(1);  //testFour.setACCESS_AUTHLIMraw(0b00000111);
  //   //// when you're ready to send it, use this dedicated function:
  // //  NFCtag.writePasswordBlock(testFour);
  //   //// just to confirm the LSByte-first data structure required for the password, all of these should print '1 2 3 4 5 6'
  //   for(uint8_t i=0; i<4; i++) { Serial.print(((uint8_t*)&password)[i], HEX); Serial.print('\t'); }  for(uint8_t i=0; i<2; i++) { Serial.print(((uint8_t*)&PACK)[i], HEX); Serial.print('\t'); } Serial.println();
  //   for(uint8_t i=0; i<6; i++) { Serial.print(NFCtag._passwordAndPackBuff[i], HEX); Serial.print('\t'); } Serial.println();
  //   for(uint8_t i=0; i<6; i++) { Serial.print(testOne._data[i+NT3H_PWD_MEMA_BYTES_START], HEX); Serial.print('\t'); } Serial.println();
  //   for(uint8_t i=0; i<6; i++) { Serial.print(testTwo._data[i+NT3H_PWD_MEMA_BYTES_START], HEX); Serial.print('\t'); } Serial.println();
  //   for(uint8_t i=0; i<6; i++) { Serial.print(testThree._data[i+NT3H_PWD_MEMA_BYTES_START], HEX); Serial.print('\t'); } Serial.println();
  //   for(uint8_t i=0; i<6; i++) { Serial.print(testFour._data[i+NT3H_PWD_MEMA_BYTES_START], HEX); Serial.print('\t'); } Serial.println();
  //   Serial.println();

  } // NT3H2x11 only stuff

  //// Reading/Writing to the bulk memory (a.k.a. user-memory):
  //// NOTE: some more user-friendly functions for this are on the way!
  //// The User-memory stretches from blocks 0x01 to (the first half of) 0x38 on the 1k version, and to 0x77 on the 2k version.
  //// the functions (not yet user-friendly) to access this memory should be something like:
  //requestMemBlock(blockAddress, yourBuffHere);     which is the same as    _getBytesFromBlock(blockAddress, 0, NT3H_BLOCK_SIZE, yourBuffHere);    for reading blocks
  //writeMemBlock(blockAddress, yourBuffHere);       which is the same as      _setBytesInBlock(blockAddress, 0, NT3H_BLOCK_SIZE, yourBuffHere);    for writing blocks
  //// to read/write WHOLE blocks of memory efficiently, just make sure your readBuff/writeBuff is the size of NT3H_BLOCK_SIZE. The _getBytes...() functions will recognize this and skip the _oneBlockBuff
  //// to indicate the size of the contents of the user-memory, the LAST_NDEF_BLOCK is used.
  //// user-friendly LAST_NDEF_BLOCK function(s) are still TBD, but setSess_LAST_NDEF_BLOCK, getSess_LAST_NDEF_BLOCK, setConf_LAST_NDEF_BLOCK and getConf_LAST_NDEF_BLOCK should be working.
  //// also:
  //// Filesystem / SD-card streaming are still TBD (, but i would very much like to be able to transfer large files more easily, and that filesystems (ESP32,STM32?) or SD-cards seem like a handy way to do that)
  
  //// many of these user-friendly functions are somewhat inefficient.
  //// Especially, those that change one small part of a block of memory, which require the entire block to be fetched first (to avoid unintentionally changing other data in the same block).
  //// For this reason, i've added 'useCache' to a lot of functions,
  ////  which lets you potentially skip a read interaction, by using the cached memory block instead. NOTE: this does not apply for session registers, which use masked-single-byte interactions by definition.
  //// HOWEVER, this is very susceptible to (user) error, and should only be used if you are sure of what you're doing.
  //// The _oneBlockBuff (class member) is the last block of memory the class interacted with, and _oneBlockBuffAddress indicates what memory block (address) the cache (supposedly) holds.
  //// There are NO checks for how old the cache is, and it is entirely possible that the RF interface changes stuff in memory, making the cache invalid (this is NOT checked (checking is not possible (efficiently)))
  //// here is an example of a reasonable usage of the useCache functionality:
  // NFCtag.getCC(CC); // first command should (almost) never useCache (because we don't know how long it's)
  // NFCtag.getUID(UID, true); // commands immidietly following another, where the contents share a memory block (UID and CC are both found in block 0x00), can use the cache effectively
  //// the code above is especially permissable, because the UID is Read-only, and therefore pretty unlikely to become desynchronized (by the RF side, for example)

  //// memory access arbitration and the WatchDog Timer (IMPORTANT):
  //// the RF and I2C interfaces cannot access the EEPROM of the tag at the same time, so there are _LOCKED flags in place (basically semaphore flags).
  //// after your I2C interactions are completed, the polite thing is to clear the I2C_LOCKED flag (in the NS_REG), using setNS_I2C_LOCKED(false);
  //// HOWEVER, the designers of this IC accounted for lazy people forgetting to clear the flag, and implemented a WatchDog Timer as well.
  //// The WDT will clear the flag for you, after a certain period of I2C inactivity (or after the first I2C START condition, it's not clearly specified in the datasheet)
  //// To set the WDT time, use setSess_WDT(float) or setSess_WDTraw(uint16_t).

  //// Configuration saving:
  //// If you are content with your settings, but don't like specifying them on every Power-On-Reset,
  ////  you can save the active settings (Session registers) to EEPROM (Configuration regisers) with the function saveSessionToConfiguration()
  //// if you are REALLY sure of yourself, and absolutely distrusting of people-with-access-to-the-RF-interface, you can (permanently!!!) disable writing Conf. registers from RF, using burnRegLockRF()
  ////  furthermore, if you are even distrustfull of people-with-access-to-the-I2C-interface, you can (permanently!!!) disable writing Conf. registers from I2C, using burnRegLockI2C()
  //// for obvious reasons, these One-Time-Program functions are not available in this library UNLESS, you #define NT3H_unlock_burning

  //// preventing RF write-access:
  //// Unfortunately, the functions for Static Locking and Dynamic Locking are currently unfinished, but I will hint that that is the way to do it ;)

  //// Pass-Through mode and Memory-Mirror mode functions are still TBD.
  //// (if you really want it enough, the low-level functions are in place, you would just need to read the datasheet thurroughly. If you do so before I do, please tell me on Github :) @ https://github.com/thijses )

  //// if functionality is still missing, or if you just have some ideas for improvements you'd like to see, feel free to contact me: https://github.com/thijses  or  tttthijses@gmail.com
}

void loop()
{
  if(NFCtag.getNS_RF_FIELD_PRESENT()) { // loop demonstration is still TBD, but this will at least show whether there IS a card present or not (much like the FD pin)
    Serial.println("field detected!");
    #ifdef LEDpin
      digitalWrite(LEDpin, HIGH);
    #endif
  } else {
    #ifdef LEDpin
      digitalWrite(LEDpin, LOW);
    #endif
  }
  delay(25);
}