#include <Wire.h>
//#include <Adafruit_TSL2591.h>
#include "ADF4351.h"
#include <SPI.h>

// Photodetector setup
//Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// D7 -> GPIO_44 -> SPI_CS (PIN_LE)
// D8 -> GPIO_07 -> SPI_SCK (PIN_SCK)
// D9 -> GPIO_08 -> SPI_POCI (PIN_MISO, unused if not reading back)
// D10 -> GPIO_09 -> SPI_PICO (PIN_MOSI)

// Frequency generator setup
#define clock D8
#define data D10
#define LE D7
#define CE 0

// D7 -> GPIO_44 -> SPI_CS (PIN_LE)
// D8 -> GPIO_07 -> SPI_SCK (PIN_SCK)
// D9 -> GPIO_08 -> SPI_POCI (PIN_MISO, unused if not reading back)
// D10 -> GPIO_09 -> SPI_PICO (PIN_MOSI)

ADF4351 adf4351(clock, data, LE, CE);

const double startFrequency = 2770.0; // Start frequency in MHz
const double endFrequency = 2970.0;   // End frequency in MHz
const double stepFrequency = 2.0;     // Frequency step in MHz
double currentFrequency = startFrequency;
double testFrequency = 2950.0;

//Register values
#define PFD 2.0 //Mhz
int INT;

//Photodetector Power
#define VCC_PIN 2  // GPIO pin for photodetector power

//Smoothing Signal
#define numReadings 5 // Number of readings per frequency step for averaging


void setup() {
  Serial.begin(115200);


  // Power the photodetector
    //pinMode(VCC_PIN, OUTPUT);
    //digitalWrite(VCC_PIN, HIGH);

  // Initialize photodetector
    //if (!tsl.begin()) {
      //Serial.println("TSL2591 not detected! Check wiring.");
      //while (1);
    //}

    //tsl.setGain(TSL2591_GAIN_HIGH);
    //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    //Serial.println("TSL2591 initialized successfully!");

  // Initialize frequency generator
  Serial.println("Initialize frequency generator....\n");
  adf4351.begin();
  updateFrequency(startFrequency);
  Serial.println("ADF4351 initialized successfully!\n");
}

void loop() {
  for (double frequency = startFrequency; frequency <= endFrequency; frequency += stepFrequency) {
    updateFrequency(frequency);
    //delay(50); // 100ms so Photodector can catch up

    // Collect and average photodetector readings
    /*uint32_t sum = 0;
    double sumSq = 0;
    for (int i = 0; i < numReadings; i++) {
      uint32_t rawData = 0; //tsl.getFullLuminosity();             //since detector is not connected
      uint16_t visibleLight = rawData & 0xFFFF;
      sum += visibleLight;
      sumSq += visibleLight * visibleLight;
      //delay(50); // 100ms delay between readings
    }

    // Compute the mean value
    double meanValue = sum / numReadings;
    double squaredMean = sqrt(sumSq /numReadings);

    // Send frequency and averaged photodetector value to Python
    */
    Serial.print(frequency);
    /*Serial.print(",");
    Serial.println(squaredMean);*/
  }
}

// Frequency generator function
void updateFrequency(double frequency) {
  uint32_t reg[6];
  calculateRegisterValues(frequency, reg);
  
  adf4351.WriteRegister(reg[5]);
  adf4351.WriteRegister(reg[4]);
  adf4351.WriteRegister(reg[3]);
  adf4351.WriteRegister(reg[2]);
  adf4351.WriteRegister(reg[1]);
  adf4351.WriteRegister(reg[0]);
}

void calculateRegisterValues(double RFOUT, uint32_t *reg) {
  INT = (int)(RFOUT / PFD);

  reg[0] = 0x390020; //INT << 15;
  reg[1] = 0x8029; //11;
  reg[2] = 0x10E42;//0xC8E42;
  reg[3] = 0x4B3;
  reg[4] = 0x8C803C; //1003C;
  reg[5] = 0x580005;

  //Serial.print("RFOUT: "); Serial.print(RFOUT); Serial.print(" MHz: "); 
  //Serial.print("0x"); Serial.println(reg[0]); //Serial.print(", "); 
  //Serial.print("0x"); Serial.print(reg[1], HEX); Serial.print(", ");
  //Serial.print("0x"); Serial.print(reg[2], HEX); Serial.print(", ");
  //Serial.print("0x"); Serial.print(reg[3], HEX); Serial.print(", ");
  //Serial.print("0x"); Serial.print(reg[4], HEX); Serial.print(", ");
  //Serial.print("0x"); Serial.print(reg[5], HEX); Serial.print(", ");
}
