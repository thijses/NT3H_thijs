
#ifndef _NT3H_thijs_base_h
#define _NT3H_thijs_base_h

#include "Arduino.h" // always import Arduino.h

#include "NT3H_thijs.h" // (i feel like this constitutes a cicular dependency, but the compiler doesn't seem to mind)


#ifndef NT3H_useWireLib // (note: ifNdef!) if this has been defined by the user, then don't do all this manual stuff
  #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__) // TODO: test 328p processor defines! (also, this code may be functional on other AVR hw as well?)
    // nothing to import, all ATMega328P registers are imported by default
  #elif defined(ARDUINO_ARCH_ESP32)
    #include "driver/i2c.h"
  #elif defined(__MSP430FR2355__) //TBD: determine other MSP430 compatibility: || defined(ENERGIA_ARCH_MSP430) || defined(__MSP430__)
    #include <msp430.h>
    extern "C" {
      #include "twi.h"
    }
  #elif defined(ARDUINO_ARCH_STM32)
    extern "C" {
      #include "utility/twi.h"
    }
  #else // if the platform does not have optimized code
    #warning("using Wire library for NT3H (no platform optimized code available)")
    #define NT3H_useWireLib  // use Wire as a backup
  #endif
#endif

#ifdef NT3H_useWireLib // note: this ifdef check is done after the ifndef, so the compiler gets a chance to define it anyway
  #include <Wire.h>
  // note: check the defined BUFFER_LENGTH in Wire.h for the max transmission length (on many platforms)
#endif

/*  this library can use several different error return types, depending on the platform (and user preference)
the ESP32 has a general error type, which includes error for I2C
the STM32 has an enum for I2C errors
otherwise, just a boolean, indicating general faillure should be enough, especially if you use NT3HdebugPrint() effectively
4 things should be defined: 
- the xxx_ERR_RETURN_TYPE
- a basic OK value, like xxx_ERR_RETURN_TYPE_OK
- a non-I2C error, like xxx_ERR_RETURN_TYPE_OK
- something to indicate what the error type is (without having to compare the ERR_RETURN_TYPE itself), like xxx_return_esp_err_t or xxx_return_i2c_status_e

*/
#ifndef NT3H_ERR_RETURN_TYPE  // unless the user already defined it manually
  #define NT3H_ERR_RETURN_TYPE_default  bool
  #define NT3H_ERR_RETURN_TYPE_default_OK  true
  #define NT3H_ERR_RETURN_TYPE_default_FAIL false
  #ifdef NT3H_useWireLib
    #define NT3H_ERR_RETURN_TYPE  NT3H_ERR_RETURN_TYPE_default
    #define NT3H_ERR_RETURN_TYPE_OK  NT3H_ERR_RETURN_TYPE_default_OK
    #define NT3H_ERR_RETURN_TYPE_FAIL  NT3H_ERR_RETURN_TYPE_default_FAIL
  #elif defined(ARDUINO_ARCH_ESP32) // the ESP32 likes to spit out esp_err_t for most things
    #define NT3H_ERR_RETURN_TYPE  esp_err_t
    #define NT3H_ERR_RETURN_TYPE_OK  ESP_OK
    #define NT3H_ERR_RETURN_TYPE_FAIL  ESP_FAIL
    #define NT3H_return_esp_err_t   // to let the code below know that the return type is an esp_err_t
  #elif defined(ARDUINO_ARCH_STM32)
    #define NT3H_ERR_RETURN_TYPE  i2c_status_e
    #define NT3H_ERR_RETURN_TYPE_OK  I2C_OK
    #define NT3H_ERR_RETURN_TYPE_FAIL  I2C_ERROR // NOTE: this may cause some debugging confusion (but then again, STM32 I2C is already hard to debug. Just try using WireLib)
    #define NT3H_return_i2c_status_e // to let the code below know that the return type is an esp_err_t
  #else
    #error("failed to define ERR_RETURN_TYPE for some reason")
  #endif
#endif
#ifndef NT3H_ERR_RETURN_TYPE_OK
  #warning("NT3H_ERR_RETURN_TYPE was manually defined, but NT3H_ERR_RETURN_TYPE_OK was not!, attempting to infer automatically...")
  #ifdef NT3H_return_esp_err_t
    #define NT3H_ERR_RETURN_TYPE_OK  ESP_OK
  #elif defined(NT3H_return_i2c_status_e)
    #define NT3H_ERR_RETURN_TYPE_OK  I2C_OK
  #else
    #error("when manually defining the NT3H_ERR_RETURN_TYPE, please also define the NT3H_ERR_RETURN_TYPE_OK. See _NT3H_thijs_base.h")
  #endif
#endif
#ifndef NT3H_ERR_RETURN_TYPE_FAIL
  #warning("NT3H_ERR_RETURN_TYPE was manually defined, but NT3H_ERR_RETURN_TYPE_FAIL was not!, attempting to infer automatically...")
  #ifdef NT3H_return_esp_err_t
    #define NT3H_ERR_RETURN_TYPE_FAIL  ESP_FAIL
  #elif defined(NT3H_return_i2c_status_e)
    #define NT3H_ERR_RETURN_TYPE_FAIL  I2C_ERROR // NOTE: this may cause some debugging confusion (but then again, STM32 I2C is already hard to debug. Just try using WireLib)
  #else
    #error("when manually defining the NT3H_ERR_RETURN_TYPE, please also define the NT3H_ERR_RETURN_TYPE_FAIL. See _NT3H_thijs_base.h")
  #endif
#endif

//// some I2C constants
#define TW_WRITE 0 //https://en.wikipedia.org/wiki/I%C2%B2C  under "Addressing structure"
#define TW_READ  1

#define ACK_CHECK_EN 1  //only for ESP32
#define ACK_CHECK_DIS 0 //ack check disable does not seem to work??? (it always checks for an ack and spits out )

