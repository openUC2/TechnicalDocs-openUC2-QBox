#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <SPI.h>
#include "adf4351.h"           // Your ADF4351 library
#include <Adafruit_TSL2591.h>  // Adafruit TSL2591 light sensor
#include <Adafruit_NeoPixel.h> // Neopixel-Bibliothek einbinden

/*
XIAO pin	XIAO ESP32S3 pin	Board Function	Test Point
D0	GPIO_01	Not used	TP310
D1	GPIO_02	Not used	TP311
D2	GPIO_03	ADF4350 MuxOut (optional)	TP312
D3	GPIO_04	ADF4350 PLL lock (optional)	TP313
D4	GPIO_05	I2C SDA to connectors	TP314
D5	GPIO_06	I2C SCL to connectors	TP315
D6	U0TXD/GPIO_43	Neopixel data input	TP316
D7	U0RXD/GPIO_44	SPI_CS to ADF4350	TP309
D8	GPIO_07	SPI_SCK	TP308
D9	GPIO_08	SPI_POCI (unused)	TP307
D10	GPIO_09	SPI_PICO	TP306
*/

// SPI pins adjusted to match hardware mapping:
// D7 -> GPIO_44 -> SPI_CS (PIN_LE)
// D8 -> GPIO_07 -> SPI_SCK (PIN_SCK)
// D9 -> GPIO_08 -> SPI_POCI (PIN_MISO, unused if not reading back)
// D10 -> GPIO_09 -> SPI_PICO (PIN_MOSI)

#define PIN_CE 17  ///< Chip Enable remains on GPIO_17
#define PIN_LD 32  ///< Lock Detect remains on GPIO_32
#define PIN_LE 44  ///< SPI Slave Select now on GPIO_44
#define PIN_MOSI 9 ///< SPI MOSI now on GPIO_09
#define PIN_MISO 8 ///< SPI MISO now on GPIO_08
#define PIN_SCK 7  ///< SPI CLK now on GPIO_07
#define PIN_NEOPIXEL 43
#define NUM_PIXELS 16 // Anzahl der LEDs im Streifen

// Neopixel
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// I2C
static const int SDA_PIN = 5; // GPIO_05
static const int SCL_PIN = 6; // GPIO_06

// Wi-Fi credentials
const char *SSID = "openUC2_ODMR";
const char *PASSWORD = "";

// Laser pin example
static const int LASER_PIN = 10;
bool laserState = false;

// ADF4351 object (example SPI settings)
// ADF4351::ADF4351(byte pinLE, byte pinCE, byte pinLD, byte pinMOSI, byte pinMISO, byte pinSCK, uint8_t mode, unsigned long speed, uint8_t order)
ADF4351 adf(PIN_LE, PIN_CE, PIN_LD, PIN_MOSI, PIN_MISO, PIN_SCK, SPI_MODE0, 5000000UL, MSBFIRST);

// TSL2591 sensor
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// WebServer on port 80
WebServer server(80);

// Neopixel settings
long firstPixelHue = 0;
int pixelWait = 1;

// Utility to serve static files from SPIFFS
void handleFileRequest(const String &path)
{
  String actualPath = path;
  if (actualPath.endsWith("/"))
  {
    actualPath += "index.html";
  }
  File file = SPIFFS.open(actualPath, "r");
  if (!file || file.isDirectory())
  {
    server.send(404, "text/plain", "Not found");
    return;
  }

  String contentType = "text/html";
  if (actualPath.endsWith(".css"))
    contentType = "text/css";
  if (actualPath.endsWith(".js"))
    contentType = "application/javascript";
  if (actualPath.endsWith(".png"))
    contentType = "image/png";
  if (actualPath.endsWith(".jpg"))
    contentType = "image/jpeg";
  if (actualPath.endsWith(".ico"))
    contentType = "image/x-icon";
  if (actualPath.endsWith(".svg"))
    contentType = "image/svg+xml";

  server.streamFile(file, contentType);
  file.close();
}

// Example: read TSL2591 sensor (light intensity)
uint32_t readTSL2591()
{
  sensors_event_t event;
  tsl.getEvent(&event);
  // event.light is in lux, but you can read raw channels as well
  uint32_t lux = (uint32_t)event.light;
  return lux;
}

