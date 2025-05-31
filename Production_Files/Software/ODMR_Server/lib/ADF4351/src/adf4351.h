#ifndef __ADF4351_H__
#define __ADF4351_H__

#include <Arduino.h>

class ADF4351{
	public:
    ADF4351(int SCLK, int DATA, int LE, int CE);
    void begin(void);
    void calculateRegisterValues(double RFOUT, uint32_t *reg);
    void updateFrequency(double frequency);
    void stop(void) {
        // Disable output by setting CE low
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
