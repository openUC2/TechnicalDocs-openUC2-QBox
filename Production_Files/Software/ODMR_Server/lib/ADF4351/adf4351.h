/*!
   @file ADF4351.h

   @section author Author

   Nils Haverkamp
   edited by Benedict Diederich

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

#define ADF_FREQ_MAX 36
#define ADF_FREQ_MIN 22

class Reg
{
public:
  Reg();
  uint32_t get();
  void set(uint32_t value);
  uint32_t whole;
  void setbf(uint8_t start, uint8_t len, uint32_t value);
  uint32_t getbf(uint8_t start, uint8_t len);
};

class ADF4351
{
public:
  ADF4351(byte pinLE, byte pinCE, byte pinLD, byte pinMOSI, byte pinMISO, byte pinSCK, uint8_t mode, unsigned long speed, uint8_t order);
  void begin();
  int setf(float freq);
  int changef(float freq);
  void enable();
  void disable();
  void writeDev(int n, Reg r);
  uint32_t getReg(int n);
  SPISettings spi_settings;
  Reg R[6];
  unsigned int N_Int;
  int RCounter;
  int ClkDiv;
  uint8_t Prescaler;
  byte pwrlevel;

  uint8_t pinLE;
  uint8_t pinCE;
  uint8_t pinLD;
  uint8_t pinMOSI;
  uint8_t pinMISO;
  uint8_t pinSCK;
};

#endif
