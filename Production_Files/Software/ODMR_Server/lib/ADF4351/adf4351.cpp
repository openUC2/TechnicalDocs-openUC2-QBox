#include "ADF4351.h"

// Constructor function; initializes communication pinouts
ADF4351::ADF4351(int SCLK, int DATA, int LE, int CE){
	_sclk = SCLK;
	_data=DATA;
	_le=LE;
	_ce=CE;
}



// Frequency generator function
void ADF4351::updateFrequency(double frequency) {
	uint32_t reg[6];
	calculateRegisterValues(frequency, reg);
	ADF4351::WriteRegister(reg[5]);
	ADF4351::WriteRegister(reg[4]);
	ADF4351::WriteRegister(reg[3]);
	ADF4351::WriteRegister(reg[2]);
	ADF4351::WriteRegister(reg[1]);
	ADF4351::WriteRegister(reg[0]);
  }
  
  void ADF4351::calculateRegisterValues(double RFOUT, uint32_t *reg) {
	// since we use 30MhZ as a base reference, we can only take frequencies that are multiples of 15Mhz 
	// the formular to compute the register values is:
	// RFOUT = (INT + FRAC / MOD) * PFD
	// where RFOUT is the output frequency, INT is the integer part of the frequency, FRAC is the fractional part of the frequency, MOD is the modulus value, and PFD is the phase frequency detector frequency.
	INT = (int)(RFOUT / PFD);
	// display value as HEX 
	Serial.print("INT: ");
	Serial.println(INT, HEX);
	reg[0] = INT << 15;// 0x390020; //INT << 15;
	reg[1] = 0x8029; //11;
	reg[2] = 0x10E42;//0xC8E42;
	reg[3] = 0x4B3;
	reg[4] = 0x8C803C; //1003C;
	reg[5] = 0x580005;
  }

void ADF4351::begin(void){
	pinMode(_sclk, OUTPUT);
	pinMode(_data, OUTPUT);
	pinMode(_le, OUTPUT);
	pinMode(_ce, OUTPUT);
	
	// initialize
	digitalWrite(_sclk, LOW);
	digitalWrite(_data, LOW);
	digitalWrite(_le, LOW);
	digitalWrite(_ce, HIGH);
}


// write data into register
void ADF4351::WriteRegister(long regData){
	digitalWrite(_le, LOW);
	_regData = regData;

	for(int i=0; i<32; i++)
	{
		if(((_regData<<i)&0x80000000)==0x80000000)
		{
			digitalWrite(_data,1);
			
		}
		else
		{
			digitalWrite(_data,0) ;
			
		}

		digitalWrite(_sclk, HIGH);
		delay(1);
		digitalWrite(_sclk, LOW);
	}
	
	// load data into register
	digitalWrite(_le, HIGH);
	delay(1);
	digitalWrite(_le, LOW);
}