#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
// #include <SPIFFS.h>  // No longer needed - using header files
#include <SPI.h>
#include <Adafruit_TSL2591.h>  // Adafruit TSL2591 light sensor
#include <Adafruit_NeoPixel.h> // Neopixel-Bibliothek einbinden

#include "adf4351.h"
#include <SPI.h>

// website
#include "website/style_css.h"
#include "website/index_html.h"
#include "website/messung_html.h"
#include "website/messung_webserial_html.h"
#include "website/justage_html.h"
#include "website/infos_html.h"
// #include "website/nvgitter_png.h"  // Excluded for size reasons

#define ADF_FREQ_MIN 2200.0f // Min frequency for ADF4351 (2.2 GHz)
#define ADF_FREQ_MAX 4400.0f // Max frequency for ADF4351 (4.4 GHz)

// Photodetector setup
// Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// D7 -> GPIO_44 -> SPI_CS (PIN_LE)
// D8 -> GPIO_07 -> SPI_SCK (PIN_SCK)
// D9 -> GPIO_08 -> SPI_POCI (PIN_MISO, unused if not reading back)
// D10 -> GPIO_09 -> SPI_PICO (PIN_MOSI)

// Frequency generator setup
#define clock D8
#define data D10
#define LE D7
#define CE 0
#define PIN_NEOPIXEL D6

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

#define NUM_PIXELS 30 // Anzahl der LEDs im Streifen

// Neopixel
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// I2C
static const int SDA_PIN = D4; // GPIO_05
static const int SCL_PIN = D5; // GPIO_06

// Wi-Fi credentials
const char *SSID = "openUC2_ODMR";
const char *PASSWORD = "";

// serial buffer for webserial comm.
String rxBuf;

// Laser pin example
static const int LASER_PIN = 10;
bool laserState = false;

// canans ADF Library modified by bene
ADF4351 adf(clock, data, LE, CE);

// TSL2591 sensor
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// WebServer on port 80
WebServer server(80);

// Neopixel settings
long firstPixelHue = 0;
int pixelWait = 1;

// LED status management
enum LEDStatus {
  LED_NO_CLIENT,     // white
  LED_CONNECTED,     // rainbow
  LED_MEASURING,     // red
  LED_INTENSITY      // blue
};

LEDStatus currentLEDStatus = LED_NO_CLIENT;
unsigned long lastLEDUpdate = 0;
const unsigned long LED_UPDATE_INTERVAL = 50; // Update every 50ms
unsigned long lastIntensityRequest = 0;
const unsigned long INTENSITY_TIMEOUT = 2000; // 2 seconds timeout

// LED control functions
void setLEDStatus(LEDStatus status) {
  currentLEDStatus = status;
}

void updateLEDs() {
  if (millis() - lastLEDUpdate < LED_UPDATE_INTERVAL) return;
  lastLEDUpdate = millis();
  
  strip.setBrightness(127); // Set to 50% brightness as requested
  
  switch (currentLEDStatus) {
    case LED_NO_CLIENT:
      // White color for no client connected
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));
      }
      break;
      
    case LED_CONNECTED:
      // Rainbow effect when client connected but idle
      for (int i = 0; i < strip.numPixels(); i++) {
        int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
      firstPixelHue += 256;
      if (firstPixelHue > 5 * 65536) firstPixelHue = 0;
      break;
      
    case LED_MEASURING:
      // Red color when measuring frequency sweep
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(255, 0, 0));
      }
      break;
      
    case LED_INTENSITY:
      // Blue color when monitoring intensity for alignment
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255));
      }
      break;
  }
  
  strip.show();
}