#define SIZEOF_I2C_CMD_DESC_T  20  //a terrible fix for a silly problem. The actual struct/typedef code is too far down the ESP32 dependancy rabbithole.
#define SIZEOF_I2C_CMD_LINK_T  20  //if the ESP32 didn't have such a fragmented filestructure this wouldn't be a problem, maybe
/* the reason why i need those 2 defines:   //this code was derived from https://github.com/espressif/esp-idf/blob/master/components/driver/i2c.c
uint8_t buffer[sizeof(i2c_cmd_desc_t) + sizeof(i2c_cmd_link_t) * numberOfOperations] = { 0 };
i2c_cmd_handle_t handle = i2c_cmd_link_create_static(buffer, sizeof(buffer));
*/


/**
 * (this is only the base class, users should use NT3H_thijs)
 * 
 */
class _NT3H_thijs_base
{
  public:
  //// I2C constants:
  uint8_t slaveAddress; // 7-bit address
  const bool isNewVersion; // NT3H1x01 or NT3H2x11
  const bool is2kVariant; // NT3Hx1x1 or NT3Hx2x1
  
  _NT3H_thijs_base(bool isNewVersion, bool is2kVariant, uint8_t address=NT3H_DEFAULT_I2C_ADDRESS) : isNewVersion(isNewVersion), is2kVariant(is2kVariant), slaveAddress(address) {}
  
  #ifdef NT3H_useWireLib // higher level generalized (arduino wire library):

    public:

    /**
     * initialize I2C peripheral through the Wire.h library
     * @param frequency SCL clock freq in Hz
     */
    void init(uint32_t frequency) {
      Wire.begin(); // init I2C as master
      Wire.setClock(frequency); // set the (approximate) desired clock frequency. Note, may be affected by pullup resistor strength (on some microcontrollers)
    }
    
    /**
     * request a block of memory (NOTE: Session register data must be requested using requestSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param readBuff a NT3H_BLOCK_SIZE buffer to store the read values in
     * @return whether it wrote/read successfully
     */
    bool requestMemBlock(uint8_t blockAddress, uint8_t readBuff[]) {
      // ideally, i'd use the Wire function: requestFrom(address, quantity, iaddress, isize, sendStop), which lets you send the register through iaddress
      // HOWEVER, this function is not implemented on all platforms (looking at you, MSP430!), and it's not that hard to do manually anyway, so:
      Wire.beginTransmission(slaveAddress);
      Wire.write(blockAddress);
      Wire.endTransmission(); // NOTE: should return 0 if all went well (currently not implemented)
      return(_onlyReadBytes(readBuff, NT3H_BLOCK_SIZE));
    }

