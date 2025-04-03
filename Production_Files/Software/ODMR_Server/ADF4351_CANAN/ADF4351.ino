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
#define PFD 15.0 //Mhz
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
  ADF4351 adf4351(clock, data, LE, CE);

  adf4351.begin();
  adf4351.updateFrequency(startFrequency);
  Serial.println("ADF4351 initialized successfully!\n");
}

void loop() {
  for (double frequency = startFrequency; frequency <= endFrequency; frequency += stepFrequency) {
    adf4351.updateFrequency(frequency);
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