// Utility to serve static files from header files
void handleFileRequest(const String &path)
{
  String actualPath = path;
  if (actualPath.endsWith("/"))
  {
    actualPath += "index.html";
  }

  String contentType = "text/html";
  const char* content = nullptr;

  // Route specific files to their header file content
  if (actualPath == "/index.html")
  {
    contentType = "text/html";
    content = INDEX_HTML;
  }
  else if (actualPath == "/messung.html")
  {
    contentType = "text/html";
    content = MESSUNG_HTML;
  }
  else if (actualPath == "/messung_webserial.html")
  {
    contentType = "text/html";
    content = MESSUNG_WEBSERIAL_HTML;
  }
  else if (actualPath == "/justage.html")
  {
    contentType = "text/html";
    content = JUSTAGE_HTML;
  }
  else if (actualPath == "/infos.html")
  {
    contentType = "text/html";
    content = INFOS_HTML;
  }
  else if (actualPath == "/style.css")
  {
    contentType = "text/css";
    content = STYLE_CSS;
  }
  else if (actualPath == "/NVGitter.png")
  {
    // Image not included in header version for size reasons
    server.send(404, "text/plain", "Image not available in header version");
    return;
  }

  if (content != nullptr)
  {
    server.send_P(200, contentType.c_str(), content);
  }
  else
  {
    server.send(404, "text/plain", "Not found");
  }
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

// read IR from TSL2591
uint16_t readIR()
{
  uint16_t x = tsl.getLuminosity(TSL2591_INFRARED);
  return x;
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

  // Set LED to measuring mode (red)
  setLEDStatus(LED_MEASURING);
  
  for (int iFrequencyRequested = 2200; iFrequencyRequested <= 4400; iFrequencyRequested += 100)
  {
    adf.updateFrequency(iFrequencyRequested * 1e6); // Set frequency in Hz
    delay(200);                                     // Wait for the frequency to stabilize
    Serial.println("Set frequency: " + String(iFrequencyRequested * 1e6) + " Hz");
  }
  
  // Return to connected mode (rainbow) after measurement
  setLEDStatus(LED_CONNECTED);
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
  // uint32_t reading = readTSL2591();
  uint32_t reading = readIR(); // Read IR instead of light for demonstration

  String response = String("{\"status\":\"measurement ok\",\"light\":") + reading + "}";
  server.send(200, "application/json", response);
}

// Example /measure?frequenz=xxxx
// We set the frequency on the ADF4351 and read the TSL2591 sensor
// Return "freq intensity magnetfield" style data (just placeholders here)
void handleMeasure()
{
  if (!server.hasArg("frequenz"))
  {
    Serial.println("No frequency set");
    server.send(400, "text/plain", "Error: no 'frequenz' param");
    return;
  }
  float freqRequested = server.arg("frequenz").toFloat();
  // Make sure it's within ADF4351 range
  if (freqRequested < ADF_FREQ_MIN || freqRequested > ADF_FREQ_MAX)
  {
    Serial.println("Freq out of range");
    server.send(400, "text/plain", "Freq out of range");
    return;
  }
  // For demonstration, call changef

  // Set the frequency on the ADF4351
  Serial.println("Setting frequency: " + String(freqRequested, 1));
  setLEDStatus(LED_MEASURING); // Set LED to red while measuring
  adf.updateFrequency(freqRequested * 1e6); // Set frequency in Hz

  // Read intensity
  // uint32_t intensity = readTSL2591();
  uint32_t intensity = readIR(); // Read IR instead of light for demonstration

  // If you have a magnetometer or something, read that too
  float exampleMagVal = 123.0f; // Placeholder

  // Return space-separated: freq intensity magnetfield
  // or however your front-end expects it
  String reply = String(freqRequested, 1) + " " + intensity + " " + exampleMagVal;
  //Serial.println("Intensity: " + String(intensity) + " Mag: " + String(exampleMagVal));
  //Serial.println("Reply: " + reply);
  server.send(200, "text/plain", reply);
  
  // Return to connected status after measurement
  setLEDStatus(LED_CONNECTED);
}

// Live intensity reading for photodiode alignment
void handleIntensity()
{
  // Set LED to blue for intensity monitoring mode and track timestamp
  setLEDStatus(LED_INTENSITY);
  lastIntensityRequest = millis();
  
  uint32_t intensity = readIR(); // Read photodiode intensity
  String response = String("{\"intensity\":") + intensity + "}";
  server.send(200, "application/json", response);
}
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

// only for esp32s3
#ifdef ESP32S3
  disableCore1WDT(); // Deactivate Watchdog for core 1
#endif
  disableLoopWDT(); // Deactivate Watchdog for loop

  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);

  Serial.begin(115200);
  delay(300); // Allow time to connect
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

  //Serial.print("Access Point has started with SSID: ");
  //Serial.println(dynamicSSID);

  // SPIFFS no longer needed - using header files instead
  // if (!SPIFFS.begin(true))
  // {
  //   Serial.println("SPIFFS mount failed");
  // }
  // else
  // {
  //   Serial.println("SPIFFS mounted");
  // }
  Serial.println("Using header files for website content");

  // I2C initialization for TSL2591
  Wire.begin(SDA_PIN, SCL_PIN);

  // Neopixel initialisieren
  strip.begin();
  strip.show();             // Alle LEDs ausschalten
  strip.setBrightness(127); // Helligkeit auf 50% einstellen (0-255)

  // perform I2C scan to verify TSL2591 is connected
  i2c_scan();

  // TSL2591 init
  if (!tsl.begin())
  {
    Serial.println("TSL2591 not found");
  }
  else
  {
    tsl.setGain(TSL2591_GAIN_MAX);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    // turn off led on TSL2591

    
  }

  // ADF4351 init
  Serial.println("ADF4351 init");
  adf.begin();
  // adf.updateFrequency(1800.0f); // Some initial frequency (example)
  adf.updateFrequency(2.2e9); // 2.2 GHz // 1.800 GHz ─ writes R5…R0

  // Setup routes
  server.onNotFound([]()
                    { handleFileRequest(server.uri()); });
  server.on("/laser_act", HTTP_POST, handleLaserAct);
  server.on("/odmr_act", HTTP_POST, handleOdmrAct);
  server.on("/measure", HTTP_GET, handleMeasure);
  server.on("/intensity", HTTP_GET, handleIntensity);

  server.begin();
  // TODO: Need a function that disables the adf4351 output
  adf.stop(); // Disable output initially
}

