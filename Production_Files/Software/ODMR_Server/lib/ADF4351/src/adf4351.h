#ifndef __ADF4351_H__
#define __ADF4351_H__

#include <Arduino.h>

class ADF4351{
	public:
    ADF4351(int SCLK, int DATA, int LE, int CE);
    void begin(void);
    void calculateRegisterValues(double RFOUT, uint32_t *reg);
    void updateFrequency(double frequency);
    // RF off. The board does not wire CE (pin 0 is a dummy), so power down over
    // SPI: R4 with RF-out disabled, then R2 with the power-down bit set.
    // updateFrequency() rewrites R4/R2 and turns the output back on.
    void stop(void) {
        WriteRegister(0x009F001C); // R4: RF out enable (bit 5) = 0
        WriteRegister(0x004E62);   // R2: power-down (bit 5) = 1
        digitalWrite(_ce, LOW);
    }
  
    //Register values
    float PFD = 15.0; //Mhz
    int INT = 0;

    void WriteRegister(long regData);

  private:
    int _data, _sclk, _le, _ce;
    long _regData;
};

#endif
