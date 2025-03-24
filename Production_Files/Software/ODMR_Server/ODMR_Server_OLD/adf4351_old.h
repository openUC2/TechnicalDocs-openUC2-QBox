/*!
   @file ADF4351.h
   
   @section author Author

   Nils Haverkamp

   @section license License

   MIT License
   Based on: https://github.com/dfannin/adf4351
*/

#ifndef ADF4351_H
#define ADF4351_H
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <stdint.h>


#define ADF_FREQ_MAX  3600
#define ADF_FREQ_MIN  2200

#define PIN_CE    17  ///< Ard Pin for Chip Enable
#define PIN_LD    32  ///< Ard Pin for Lock Detect
#define PIN_LE     5  ///< Ard Pin for SPI Slave Select
#define PIN_MOSI  23  ///< Ard Pin for SPI MOSI
#define PIN_MISO  12  ///< Ard Pin for SPI MISO
#define PIN_SCK   18  ///< Ard Pin for SPI CLK

class Reg
{
  public:
    Reg();
    uint32_t get()  ;
    void set(uint32_t value);
    uint32_t whole ;
    void setbf(uint8_t start, uint8_t len , uint32_t value) ;
    uint32_t getbf(uint8_t start, uint8_t len) ;
};

class ADF4351
{
  public:
    ADF4351(byte pin, uint8_t mode, unsigned long  speed, uint8_t order ) ;
    void begin() ;
    int setf(float freq);
    int changef(float freq);
    void enable();
    void disable();
    void writeDev(int n, Reg r) ;
    uint32_t getReg(int n) ;
    SPISettings spi_settings;
    uint8_t pinLE ;
    Reg R[6] ;
    unsigned int N_Int ;
    int RCounter ;
    int ClkDiv ;
    uint8_t Prescaler ;
    byte pwrlevel ;
};

#endif