void loop()
{
  server.handleClient();
  
  // Update LED status indicators
  updateLEDs();
  
  // Check if any clients are connected to determine LED status
  if (WiFi.softAPgetStationNum() > 0) {
    if (currentLEDStatus == LED_NO_CLIENT) {
      setLEDStatus(LED_CONNECTED);
    }
    // Check if intensity monitoring has timed out
    if (currentLEDStatus == LED_INTENSITY && 
        (millis() - lastIntensityRequest) > INTENSITY_TIMEOUT) {
      setLEDStatus(LED_CONNECTED);
    }
  } else {
    setLEDStatus(LED_NO_CLIENT);
  }
  
  // Read TSL2591 sensor (light intensity)
  // uint32_t lux = readTSL2591();
  uint32_t lux = readIR(); // Read IR instead of light for demonstration

    // catch serial commands and process them
  while (Serial.available())
  { // collect one line
    char c = Serial.read();
    if (c == '\n')
    {
      rxBuf.trim();

      if (rxBuf.startsWith("MEASURE"))
      { // MEASURE <freq>
        // Format: MEASURE 2500
        float f = rxBuf.substring(7).toFloat();
        if (f >= ADF_FREQ_MIN && f <= ADF_FREQ_MAX)
        {
          adf.updateFrequency(f * 1e6); // tune synthesiser
          uint32_t i = readIR();        // intensity
          float b = 123.0f;             // your magnetometer
          Serial.printf("DATA %.1f %lu %.1f\n", f, i, b);
        }
        else
        {
          Serial.println("ERR range");
        }
      }

      if (rxBuf == "LASER ON")
      {
        digitalWrite(LASER_PIN, HIGH);
      }
      if (rxBuf == "LASER OFF")
      {
        digitalWrite(LASER_PIN, LOW);
      }

      rxBuf = "";
    }
    else
    {
      rxBuf += c;
    }
  }
}