// Example /laser_act handler for JSON:
// {"task": "/laser_act", "LASERid":1, "LASERval": 0 or 1}
void handleLaserAct()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"no JSON body\"}");
    return;
  }
  String body = server.arg("plain");
  // Minimal parse for demonstration
  // (Use ArduinoJson if you need more robust parsing)
  if (body.indexOf("\"LASERval\":1") >= 0)
  {
    laserState = true;
    digitalWrite(LASER_PIN, HIGH);
  }
  else
  {
    laserState = false;
    digitalWrite(LASER_PIN, LOW);
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Example /odmr_act handler for JSON:
// {"task": "/odmr_act", "measure":1}
void handleOdmrAct()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"no JSON body\"}");
    return;
  }
  // In actual usage, parse measure instructions
  // For demonstration: just respond with a random value or TSL reading
  uint32_t reading = readTSL2591();
  String response = String("{\"status\":\"measurement ok\",\"light\":") + reading + "}";
  server.send(200, "application/json", response);
}

// Example /measure?frequenz=xxxx
// We set the frequency on the ADF4351 and read the TSL2591 sensor
// Return "freq intensity magnetfield" style data (just placeholders here)
void handleMeasure()
{
  Serial.println("STart measurement");
  if (!server.hasArg("frequenz"))
  {
    Serial.println("No frequency set");
    server.send(400, "text/plain", "Error: no 'frequenz' param");
    return;
  }
  float freqRequested = server.arg("frequenz").toFloat()/100;
  // Make sure it's within ADF4351 range
  if (freqRequested < ADF_FREQ_MIN || freqRequested > ADF_FREQ_MAX)
  {
    Serial.println("Freq out of range");
    server.send(400, "text/plain", "Freq out of range");
    return;
  }
  // For demonstration, call changef
  adf.changef(freqRequested);

  // Read intensity
  uint32_t intensity = readTSL2591();

  // If you have a magnetometer or something, read that too
  float exampleMagVal = 123.0f; // Placeholder

  // Return space-separated: freq intensity magnetfield
  // or however your front-end expects it
  String reply = String(freqRequested*100, 1) + " " + intensity + " " + exampleMagVal;
  server.send(200, "text/plain", reply);
}

void i2c_scan()
{
  byte error, address;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for (address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void setup()
{
  disableCore1WDT(); // Deaktiviert den Watchdog für Core 1
  disableLoopWDT();  // Deaktiviert den Watchdog für die loop()
  
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);

  Serial.begin(115200);
  delay(3000); // Allow time to connect
  Serial.println("Booting...");

  // Wi-Fi
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String macID = String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  macID.toUpperCase();
  String dynamicSSID = "ODMR_" + macID;

  // Wi-Fi Access Point starting
  WiFi.mode(WIFI_AP);
  WiFi.softAP(dynamicSSID.c_str(), PASSWORD);

  Serial.print("Access Point has started with SSID: ");
  Serial.println(dynamicSSID);

  // SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS mount failed");
  }
  else
  {
    Serial.println("SPIFFS mounted");
  }

  // I2C initialization for TSL2591
  Wire.begin(SDA_PIN, SCL_PIN);

  // Neopixel initialisieren
  Serial.println("Neopixel init");
  strip.begin();
  strip.show();            // Alle LEDs ausschalten
  strip.setBrightness(50); // Helligkeit einstellen (0-255)

  // perform I2C scan to verify TSL2591 is connected
  i2c_scan();

  // TSL2591 init
  if (!tsl.begin())
  {
    Serial.println("TSL2591 not found");
  }
  else
  {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
  }

  // ADF4351 init
  Serial.println("ADF4351 init");
  adf.begin();
  adf.setf(2800.0f); // Some initial frequency (example)

  // Setup routes
  server.onNotFound([]()
                    { handleFileRequest(server.uri()); });
  server.on("/laser_act", HTTP_POST, handleLaserAct);
  server.on("/odmr_act", HTTP_POST, handleOdmrAct);
  server.on("/measure", HTTP_GET, handleMeasure);

  server.begin();
}

void loop()
{
  server.handleClient();

  if (firstPixelHue > 5 * 65536)
    firstPixelHue = 0;
  { // 5 Zyklen durch den Regenbogen
    for (int i = 0; i < strip.numPixels(); i++)
    {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(pixelWait);
  }
  firstPixelHue += 256;
}
