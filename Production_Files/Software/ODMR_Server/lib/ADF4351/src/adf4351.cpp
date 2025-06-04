#include "adf4351.h"

#include <stdint.h>
#include <math.h>
// Constructor function; initializes communication pinouts
ADF4351::ADF4351(int SCLK, int DATA, int LE, int CE)
{
	_sclk = SCLK;
	_data = DATA;
	_le = LE;
	_ce = CE;
}

// Frequency generator function
/*OLD
void ADF4351::updateFrequency(double frequency)
{
	uint32_t reg[6];
	calculateRegisterValues(frequency, reg);
	ADF4351::WriteRegister(reg[5]);
	ADF4351::WriteRegister(reg[4]);
	ADF4351::WriteRegister(reg[3]);
	ADF4351::WriteRegister(reg[2]);
	ADF4351::WriteRegister(reg[1]);
	ADF4351::WriteRegister(reg[0]);
}
*/


/* Euclidean GCD for reducing FRAC / MOD */
static uint32_t gcd32(uint32_t a, uint32_t b)
{
    while (b) { uint32_t t = b; b = a % b; a = t; }
    return a;
}

// ─────────────────────────────────────────────────────────────
//  Public call: set new carrier on the ADF4351
// ─────────────────────────────────────────────────────────────
void ADF4351::updateFrequency(double freqHz)
{
	if (freqHz < 2.2e9 || freqHz > 4.4e9) {
		Serial.println("ADF4351: Frequency out of range (2.2 GHz to 4.4 GHz)");
		return;
	}
	//Serial.print("ADF4351: Setting frequency to ");
	//Serial.print(freqHz / 1e6, 1);
    uint32_t reg[6];
    calculateRegisterValues(freqHz, reg);
    for (int i = 5; i >= 0; --i) WriteRegister(reg[i]);   // R5 → R0
}

// ─────────────────────────────────────────────────────────────
//  Register generator   (30 MHz reference, 100 kHz step)
// ─────────────────────────────────────────────────────────────
void ADF4351::calculateRegisterValues(double RFout, uint32_t *reg)
{
    constexpr double REF          = 30e6;      // reference crystal
    constexpr double PFD          = REF;       // R-counter = 1
    constexpr double CHAN_SPACING = 100e3;     // step size

    /* ── choose RF divider so 2.2 GHz ≤ VCO ≤ 4.4 GHz ── */
    uint8_t  divSel = 0;                       // 0:/1 … 7:/128
    double   vco    = RFout;
    while (vco < 2.2e9 && divSel < 7) { vco *= 2.0; ++divSel; }

    /* ── INT / FRAC / MOD ── */
    uint32_t INT  = static_cast<uint32_t>(vco / PFD);
    uint32_t MOD  = static_cast<uint32_t>(round(PFD / CHAN_SPACING));
    uint32_t FRAC = static_cast<uint32_t>(
                        round((vco - static_cast<double>(INT) * PFD) /
                              CHAN_SPACING));

    if (FRAC == 0) {
        MOD = 2;                               // ADI recommendation, int-N
    } else {
        uint32_t g = gcd32(FRAC, MOD);         // reduce fraction
        FRAC /= g;
        MOD  /= g;
    }

    constexpr uint32_t PHASE = 1;

    /* ── build register words ── */
    reg[0] = (INT  << 15) | (FRAC << 3) | 0x0;
    reg[1] = (1UL  << 27) | (PHASE << 15) | (MOD << 3) | 0x1;
    reg[2] = 0x004E42;                         // low-noise / CP 2.5 mA
    reg[3] = 0x0004B3;                         // clock-divider off
    reg[4] = (0x009F003C & ~(0x7UL << 20)) | (static_cast<uint32_t>(divSel) << 20);
    reg[5] = 0x00580005;                       // reserved / lock-detect
}

/* OLD 
void ADF4351::calculateRegisterValues(double RFOUT, uint32_t *reg)
{
	// since we use 30MhZ as a base reference, we can only take frequencies that are multiples of 15Mhz
	// the formular to compute the register values is:
	// RFOUT = (INT + FRAC / MOD) * PFD
	// where RFOUT is the output frequency, INT is the integer part of the frequency, FRAC is the fractional part of the frequency, MOD is the modulus value, and PFD is the phase frequency detector frequency.
	if (0)
	{
		INT = (int)(RFOUT / 10);
		// display value as HEX
		Serial.print("INT: ");
		Serial.println(INT, HEX);
		reg[0] = INT << 15; // 0x390020; //INT << 15;
		reg[1] = 0x8029;	// 11;
		reg[2] = 0x258E42;	// 0xC8E42;
		reg[3] = 0x4B3;
		reg[4] = 0x8C803C; // 1003C;
		reg[5] = 0x580005;
	}
	else
	{

		
		// This gives 1.8Ghz
		// PFD = 15Mhz
		// RFOUT = 1.8Ghz
		// INT = (int)(RFOUT / PFD);
		

		reg[0] = 0x3C0000;
		reg[1] = 0x8008011;
		reg[2] = 0x4E42;
		reg[3] = 0x4B3;
		reg[4] = 0x9F003C;
		reg[5] = 0x580005;
	}
}
*/

void ADF4351::begin(void)
{
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
void ADF4351::WriteRegister(long regData)
{
	digitalWrite(_le, LOW);
	_regData = regData;

	for (int i = 0; i < 32; i++)
	{
		if (((_regData << i) & 0x80000000) == 0x80000000)
		{
			digitalWrite(_data, 1);
		}
		else
		{
			digitalWrite(_data, 0);
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