    /**
     * request a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param readBuff a uint8_t pointer to store the read value in
     * @return whether it wrote/read successfully
     */
    bool requestSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t& readBuff) {
      // ideally, i'd use the Wire function: requestFrom(address, quantity, iaddress, isize, sendStop), which lets you send the register through iaddress
      // HOWEVER, this function is not implemented on all platforms (looking at you, MSP430!), and it's not that hard to do manually anyway, so:
      Wire.beginTransmission(slaveAddress);
      Wire.write(NT3H_SESS_REGS_MEMA);
      Wire.write(registerIndex);
      Wire.endTransmission(); // NOTE: should return 0 if all went well (currently not used)
      return(_onlyReadBytes(&readBuff, 1));
    }
  
    /**
     * (private) read bytes into a buffer (without first writing)
     * @param readBuff a buffer to store the read values in
     * @param bytesToRead how many bytes to read
     * @return whether it read successfully
     */
    bool _onlyReadBytes(uint8_t readBuff[], uint8_t bytesToRead) {
      Wire.requestFrom(slaveAddress, bytesToRead); // NOTE: should return number of bytes read (currently not used)
      if(Wire.available() != bytesToRead) { NT3HdebugPrint("onlyReadBytes() received insufficient data"); return(false); }
      for(uint8_t i=0; i<bytesToRead; i++) { readBuff[i] = Wire.read(); } // dumb byte-by-byte copy
      // unfortunately, TwoWire.rxBuffer is a private member, so we cant just memcpy. Then again, this implementation is not meant to be efficient
      return(true);
    }
    
    /**
     * write a block worth of bytes from a buffer to a memory address (note: Session register data must be written using writeSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param writeBuff a buffer of bytes to write to the device
     * @param bytesToWrite how many bytes of actual data to write, the remainder (to complete the NT3H_BLOCK_SIZE block) will be 0's
     * @return whether it wrote successfully
     */
    bool writeMemBlock(uint8_t blockAddress, uint8_t writeBuff[], uint8_t bytesToWrite=NT3H_BLOCK_SIZE) {
      if(bytesToWrite > NT3H_BLOCK_SIZE) {/* PANIC */  NT3HdebugPrint("writeMemBlock() can only write in blocks of 16 bytes, not more!"); return(false); }
      Wire.beginTransmission(slaveAddress);
      Wire.write(blockAddress);
      Wire.write(writeBuff, bytesToWrite); // (usually) just calls a forloop that calls .write(byte) for every byte.
      for(uint8_t i=0; i<(NT3H_BLOCK_SIZE-bytesToWrite); i++) { Wire.write(0); } // pad 0's to make the block complete
      Wire.endTransmission();
      return(true);
    }

    /**
     * update a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param regDat the byte to write to the register
     * @param mask the bits of the register that regDat should affect
     * @return whether it wrote successfully
     */
    bool writeSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t regDat, uint8_t mask=0xFF) {
      Wire.beginTransmission(slaveAddress);
      Wire.write(NT3H_SESS_REGS_MEMA);
      Wire.write(registerIndex);
      Wire.write(mask);
      Wire.write(regDat);
      Wire.endTransmission();
      return(true);
    }

    // /**
    //  * send a repeated start condition.
    //  * IF I2C_RST_ON_OFF is enabled in the session registers, this will soft-reset the IC
    //  * ELSE it will probably do nothing, but it may annoy the memory arbitration untill the WDT resets it, idk.
    //  */
    // void softReset() {
    //   //TODO: find way to make Wire library send repeated start
    //   NT3HdebugPrint("softReset() doesn't work with WireLib, as it is not meant for sending repeated starts.");
    // }

  #elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__) // TODO: test 328p processor defines! (also, this code may be functional on other AVR hw as well?)
    private:
    //// I2C constants:
    static const uint8_t twi_basic = (1<<TWINT) | (1<<TWEN); //any action will feature these 2 things (note TWEA is 0)
    static const uint8_t twi_START = twi_basic | (1<<TWSTA);
    static const uint8_t twi_STOP  = twi_basic | (1<<TWSTO);
    static const uint8_t twi_basic_ACK = twi_basic | (1<<TWEA); //(for master receiver mode) basic action, repond with ACK (if appropriate)
    
    static const uint8_t twi_SR_noPres = 0b11111000; //TWSR (stas register) without prescalebits
    // status register contents (master mode)
    static const uint8_t twi_SR_M_START = 0x08;      //start condition has been transmitted
    static const uint8_t twi_SR_M_RESTART = 0x10;    //repeated start condition has been transmitted
    static const uint8_t twi_SR_M_SLA_W_ACK = 0x18;  //SLA+W has been transmitted, ACK received
    static const uint8_t twi_SR_M_SLA_W_NACK = 0x20; //SLA+W has been transmitted, NOT ACK received
    static const uint8_t twi_SR_M_DAT_T_ACK = 0x28;  //data has been transmitted, ACK received
    static const uint8_t twi_SR_M_DAT_T_NACK = 0x30; //data has been transmitted, NOT ACK received
    static const uint8_t twi_SR_M_arbit = 0x38;      //arbitration
    static const uint8_t twi_SR_M_SLA_R_ACK = 0x40;  //SLA+R has been transmitted, ACK received
    static const uint8_t twi_SR_M_SLA_R_NACK = 0x48; //SLA+R has been transmitted, NOT ACK received
    static const uint8_t twi_SR_M_DAT_R_ACK = 0x50;  //data has been received, ACK returned
    static const uint8_t twi_SR_M_DAT_R_NACK = 0x58; //data has been received, NOT ACK returned
    // status register contents (slave mode)
    static const uint8_t twi_SR_S_SLA_W_ACK = 0x60;  //own address + W has been received, ACK returned
    static const uint8_t twi_SR_S_arbit_SLA_W = 0x68;//arbitration
    static const uint8_t twi_SR_S_GEN_ACK = 0x70;    //general call + W has been received, ACK returned
    static const uint8_t twi_SR_S_arbit_GEN = 0x78;  //arbitration
    static const uint8_t twi_SR_S_DAT_SR_ACK = 0x80; //data has been received after SLA+W, ACK returned
    static const uint8_t twi_SR_S_DAT_SR_NACK = 0x88;//data has been received after SLA+W, NOT ACK returned
    static const uint8_t twi_SR_S_DAT_GR_ACK = 0x90; //data has been received after GEN+W, ACK returned
    static const uint8_t twi_SR_S_DAT_GR_NACK = 0x98;//data has been received after GEN+W, NOT ACK returned
    static const uint8_t twi_SR_S_prem_STOP_RE =0xA0;//a STOP or repeated_START condition has been received prematurely (page 193)
    static const uint8_t twi_SR_S_SLA_R_ACK = 0xA8;  //own address + R has been received, ACK returned
    static const uint8_t twi_SR_S_arbit_SLA_R = 0xB0;//arbitration
    static const uint8_t twi_SR_S_DAT_ST_ACK = 0xB8; //data has been transmitted, ACK received     (master receiver wants more data)
    static const uint8_t twi_SR_S_DAT_ST_NACK = 0xC0;//data has been transmitted, NOT ACK received (master receiver doesnt want any more)
    static const uint8_t twi_SR_S_DAT_STL_ACK = 0xC8;//last (TWEA==0) data has been transmitted, ACK received (data length misconception)
    // status register contents (miscellaneous states)
    static const uint8_t twi_SR_nothing = twi_SR_noPres; //(0xF8) no relevant state info, TWINT=0
    static const uint8_t twi_SR_bus_err = 0; //bus error due to an illigal start/stop condition (if this happens, set TWCR to STOP condition)

    /*  what the ACK bit does (and what the status registers read if ACK is used wrong/unexpectedly):
    after a START, in response to an address byte, the slave uses ACK if (TWEA=1) it accepts the communication in general
    during data transferrence (either direction) the ACK/NOT-ACK is used to let the other side know whether or not they want more data
    if the recipient sends an ACK, it expects more data
    if the master transmitter ran out of data to send to the slave receiver, the slave status register will read 0xA0 (twi_SR_S_STOP_RESTART)
    if the slave transmitter ran out of data to send to the master receiver, the slave status register will read 0xC8 (twi_SR_S_DAT_STL_ACK)
        in that case, the slave transmitter will send all 1's untill STOP (or RESTART)
    in cases where there is too much data (from either side), the NOT-ACK will just be received earlier than expected
        if the slave sends NOT-ACK early, the master should STOP/RESTART the transmission (or the slave should ignore the overflowing data)
        if the master sends NOT-ACK early, the slave doesnt have to do anything (except maybe raise an error internally)
    
    in general, the TWEA (Enable Ack) bit should be synchronized in both devices (except for master transmitter, which doesnt use it).
    in master receiver, TWEA signals to the slave that the last byte is received, and the transmission will end
    in both slave modes, if TWEA==0, the slave expects for there to be a STOP/RESTART next 'tick', if not, the status register will read 0 (twi_SR_bus_err)
    */
    
    inline void twoWireTransferWait() { while(!(TWCR & (1<<TWINT))); }
    #define twoWireStatusReg      (TWSR & twi_SR_noPres)

    inline void twiWrite(uint8_t byteToWrite) {
      TWDR = byteToWrite;
      TWCR = twi_basic; //initiate transfer
      twoWireTransferWait();
    }
    
    inline bool startWrite() {
      TWCR = twi_START; //send start
      twoWireTransferWait();
      twiWrite((slaveAddress<<1) | TW_WRITE);
      if(twoWireStatusReg != twi_SR_M_SLA_W_ACK) { NT3HdebugPrint("SLA_W ack error"); TWCR = twi_STOP; return(false); }
      return(true);
    }

    inline bool startRead() {
      TWCR = twi_START; //repeated start
      twoWireTransferWait();
      twiWrite((slaveAddress<<1) | TW_READ);
      if(twoWireStatusReg != twi_SR_M_SLA_R_ACK) { NT3HdebugPrint("SLA_R ack error"); TWCR = twi_STOP; return(false); }
      return(true);
    }

    public:

    /**
     * initialize I2C peripheral
     * @param frequency SCL clock freq in Hz
     * @return frequency it was able to set
     */
    uint32_t init(uint32_t frequency) {
      // set frequency (SCL freq = F_CPU / (16 + 2*TWBR*prescaler) , where prescaler is 1,8,16 or 64x, see page 200)
      TWSR &= 0b11111000; //set prescaler to 1x
      //TWBR  = 12; //set clock reducer to 400kHz (i recommend external pullups at this point)
      #define prescaler 1
      TWBR = ((F_CPU / frequency) - 16) / (2*prescaler);
      uint32_t reconstFreq = F_CPU / (16 + (2*TWBR*prescaler));
      //Serial.print("freq: "); Serial.print(frequency); Serial.print(" TWBR:"); Serial.print(TWBR); Serial.print(" freq: "); Serial.println(reconstFreq);
      // the fastest i could get I2C to work is 800kHz (with another arduino as slave at least), which is TWBR=2 (with some 1K pullups)
      // any faster and i get SLA_ACK errors.
      return(reconstFreq);
    }
    
    /**
     * request a block of memory (NOTE: Session register data must be requested using requestSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param readBuff a NT3H_BLOCK_SIZE buffer to store the read values in
     * @return whether it wrote/read successfully
     */
    bool requestMemBlock(uint8_t blockAddress, uint8_t readBuff[]) {
      if(!startWrite()) { return(false); }
      twiWrite(blockAddress);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      TWCR = twi_STOP; // pretty sure this is preferred, in case I2C_RST_ON_OFF is enabled
      return(_onlyReadBytes(readBuff, NT3H_BLOCK_SIZE));
    }

    /**
     * request a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param readBuff a uint8_t pointer to store the read value in
     * @return whether it wrote/read successfully
     */
    bool requestSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t& readBuff) {
      if(!startWrite()) { return(false); }
      twiWrite(NT3H_SESS_REGS_MEMA);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      twiWrite(registerIndex);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      TWCR = twi_STOP; // pretty sure this is preferred, in case I2C_RST_ON_OFF is enabled
      return(_onlyReadBytes(&readBuff, 1));
    }
  
    /**
     * (private) read bytes into a buffer (without first writing)
     * @param readBuff a buffer to store the read values in
     * @param bytesToRead how many bytes to read
     * @return whether it read successfully
     */
    bool _onlyReadBytes(uint8_t readBuff[], uint8_t bytesToRead) {
      if(!startRead()) { return(false); }
      for(uint8_t i=0; i<(bytesToRead-1); i++) {
        TWCR = twi_basic_ACK; //request several bytes
        twoWireTransferWait();
        //if(twoWireStatusReg != twi_SR_M_DAT_R_ACK) { NT3HdebugPrint("DAT_R Ack error"); return(false); }
        readBuff[i] = TWDR;
      }
      TWCR = twi_basic; //request 1 more byte
      twoWireTransferWait();
      //if(twoWireStatusReg != twi_SR_M_DAT_R_NACK) { NT3HdebugPrint("DAT_R Nack error"); return(false); }
      readBuff[bytesToRead-1] = TWDR;
      TWCR = twi_STOP;
      return(true);
    }
    
    /**
     * write a block worth of bytes from a buffer to a memory address (note: Session register data must be written using writeSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param writeBuff a buffer of bytes to write to the device
     * @param bytesToWrite how many bytes of actual data to write, the remainder (to complete the NT3H_BLOCK_SIZE block) will be 0's
     * @return whether it wrote successfully
     */
    bool writeMemBlock(uint8_t blockAddress, uint8_t writeBuff[], uint8_t bytesToWrite=NT3H_BLOCK_SIZE) {
      if(bytesToWrite > NT3H_BLOCK_SIZE) {/* PANIC */  NT3HdebugPrint("writeMemBlock() can only write in blocks of 16 bytes, not more!"); return(false); }
      if(!startWrite()) { return(false); }
      twiWrite(registerToWrite);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      for(uint8_t i=0; i<NT3H_BLOCK_SIZE; i++) {
        twiWrite((i<bytesToWrite) ? writeBuff[i] : 0); // write real data if it exists, pad 0's where needed
        //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      }
      TWCR = twi_STOP;
      return(true);
    }

    /**
     * update a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param regDat the byte to write to the register
     * @param mask the bits of the register that regDat should affect
     * @return whether it wrote successfully
     */
    bool writeSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t regDat, uint8_t mask=0xFF) {
      if(!startWrite()) { return(false); }
      twiWrite(NT3H_SESS_REGS_MEMA);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      twiWrite(registerIndex);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      twiWrite(mask);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      twiWrite(regDat);  //if(twoWireStatusReg != twi_SR_M_DAT_T_ACK) { return(false); } //should be ACK(?)
      TWCR = twi_STOP;
      return(true);
    }

    // /**
    //  * send a repeated start condition.
    //  * IF I2C_RST_ON_OFF is enabled in the session registers, this will soft-reset the IC
    //  * ELSE it will probably do nothing, but it may annoy the memory arbitration untill the WDT resets it, idk.
    //  */
    // void softReset() {
    //   //TODO: make function:  startRead();startRead();   startWrite();startWrite();
    // }
  
  #elif defined(ARDUINO_ARCH_ESP32)
    // see my NT3H library for notes on the ESP32's mediocre I2C peripheral
    
    public:
    //// I2C constants:
    i2c_port_t I2Cport = 0;
    uint32_t I2Ctimeout = 10; //in millis
    //const TickType_t I2CtimeoutTicks = 100 / portTICK_RATE_MS; //timeout (divide by portTICK_RATE_MS to convert millis to the right format)
    //uint8_t constWriteBuff[1]; //i2c_master_write_read_device() requires a const uint8_t* writeBuffer. You can make this array bigger if you want, shouldnt really matter
    
    /**
     * initialize I2C peripheral
     * @param frequency SCL clock freq in Hz
     * @param SDApin GPIO pin to use as SDA
     * @param SCLpin GPIO pin to use as SCL
     * @param I2CportToUse which of the ESP32's I2C peripherals to use
     * @return (esp_err_t) whether it was able to establish the peripheral
     */
    esp_err_t init(uint32_t frequency, int SDApin=21, int SCLpin=22, i2c_port_t I2CportToUse = 0) {
      if(I2CportToUse < I2C_NUM_MAX) { I2Cport = I2CportToUse; } else { NT3HdebugPrint("can't init(), invalid I2Cport!"); return(ESP_ERR_INVALID_ARG); }
      i2c_config_t conf;
      conf.mode = I2C_MODE_MASTER;
      conf.sda_io_num = SDApin;
      conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
      conf.scl_io_num = SCLpin;
      conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
      conf.master.clk_speed = frequency;
      //conf.clk_flags = 0;          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
      esp_err_t err = i2c_param_config(I2Cport, &conf);
      if (err != ESP_OK) { NT3HdebugPrint("can't init(), i2c_param_config error!"); NT3HdebugPrint(esp_err_to_name(err)); return(err); }
      return(i2c_driver_install(I2Cport, conf.mode, 0, 0, 0));
    }
    
    /**
     * request a block of memory (NOTE: Session register data must be requested using requestSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param readBuff a NT3H_BLOCK_SIZE buffer to store the read values in
     * @return (esp_err_t or bool) whether it wrote/read successfully
     */
    NT3H_ERR_RETURN_TYPE requestMemBlock(uint8_t blockAddress, uint8_t readBuff[]) {
      // for alternate commands (manually selected I2C operations), please refer to AS5600_thijs or TMP112_thijs
      esp_err_t err = i2c_master_write_read_device(I2Cport, slaveAddress, &blockAddress, 1, readBuff, NT3H_BLOCK_SIZE, I2Ctimeout / portTICK_RATE_MS); //faster (seems to work fine)
      if(err != ESP_OK) { NT3HdebugPrint(esp_err_to_name(err)); }
      #ifdef NT3H_return_esp_err_t
        return(err);
      #else
        return(err == ESP_OK);
      #endif
    }

    /**
     * request a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param readBuff a uint8_t pointer to store the read value in
     * @return (esp_err_t or bool) whether it wrote/read successfully
     */
    NT3H_ERR_RETURN_TYPE requestSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t& readBuff) {
      // for alternate commands (manually selected I2C operations), please refer to AS5600_thijs or TMP112_thijs
      uint8_t requestArr[2] = {NT3H_SESS_REGS_MEMA, registerIndex};
      esp_err_t err = i2c_master_write_read_device(I2Cport, slaveAddress, requestArr, 1, &readBuff, 1, I2Ctimeout / portTICK_RATE_MS); //faster (seems to work fine)
      if(err != ESP_OK) { NT3HdebugPrint(esp_err_to_name(err)); }
      #ifdef NT3H_return_esp_err_t
        return(err);
      #else
        return(err == ESP_OK);
      #endif
    }
  
    /**
     * (private) read bytes into a buffer (without first writing)
     * @param readBuff a buffer to store the read values in
     * @param bytesToRead how many bytes to read
     * @return (esp_err_t or bool) whether it read successfully
     */
    NT3H_ERR_RETURN_TYPE _onlyReadBytes(uint8_t readBuff[], uint8_t bytesToRead) {
      // for alternate commands (manually selected I2C operations), please refer to AS5600_thijs or TMP112_thijs
      esp_err_t err = i2c_master_read_from_device(I2Cport, slaveAddress, readBuff, bytesToRead, I2Ctimeout / portTICK_RATE_MS);  //faster?
      if(err != ESP_OK) { NT3HdebugPrint(esp_err_to_name(err)); }
      #ifdef NT3H_return_esp_err_t
        return(err);
      #else
        return(err == ESP_OK);
      #endif
    }
    
    /**
     * write a block worth of bytes from a buffer to a memory address (note: Session register data must be written using writeSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param writeBuff a buffer of bytes to write to the device
     * @param bytesToWrite how many bytes of actual data to write, the remainder (to complete the NT3H_BLOCK_SIZE block) will be 0's
     * @return (esp_err_t or bool) whether it wrote successfully
     */
    NT3H_ERR_RETURN_TYPE writeMemBlock(uint8_t blockAddress, uint8_t writeBuff[], uint8_t bytesToWrite=NT3H_BLOCK_SIZE) {
      if(bytesToWrite > NT3H_BLOCK_SIZE) {/* PANIC */  NT3HdebugPrint("writeMemBlock() can only write in blocks of 16 bytes, not more!"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
      esp_err_t err;
      // if(bytesToWrite == NT3H_BLOCK_SIZE) {
      //   const uint8_t numberOfCommands = 5; //start, write, write, write, stop
      //   uint8_t CMDbuffer[SIZEOF_I2C_CMD_DESC_T + SIZEOF_I2C_CMD_LINK_T * numberOfCommands] = { 0 };
      //   i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(CMDbuffer, sizeof(CMDbuffer)); //create a CMD sequence
      //   i2c_master_start(cmd);
      //   i2c_master_write_byte(cmd, (slaveAddress<<1) | TW_WRITE, ACK_CHECK_EN);
      //   i2c_master_write_byte(cmd, blockAddress, ACK_CHECK_DIS);
      //   i2c_master_write(cmd, writeBuff, bytesToWrite, ACK_CHECK_DIS);
      //   i2c_master_stop(cmd);
      //   err = i2c_master_cmd_begin(I2Cport, cmd, I2Ctimeout / portTICK_RATE_MS);
      //   i2c_cmd_link_delete_static(cmd);
      // } else { // probably slightly slower:
        uint8_t copiedArray[NT3H_BLOCK_SIZE+1]; copiedArray[0]=blockAddress; for(uint8_t i=0;i<NT3H_BLOCK_SIZE;i++) { copiedArray[i+1]=(i<bytesToWrite) ? writeBuff[i] : 0; }
        err = i2c_master_write_to_device(I2Cport, slaveAddress, copiedArray, NT3H_BLOCK_SIZE+1, I2Ctimeout / portTICK_RATE_MS);
      // }
      if(err != ESP_OK) { NT3HdebugPrint(esp_err_to_name(err)); }
      #ifdef NT3H_return_esp_err_t
        return(err);
      #else
        return(err == ESP_OK);
      #endif
    }

    /**
     * update a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param regDat the byte to write to the register
     * @param mask the bits of the register that regDat should affect
     * @return (esp_err_t or bool) whether it wrote successfully
     */
    NT3H_ERR_RETURN_TYPE writeSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t regDat, uint8_t mask=0xFF) {
      // const uint8_t numberOfCommands = 7; //start, write, write, write, write, write, stop
      // uint8_t CMDbuffer[SIZEOF_I2C_CMD_DESC_T + SIZEOF_I2C_CMD_LINK_T * numberOfCommands] = { 0 };
      // i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(CMDbuffer, sizeof(CMDbuffer)); //create a CMD sequence
      // i2c_master_start(cmd);
      // i2c_master_write_byte(cmd, (slaveAddress<<1) | TW_WRITE, ACK_CHECK_EN);
      // i2c_master_write_byte(cmd, NT3H_SESS_REGS_MEMA, ACK_CHECK_DIS); // TODO: ACK checks...
      // i2c_master_write_byte(cmd, registerIndex, ACK_CHECK_DIS);
      // i2c_master_write_byte(cmd, mask, ACK_CHECK_DIS);
      // i2c_master_write_byte(cmd, regDat, ACK_CHECK_DIS);
      // i2c_master_stop(cmd);
      // esp_err_t err = i2c_master_cmd_begin(I2Cport, cmd, I2Ctimeout / portTICK_RATE_MS);
      // i2c_cmd_link_delete_static(cmd);
      //// hopefully faster:
      uint8_t regWriteArr[4] = {NT3H_SESS_REGS_MEMA, registerIndex, mask, regDat};
      esp_err_t err = i2c_master_write_to_device(I2Cport, slaveAddress, regWriteArr, 4, I2Ctimeout / portTICK_RATE_MS);
      if(err != ESP_OK) { NT3HdebugPrint(esp_err_to_name(err)); }
      #ifdef NT3H_return_esp_err_t
        return(err);
      #else
        return(err == ESP_OK);
      #endif
    }

    // /**
    //  * send a repeated start condition.
    //  * IF I2C_RST_ON_OFF is enabled in the session registers, this will soft-reset the IC
    //  * ELSE it will probably do nothing, but it may annoy the memory arbitration untill the WDT resets it, idk.
    //  */
    // void softReset() {
    //   //TODO: make function
    // }

  #elif defined(__MSP430FR2355__) //TBD: determine other MSP430 compatibility: || defined(ENERGIA_ARCH_MSP430) || defined(__MSP430__)

    public:

    /**
     * initialize I2C peripheral
     * @param frequency SCL clock freq in Hz
     */
    void init(uint32_t frequency) {
      //twi_setModule(module); // the MSP430 implementation of I2C is slightly janky. Instead of different classes for different I2C interfaces, they have a global variable indicating which module is targeted
      // the default module is all i'm going to need for my uses, but if you wanted to use multiple I2C peripherals, please uncomment all the twi_setModule() things and add a module constant to each NT3H_thijs obj
      twi_init();
      twi_setClock(frequency);
    }
    
    /**
     * request a block of memory (NOTE: Session register data must be requested using requestSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param readBuff a NT3H_BLOCK_SIZE buffer to store the read values in
     * @return whether it wrote/read successfully
     */
    bool requestMemBlock(uint8_t blockAddress, uint8_t readBuff[]) {
      //twi_setModule(module);  // see init() for explenation
      int8_t ret = twi_writeTo(slaveAddress, &blockAddress, 1, 1, true); // transmit 1 byte, wait for the transmission to complete and send a STOP command
      if(ret != 0) { NT3HdebugPrint("requestSessRegByte() twi_writeTo error!"); return(false); }
      return(_onlyReadBytes(readBuff, NT3H_BLOCK_SIZE));
    }

    /**
     * request a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param readBuff a uint8_t pointer to store the read value in
     * @return whether it wrote/read successfully
     */
    bool requestSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t& readBuff) {
      //twi_setModule(module);  // see init() for explenation
      uint8_t requestArr[2] = {NT3H_SESS_REGS_MEMA, registerIndex};
      int8_t ret = twi_writeTo(slaveAddress, requestArr, 2, 1, true); // transmit 1 byte, wait for the transmission to complete and send a STOP command
      if(ret != 0) { NT3HdebugPrint("requestSessRegByte() twi_writeTo error!"); return(false); }
      return(_onlyReadBytes(&readBuff, 1));
    }
  
    /**
     * (private) read bytes into a buffer (without first writing)
     * @param readBuff a buffer to store the read values in
     * @param bytesToRead how many bytes to read
     * @return whether it read successfully
     */
    bool _onlyReadBytes(uint8_t readBuff[], uint8_t bytesToRead) {
      //twi_setModule(module);  // see init() for explenation
      uint8_t readQuantity = twi_readFrom(slaveAddress, readBuff, bytesToRead, true); // note: sendstop=true
      if(readQuantity != bytesToRead) { NT3HdebugPrint("_onlyReadBytes() received insufficient data"); return(false); }
      return(true);
    }
    
    /**
     * write a block worth of bytes from a buffer to a memory address (note: Session register data must be written using writeSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param writeBuff a buffer of bytes to write to the device
     * @param bytesToWrite how many bytes of actual data to write, the remainder (to complete the NT3H_BLOCK_SIZE block) will be 0's
     * @return whether it wrote successfully
     */
    bool writeMemBlock(uint8_t blockAddress, uint8_t writeBuff[], uint8_t bytesToWrite=NT3H_BLOCK_SIZE) {
      //twi_setModule(module);  // see init() for explenation
      if(bytesToWrite > NT3H_BLOCK_SIZE) {/* PANIC */  NT3HdebugPrint("writeMemBlock() can only write in blocks of 16 bytes, not more!"); return(false); }
      uint8_t copiedArray[NT3H_BLOCK_SIZE+1]; copiedArray[0]=blockAddress; for(uint8_t i=0;i<NT3H_BLOCK_SIZE;i++) { copiedArray[i+1]=(i<bytesToWrite) ? writeBuff[i] : 0; }
      int8_t ret = twi_writeTo(slaveAddress, copiedArray, NT3H_BLOCK_SIZE+1, 1, true); // transmit some bytes, wait for the transmission to complete and send a STOP command
      if(ret != 0) { NT3HdebugPrint("writeMemBlock() twi_writeTo error!"); return(false); }
      return(true);
      // note: for my opinions (complaints) about the MSP430 twi library, please see writeBytes() in AS5600_thijs or TMP112_thijs
    }

    /**
     * update a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param regDat the byte to write to the register
     * @param mask the bits of the register that regDat should affect
     * @return whether it wrote successfully
     */
    bool writeSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t regDat, uint8_t mask=0xFF) {
      //twi_setModule(module);  // see init() for explenation
      uint8_t regWriteArr[4] = {NT3H_SESS_REGS_MEMA, registerIndex, mask, regDat};
      int8_t ret = twi_writeTo(slaveAddress, regWriteArr, 4, 1, true); // transmit some bytes, wait for the transmission to complete and send a STOP command
      if(ret != 0) { NT3HdebugPrint("writeSessRegByte() twi_writeTo error!"); return(false); }
      return(true);
    }

    // /**
    //  * send a repeated start condition.
    //  * IF I2C_RST_ON_OFF is enabled in the session registers, this will soft-reset the IC
    //  * ELSE it will probably do nothing, but it may annoy the memory arbitration untill the WDT resets it, idk.
    //  */
    // void softReset() {
    //   //TODO: find way to make MSP430 twi library send a repeated start
    // }

  #elif defined(ARDUINO_ARCH_STM32)
  
    /* Notes on the STM32 I2C perihperal (specifically that of the STM32WB55):
    Much like the ESP32, the STM32 libraries are built on several layers of abstraction.
    The twi.h library goes to some HAL library, and i have no intention of finding out where it goes from there.
    Since this particular implementation does not need to be terribly fast, i'll just stick with twi.h,
     which does have the advantage of working with the whole STM32 family (whereas a lower implementation would target specific subfamilies)
    The STM32 can map the I2C pins to a limited (but at least more than one) selection of pins,
     see PeripheralPins.c for the PinMap_I2C_SDA and PinMap_I2C_SCL (or just look at the datasheet)
     (for my purposes, that's: .platformio\packages\framework-arduinoststm32\variants\STM32WBxx\WB55R(C-E-G)V\PeripheralPins.c )
     Here is a handy little table:
      I2C1: SDA: PA10, PB7, PB9
            SCL: PA9, PB6, PB8
      I2C3: SDA: PB4, PB11, PB14, PC1
            SCL: PA7, PB10, PB13, PC0
    
    */

    public:

    i2c_t _i2c; // handler thingy (presumably)
    static const uint8_t STM32_MASTER_ADDRESS = 0x01; // a reserved address which tells the peripheral it's a master, not a slave
    
    /**
     * initialize I2C peripheral on STM32
     * @param frequency SCL clock freq in Hz
     * @param SDApin pin (arduino naming) to use as SDA (select few possible)
     * @param SCLpin pin (arduino naming) to use as SCL (select few possible)
     * @param generalCall i'm honestly not sure, the STM32 twi library is not documented very well...
     */
    void init(uint32_t frequency, uint32_t SDApin=PIN_WIRE_SDA, uint32_t SCLpin=PIN_WIRE_SCL, bool generalCall = false) {
      _i2c.sda = digitalPinToPinName(SDApin);
      _i2c.scl = digitalPinToPinName(SCLpin);
      _i2c.__this = (void *)this; // i truly do not understand the stucture of the STM32 i2c_t, but whatever, i guess the i2c_t class needs to know where this higher level class is or something
      _i2c.isMaster = true;
      _i2c.generalCall = (generalCall == true) ? 1 : 0; // 'generalCall' is just a uint8_t instead of a bool
      i2c_custom_init(&_i2c, frequency, I2C_ADDRESSINGMODE_7BIT, (STM32_MASTER_ADDRESS << 1)); // this selects which I2C peripheral is used based on what pins you entered
      // note: use i2c_setTiming(&_i2c, frequency) if you want to change the frequency later
    }
    
    /**
     * request a block of memory (NOTE: Session register data must be requested using requestSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param readBuff a NT3H_BLOCK_SIZE buffer to store the read values in
     * @return (i2c_status_e or bool) whether it wrote/read successfully
     */
    NT3H_ERR_RETURN_TYPE requestMemBlock(uint8_t blockAddress, uint8_t readBuff[]) {
      #if defined(I2C_OTHER_FRAME) // not on all STM32 variants
        _i2c.handle.XferOptions = I2C_OTHER_AND_LAST_FRAME; // (this one i don't understand, but the Wire.h library does it, and without it i get HAL_I2C_ERROR_SIZE~~64 (-> I2C_ERROR~~4))
      #endif
      i2c_status_e err = i2c_master_write(&_i2c, (slaveAddress << 1), &blockAddress, 1);
      if(err != I2C_OK) {
        NT3HdebugPrint("requestReadBytes() i2c_master_write error!");
        #ifdef NT3H_return_i2c_status_e
          return(err);
        #else
          return(false);
        #endif
      }
      return(_onlyReadBytes(readBuff, NT3H_BLOCK_SIZE));
    }

    /**
     * request a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param readBuff a uint8_t pointer to store the read value in
     * @return (i2c_status_e or bool) whether it wrote/read successfully
     */
    NT3H_ERR_RETURN_TYPE requestSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t& readBuff) {
      #if defined(I2C_OTHER_FRAME) // not on all STM32 variants
        _i2c.handle.XferOptions = I2C_OTHER_AND_LAST_FRAME; // (this one i don't understand, but the Wire.h library does it, and without it i get HAL_I2C_ERROR_SIZE~~64 (-> I2C_ERROR~~4))
      #endif
      uint8_t requestArr[2] = {NT3H_SESS_REGS_MEMA, registerIndex};
      i2c_status_e err = i2c_master_write(&_i2c, (slaveAddress << 1), requestArr, 2);
      if(err != I2C_OK) {
        NT3HdebugPrint("requestReadBytes() i2c_master_write error!");
        #ifdef NT3H_return_i2c_status_e
          return(err);
        #else
          return(false);
        #endif
      }
      return(_onlyReadBytes(&readBuff, 1));
    }
  
    /**
     * read bytes into a buffer (without first writing a register value!)
     * @param readBuff a buffer to store the read values in
     * @param bytesToRead how many bytes to read
     * @return (i2c_status_e or bool) whether it read successfully
     */
    NT3H_ERR_RETURN_TYPE _onlyReadBytes(uint8_t readBuff[], uint8_t bytesToRead) {
      #if defined(I2C_OTHER_FRAME) // if the STM32 subfamily is capable of writing without sending a stop
        _i2c.handle.XferOptions = I2C_OTHER_AND_LAST_FRAME; // tell the peripheral it should send a STOP at the end
      #endif
      i2c_status_e err = i2c_master_read(&_i2c, (slaveAddress << 1), readBuff, bytesToRead);
      if(err != I2C_OK) { NT3HdebugPrint("onlyReadBytes() i2c_master_read error!"); }
      #ifdef NT3H_return_i2c_status_e
        return(err);
      #else
        return(err == I2C_OK);
      #endif
    }
    
    /**
     * write a block worth of bytes from a buffer to a memory address (note: Session register data must be written using writeSessRegByte() function)
     * @param blockAddress MEMory Address (MEMA) of the block
     * @param writeBuff a buffer of bytes to write to the device
     * @param bytesToWrite how many bytes of actual data to write, the remainder (to complete the NT3H_BLOCK_SIZE block) will be 0's
     * @return (i2c_status_e or bool) whether it wrote successfully
     */
    NT3H_ERR_RETURN_TYPE writeMemBlock(uint8_t blockAddress, uint8_t writeBuff[], uint8_t bytesToWrite=NT3H_BLOCK_SIZE) {
      // note: for some alternate (potentially intersting) code, please refer to writeBytes() in AS5600_thijs or TMP112_thijs
      if(bytesToWrite > NT3H_BLOCK_SIZE) {/* PANIC */  NT3HdebugPrint("writeMemBlock() can only write in blocks of 16 bytes, not more!"); return(NT3H_ERR_RETURN_TYPE_FAIL); }
      #if defined(I2C_OTHER_FRAME) // if the STM32 subfamily is capable of writing without sending a stop
        _i2c.handle.XferOptions = I2C_OTHER_AND_LAST_FRAME; // tell the peripheral it should send a STOP at the end
      #endif
      uint8_t copiedArray[NT3H_BLOCK_SIZE+1]; copiedArray[0]=blockAddress; for(uint8_t i=0;i<NT3H_BLOCK_SIZE;i++) { copiedArray[i+1]=(i<bytesToWrite) ? writeBuff[i] : 0; }
      i2c_status_e err = i2c_master_write(&_i2c, (slaveAddress << 1), copiedArray, NT3H_BLOCK_SIZE+1);
      if(err != I2C_OK) { NT3HdebugPrint("writeMemBlock() i2c_master_write error!"); }
      #ifdef NT3H_return_i2c_status_e
        return(err);
      #else
        return(err == I2C_OK);
      #endif
    }

    /**
     * update a Session register byte (must be done using this special command) 
     * @param registerIndex Register Address (REGA) of the register byte (0~7) (uint8_t)
     * @param regDat the byte to write to the register
     * @param mask the bits of the register that regDat should affect
     * @return (i2c_status_e or bool) whether it wrote successfully
     */
    NT3H_ERR_RETURN_TYPE writeSessRegByte(NT3H_CONF_SESS_REGS_ENUM registerIndex, uint8_t regDat, uint8_t mask=0xFF) {
      #if defined(I2C_OTHER_FRAME) // if the STM32 subfamily is capable of writing without sending a stop
        _i2c.handle.XferOptions = I2C_OTHER_AND_LAST_FRAME; // tell the peripheral it should send a STOP at the end
      #endif
      uint8_t regWriteArr[4] = {NT3H_SESS_REGS_MEMA, registerIndex, mask, regDat};
      i2c_status_e err = i2c_master_write(&_i2c, (slaveAddress << 1), regWriteArr, 4);
      if(err != I2C_OK) { NT3HdebugPrint("writeSessRegByte() i2c_master_write error!"); }
      #ifdef NT3H_return_i2c_status_e
        return(err);
      #else
        return(err == I2C_OK);
      #endif
    }

    // /**
    //  * send a repeated start condition.
    //  * IF I2C_RST_ON_OFF is enabled in the session registers, this will soft-reset the IC
    //  * ELSE it will probably do nothing, but it may annoy the memory arbitration untill the WDT resets it, idk.
    //  */
    // void softReset() {
    //   //TODO: find a way to make the STM32 i2c library send a repeated start
    // }


  #else
    #error("should never happen, platform optimization code has issue (probably at the top there)")
  #endif // platform-optimized code end

  /**
   * write a whole block (using a dedicated struct). Just a macro to the other writeMemBlock(), as &_blockStruct returns a uint8_t&
   * @param blockAddress MEMory Address (MEMA) of the block
   * @param blockToWrite (reference object) is a special struct which will ensure the whole block gets written all at once
   * @return (bool or esp_err_t or i2c_status_e, see on defines at top) whether it wrote successfully
   */
  NT3H_ERR_RETURN_TYPE writeMemBlock(uint8_t blockAddress, _blockStruct& blockToWrite) { return(writeMemBlock(blockAddress, &blockToWrite)); } // (just a macro)

  /*
  the remainder of the code can be found in the main header file: NT3H_thijs.h
  This is just a parent class, meant to hold all the low-level I2C implementations
  */
};


#endif // _NT3H_thijs_base_h