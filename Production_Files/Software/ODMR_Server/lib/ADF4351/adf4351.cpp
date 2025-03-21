/*!
   @section author Author

   Nils Haverkamp

   @section license License

   MIT License
   Based on: https://github.com/dfannin/adf4351
*/

#include "adf4351.h"


ADF4351::ADF4351(byte pinLE, byte pinCE, byte pinLD, byte pinMOSI, byte pinMISO, byte pinSCK, uint8_t mode, unsigned long speed, uint8_t order)
{

  this->pinLE = pinLE;
  this->pinCE = pinCE;
  this->pinLD = pinLD;
  this->pinMOSI = pinMOSI;
  this->pinMISO = pinMISO;
  this->pinSCK = pinSCK;

  spi_settings = SPISettings(speed, order, mode);
  RCounter = 100;
  pwrlevel = 3;
}

void ADF4351::begin()
{
  Serial.println("ADF4351 begin with pins: ");
  Serial.print("LE: ");
  Serial.println(this->pinLE);
  Serial.print("CE: ");
  Serial.println(this->pinCE);
  Serial.print("LD: ");
  Serial.println(this->pinLD);
  Serial.print("MOSI: ");
  Serial.println(this->pinMOSI);
  Serial.print("MISO: ");
  Serial.println(this->pinMISO);
  Serial.print("SCK: ");
  Serial.println(this->pinSCK);
  
  pinMode(this->pinLE, OUTPUT);
  digitalWrite(this->pinLE, LOW);
  pinMode(this->pinCE, OUTPUT);
  digitalWrite(this->pinCE, HIGH);
  // pinMode(this->pinLD, INPUT);
  SPI.begin(this->pinSCK, this->pinMISO, this->pinMOSI);
};

int ADF4351::setf(float freq)
{
  Serial.println("ADF4351 setf");
  if (freq > 3600)
  {
    Serial.println("Freq > 3600");
    return 1;
  }
  if (freq < 2200)
  {
    Serial.println("Freq < 2200");
    return 1;
  }
  N_Int = (int)(freq * 10);
  if (N_Int < 23 || N_Int > 65535)
  {
    Serial.print(F("N_Int="));
    Serial.println(N_Int);
    Serial.println(F("N_Int out of range"));
    return 1;
  }
  R[0].set(0UL);
  R[0].setbf(15, 16, N_Int);
  R[1].set(32785UL);
  R[2].set(4034UL);
  R[2].setbf(14, 10, RCounter);
  R[3].set(33971UL);
  R[4].set(8798244UL);
  R[4].setbf(3, 2, pwrlevel);
  R[5].set(5767173UL);
  for (int i = 5; i > -1; i--)
  {
    writeDev(i, R[i]);
  }
  return 0;
}

int ADF4351::changef(float freq)
{
  /*
  if (freq > 3600)
  {

    return 1;
  }
  if (freq < 2200)
  {
    return 1;
  }
    */
  N_Int = (int)(freq * 10);
  if (N_Int < 23 || N_Int > 65535)
  {
    Serial.print(F("N_Int="));
    Serial.println(N_Int);
    Serial.println(F("N_Int out of range"));
    return 1;
  }
  R[0].set(0UL);
  R[0].setbf(15, 16, N_Int);
  writeDev(0, R[0]);
  Serial.print("Changing frequency: ");
  Serial.println(freq);
  return 0;
}
void ADF4351::enable()
{
  digitalWrite(this->pinCE, HIGH);
}

void ADF4351::disable()
{
  digitalWrite(this->pinCE, HIGH);
}

void ADF4351::writeDev(int n, Reg r)
{
  byte txbyte;
  int i;
  digitalWrite(pinLE, LOW);
  delayMicroseconds(10);
  i = n; // not used
  for (i = 3; i > -1; i--)
  {
    txbyte = (byte)(r.whole >> (i * 8));
    SPI.transfer(txbyte);
  }
  digitalWrite(pinLE, HIGH);
  delayMicroseconds(5);
  digitalWrite(pinLE, LOW);
}

uint32_t ADF4351::getReg(int n)
{
  return R[n].whole;
}

Reg::Reg()
{
  whole = 0;
}

uint32_t Reg::get()
{
  return whole;
}

void Reg::set(uint32_t value)
{
  whole = value;
}

void Reg::setbf(uint8_t start, uint8_t len, uint32_t value)
{
  uint32_t bitmask = ((1UL << len) - 1UL);
  value &= bitmask;
  bitmask <<= start;
  whole = (whole & (~bitmask)) | (value << start);
}

uint32_t Reg::getbf(uint8_t start, uint8_t len)
{
  uint32_t bitmask = ((1UL << len) - 1UL) << start;
  uint32_t result = (whole & bitmask) >> start;
  return (result);
}
