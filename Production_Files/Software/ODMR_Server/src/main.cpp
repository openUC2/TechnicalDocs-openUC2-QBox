#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <Adafruit_TSL2591.h>  // Adafruit TSL2591 light sensor
#include <Adafruit_NeoPixel.h> // Neopixel-Bibliothek einbinden

#include "adf4351.h"

// Version info (auto-generated)
#include "version_info.h"

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
#define IS_WIFI_AP_MODE 1
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
const char *PASSWORD = ""; // Empty password for open network

// serial buffer for webserial comm.
String rxBuf;
const size_t MAX_SERIAL_BUFFER = 256; // Prevent buffer overflow

// Laser pin example
static const int LASER_PIN = 10;
bool laserState = false;

// canans ADF Library modified by bene
ADF4351 adf(clock, data, LE, CE);

// TSL2591 sensor
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// TSL2591 settings storage
tsl2591Gain_t currentGain = TSL2591_GAIN_MAX;
tsl2591IntegrationTime_t currentIntegrationTime = TSL2591_INTEGRATIONTIME_100MS;

// Cached sensor value for non-blocking reads (P0 #6)
volatile uint16_t cachedIR = 0;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 100; // Read sensor every 100ms

// WebServer on port 80
WebServer server(80);

// DNS Server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Neopixel settings
long firstPixelHue = 0;
int pixelWait = 1;

// LED status management
enum LEDStatus
{
  LED_NO_CLIENT, // white
  LED_CONNECTED, // rainbow
  LED_MEASURING, // red
  LED_INTENSITY  // blue
};

LEDStatus currentLEDStatus = LED_NO_CLIENT;
unsigned long lastLEDUpdate = 0;
const unsigned long LED_UPDATE_INTERVAL = 20; // Update every 20ms
unsigned long lastIntensityRequest = 0;
const unsigned long INTENSITY_TIMEOUT = 2000; // 2 seconds timeout

// LED control functions
void setLEDStatus(LEDStatus status)
{
  currentLEDStatus = status;
}

void updateLEDs()
{
  if (millis() - lastLEDUpdate < LED_UPDATE_INTERVAL)
    return;
  lastLEDUpdate = millis();

  strip.setBrightness(100); // Set to 50% brightness

  switch (currentLEDStatus)
  {
  case LED_NO_CLIENT:
    // White color for no client connected
    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, strip.Color(100, 100, 100)); // TODO: Make it glowing?
    }
    break;

  case LED_CONNECTED:
    // Rainbow effect when client connected but idle
    for (int i = 0; i < strip.numPixels(); i++)
    {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    firstPixelHue += 256;
    if (firstPixelHue > 5 * 65536)
      firstPixelHue = 0;
    break;

  case LED_MEASURING:
    // Red color when measuring frequency sweep
    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, strip.Color(100, 0, 0)); // TODO: Make it glowing?
    }
    break;

  case LED_INTENSITY:
    // Blue color when monitoring intensity for alignment
    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, strip.Color(0, 0, 100)); // TODO: Make it glowing?
    }
    break;
  }

  strip.show();
}

// Utility to serve static files from header files
// Determine MIME content type from file extension
String getContentType(const String &path)
{
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".ico"))  return "image/x-icon";
  if (path.endsWith(".json")) return "application/json";
  return "text/plain";
}

// Serve files from SPIFFS, fall back to index.html for SPA navigation
void handleFileRequest(const String &path)
{
  String actualPath = path;
  if (actualPath.endsWith("/"))
  {
    actualPath += "index.html";
  }
  // Ensure leading slash for SPIFFS paths
  if (!actualPath.startsWith("/"))
  {
    actualPath = "/" + actualPath;
  }

  // Try to serve from SPIFFS
  if (SPIFFS.exists(actualPath))
  {
    File file = SPIFFS.open(actualPath, "r");
    if (file)
    {
      String ct = getContentType(actualPath);
      server.streamFile(file, ct);
      file.close();
      return;
    }
  }

  // File not found — for HTML navigation requests fall back to index.html
  String accept = server.header("Accept");
  if (actualPath.endsWith(".html") || accept.indexOf("text/html") >= 0)
  {
    Serial.print("Unknown HTML path -> index: ");
    Serial.println(actualPath);
    if (SPIFFS.exists("/index.html"))
    {
      File idx = SPIFFS.open("/index.html", "r");
      server.streamFile(idx, "text/html");
      idx.close();
    }
    else
    {
      server.send(200, "text/html", "<h1>ODMR Server</h1><p>SPIFFS not mounted or index.html missing.</p>");
    }
  }
  else
  {
    Serial.print("404 Not Found: ");
    Serial.println(actualPath);
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
  Serial.println("Light: " + String(lux) + " lux");
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
  setLEDStatus(LED_MEASURING);              // Set LED to red while measuring
  adf.updateFrequency(freqRequested * 1e6); // Set frequency in Hz

  // Read intensity
  // uint32_t intensity = readTSL2591();
  uint32_t intensity = readIR(); // Read IR instead of light for demonstration

  // If you have a magnetometer or something, read that too
  float exampleMagVal = 123.0f; // Placeholder

  // Return space-separated: freq intensity magnetfield
  // or however your front-end expects it
  String reply = String(freqRequested, 1) + " " + intensity + " " + exampleMagVal;
  // Serial.println("Intensity: " + String(intensity) + " Mag: " + String(exampleMagVal));
  // Serial.println("Reply: " + reply);
  server.send(200, "text/plain", reply);

  // Return to connected status after measurement
  setLEDStatus(LED_CONNECTED);
}

// Live intensity reading for photodiode alignment - uses cached value (P0 #6)
void handleIntensity()
{
  // Set LED to blue for intensity monitoring mode and track timestamp
  setLEDStatus(LED_INTENSITY);
  lastIntensityRequest = millis();

  // Return cached value for instant response
  uint32_t intensity = cachedIR;
  String response = String("{\"intensity\":") + intensity + "}";
  server.send(200, "application/json", response);
}

// Get current TSL2591 settings
void handleGetTSLSettings()
{
  String response = "{\"gain\":";
  response += String((int)currentGain);
  response += ",\"integration_time\":";
  response += String((int)currentIntegrationTime);
  response += "}";
  server.send(200, "application/json", response);
}

// Set TSL2591 gain
void handleSetTSLGain()
{
  if (!server.hasArg("gain"))
  {
    server.send(400, "application/json", "{\"error\":\"no gain parameter\"}");
    return;
  }

  int gainValue = server.arg("gain").toInt();
  tsl2591Gain_t newGain;

  switch (gainValue)
  {
  case 0x00:
    newGain = TSL2591_GAIN_LOW;
    break;
  case 0x10:
    newGain = TSL2591_GAIN_MED;
    break;
  case 0x20:
    newGain = TSL2591_GAIN_HIGH;
    break;
  case 0x30:
    newGain = TSL2591_GAIN_MAX;
    break;
  default:
    server.send(400, "application/json", "{\"error\":\"invalid gain value\"}");
    return;
  }

  currentGain = newGain;
  tsl.setGain(currentGain);
  Serial.printf("TSL2591 Gain set to: 0x%02X\n", (int)currentGain);

  String response = "{\"status\":\"ok\",\"gain\":";
  response += String((int)currentGain);
  response += "}";
  server.send(200, "application/json", response);
}

// Set TSL2591 integration time
void handleSetTSLIntegrationTime()
{
  if (!server.hasArg("integration_time"))
  {
    server.send(400, "application/json", "{\"error\":\"no integration_time parameter\"}");
    return;
  }

  int timeValue = server.arg("integration_time").toInt();
  tsl2591IntegrationTime_t newTime;

  switch (timeValue)
  {
  case 0x00:
    newTime = TSL2591_INTEGRATIONTIME_100MS;
    break;
  case 0x01:
    newTime = TSL2591_INTEGRATIONTIME_200MS;
    break;
  case 0x02:
    newTime = TSL2591_INTEGRATIONTIME_300MS;
    break;
  case 0x03:
    newTime = TSL2591_INTEGRATIONTIME_400MS;
    break;
  case 0x04:
    newTime = TSL2591_INTEGRATIONTIME_500MS;
    break;
  case 0x05:
    newTime = TSL2591_INTEGRATIONTIME_600MS;
    break;
  default:
    server.send(400, "application/json", "{\"error\":\"invalid integration time value\"}");
    return;
  }

  currentIntegrationTime = newTime;
  tsl.setTiming(currentIntegrationTime);
  Serial.printf("TSL2591 Integration Time set to: 0x%02X\n", (int)currentIntegrationTime);

  String response = "{\"status\":\"ok\",\"integration_time\":";
  response += String((int)currentIntegrationTime);
  response += "}";
  server.send(200, "application/json", response);
}

// Check if WebSerial should be enabled (not on local AP interface)
void handleWebSerialCheck()
{
  // Check if request is coming from the local AP (192.168.4.x)
  IPAddress clientIP = server.client().remoteIP();
  bool isLocalAP = (clientIP[0] == 192 && clientIP[1] == 168 && clientIP[2] == 4);

  String response = String("{\"webserial_enabled\":") + (!isLocalAP ? "true" : "false") + "}";
  server.send(200, "application/json", response);
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

// Messen der IR-Intensität bei einer gegebenen Frequenz (in MHz)
uint32_t measureIntensityAtFrequency(float freqMHz,
                                     uint8_t averages = 3,
                                     uint16_t settle_ms = 10)
{
  if (freqMHz < ADF_FREQ_MIN || freqMHz > ADF_FREQ_MAX)
  {
    Serial.printf("ERR measureIntensityAtFrequency: %.3f MHz out of range\n", freqMHz);
    return 0;
  }

  adf.updateFrequency(freqMHz * 1e6); // MHz -> Hz
  delay(settle_ms);                   // PLL settle

  uint64_t sum = 0;
  for (uint8_t i = 0; i < averages; ++i)
  {
    sum += readIR();
    delay(1);
  }
  return (uint32_t)(sum / averages);
}

// /ratio?f1=2865&f2=2875[&f3=2855][&avg=5]
// Liefert Intensitäten und normierte Ratios als JSON
void handleMeasureRatio()
{
  if (!server.hasArg("f1") || !server.hasArg("f2"))
  {
    server.send(400, "application/json",
                "{\"error\":\"need f1 and f2 in MHz\"}");
    return;
  }

  float f1 = server.arg("f1").toFloat();
  float f2 = server.arg("f2").toFloat();

  bool hasF3 = server.hasArg("f3");
  float f3 = hasF3 ? server.arg("f3").toFloat() : 0.0f;

  uint8_t averages = 3;
  if (server.hasArg("avg"))
  {
    int tmp = server.arg("avg").toInt();
    if (tmp < 1)
      tmp = 1;
    if (tmp > 20)
      tmp = 20;
    averages = (uint8_t)tmp;
  }

  setLEDStatus(LED_MEASURING);

  uint32_t I1 = measureIntensityAtFrequency(f1, averages);
  uint32_t I2 = measureIntensityAtFrequency(f2, averages);
  uint32_t I3 = 0;
  Serial.printf("Measured intensities: I1= %u @ %.3f MHz, I2= %u @ %.3f MHz",
                I1, f1, I2, f2);
  if (hasF3)
  {
    I3 = measureIntensityAtFrequency(f3, averages);
  }

  float r12 = 0.0f;
  if (I1 + I2 > 0)
  {
    r12 = ((float)I1 - (float)I2) / ((float)I1 + (float)I2);
  }
  Serial.printf(", I3= %u @ %.3f MHz\n", I3, f3);

  float r13 = 0.0f;
  float r23 = 0.0f;
  if (hasF3)
  {
    if (I1 + I3 > 0)
      r13 = ((float)I1 - (float)I3) / ((float)I1 + (float)I3);
    if (I2 + I3 > 0)
      r23 = ((float)I2 - (float)I3) / ((float)I2 + (float)I3);
  }
  Serial.printf("Ratios: r12= %.6f", r12);
  if (hasF3)
  {
    Serial.printf(", r13= %.6f, r23= %.6f", r13, r23);
  }
  Serial.println();

  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"avg\":" + String(averages) + ",";
  json += "\"points\":[";
  json += "{\"f\":" + String(f1, 3) + ",\"I\":" + String(I1) + "},";
  json += "{\"f\":" + String(f2, 3) + ",\"I\":" + String(I2) + "}";
  if (hasF3)
  {
    json += ",{\"f\":" + String(f3, 3) + ",\"I\":" + String(I3) + "}";
  }
  json += "],";
  json += "\"ratio\":{";
  json += "\"type\":\"diff_over_sum\",";
  json += "\"r12\":" + String(r12, 6);
  if (hasF3)
  {
    json += ",\"r13\":" + String(r13, 6);
    json += ",\"r23\":" + String(r23, 6);
  }
  json += "}";
  json += "}";
  Serial.println("JSON response: " + json);

  server.send(200, "application/json", json);

  setLEDStatus(LED_CONNECTED);
}

void setup()
{

// only for esp32s3
#ifdef ESP32S3
  // disableCore1WDT(); // Deactivate Watchdog for core 1
#endif

  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);

  Serial.begin(115200);
  delay(1500); // Allow time to connect
  Serial.println("Booting...");

  disableLoopWDT(); // Deactivate Watchdog for loop
  // Check WiFi capabilities
  Serial.print("WiFi Mode capabilities: ");
  Serial.println(WiFi.getMode());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Wi-Fi
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String macID = String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  macID.toUpperCase();
  String dynamicSSID = "openUC2_ODMR_" + macID;
  Serial.print("SSID: ");
  Serial.println(dynamicSSID);

  // choose random channel between 1 and 11
  randomSeed(micros());
  int wifiChannel = random(1, 12);
  Serial.print("Using WiFi channel: ");
  Serial.println(wifiChannel);
  // Note: channel is passed to WiFi.softAP() directly (P1 #7)
  // Ensure WiFi is completely disconnected and reset
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000);

  Serial.println("Starting WiFi Access Point...");
  if (IS_WIFI_AP_MODE)
  {
    // Wi-Fi Access Point starting
    WiFi.mode(WIFI_AP);

    // Try setting WiFi configuration first
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),  // IP address of the AP
        IPAddress(192, 168, 4, 1),  // Gateway
        IPAddress(255, 255, 255, 0) // Subnet mask
    );

    // Start the Access Point with channel parameter (P1 #7)
    bool apResult = WiFi.softAP(dynamicSSID.c_str(), PASSWORD, wifiChannel, 0, 4);

    if (apResult)
    {
      Serial.print("Access Point started successfully with SSID: ");
      Serial.println(dynamicSSID);
      Serial.print("AP IP address: ");
      Serial.println(WiFi.softAPIP());
      Serial.print("AP WiFi channel: ");
      Serial.println(wifiChannel);
    }
    else
    {
      Serial.println("Failed to start Access Point!");
      Serial.println("Trying alternative configuration with channel 1...");

      // Try with channel 1 as fallback
      apResult = WiFi.softAP(dynamicSSID.c_str(), PASSWORD, 1, 0, 4);
      if (apResult)
      {
        Serial.println("Access Point started with alternative config");
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
      }
      else
      {
        Serial.println("Access Point startup failed completely!");
      }
    }

    // Start DNS server for captive portal
    // This will redirect all DNS requests to our IP (192.168.4.1)
    dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    Serial.println("DNS Server started for captive portal");
  }
  else
  {
    // use ssid/password mode instead  (SSID: openUC2; Password: Wifi So You Can See Too)
    // connect to the available network
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to WiFi network...");
    WiFi.begin("openUC2", "Wifi So You Can See Too");
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // Mount SPIFFS filesystem (holds all web assets)
  if (!SPIFFS.begin(true))
  {
    Serial.println("ERROR: SPIFFS mount failed!");
  }
  else
  {
    Serial.println("SPIFFS mounted successfully");
    // List files for debug
    File root = SPIFFS.open("/");
    File f = root.openNextFile();
    while (f)
    {
      Serial.printf("  SPIFFS: %-30s %6d bytes\n", f.name(), f.size());
      f = root.openNextFile();
    }
  }

  // I2C initialization for TSL2591
  Wire.begin(SDA_PIN, SCL_PIN);

  // Neopixel initialisieren
  strip.begin();
  strip.show();             // Alle LEDs ausschalten
  strip.setBrightness(100); // Helligkeit auf 50% einstellen (0-255)

  // perform I2C scan to verify TSL2591 is connected
  i2c_scan();

  // TSL2591 init
  if (!tsl.begin())
  {
    Serial.println("TSL2591 not found");
  }
  else
  {
    tsl.setGain(currentGain);
    tsl.setTiming(currentIntegrationTime);
    // turn off led on TSL2591
    tsl.enableAutoRange(true);
    Serial.println("TSL2591 initialized");
    Serial.printf("TSL2591 Gain: 0x%02X, Integration Time: 0x%02X\n", (int)currentGain, (int)currentIntegrationTime);
  }

  // ADF4351 init
  Serial.println("ADF4351 init");
  adf.begin();
  // adf.updateFrequency(1800.0f); // Some initial frequency (example)
  // adf.updateFrequency(2.2e9); // 2.2 GHz // 1.800 GHz ─ writes R5…R0

  // Setup routes
  // Root handler — serve index.html from SPIFFS
  server.on("/", HTTP_GET, []()
            { handleFileRequest("/index.html"); });

  // Captive portal detection endpoints - return "success" responses (P0 #1)
  // This prevents OS from thinking it's a captive portal and keeps devices connected

  // Android captive portal detection - expects 204
  server.on("/generate_204", HTTP_GET, []()
            { server.send(204, "text/plain", ""); });

  // Microsoft Windows captive portal detection
  server.on("/connecttest.txt", HTTP_GET, []()
            { server.send(200, "text/plain", "Microsoft Connect Test"); });
  server.on("/ncsi.txt", HTTP_GET, []()
            { server.send(200, "text/plain", "Microsoft NCSI"); });

  // Apple iOS/MacOS captive portal detection
  server.on("/hotspot-detect.html", HTTP_GET, []()
            { server.send(200, "text/html", "Success"); });
  server.on("/library/test/success.html", HTTP_GET, []()
            { server.send(200, "text/html", "Success"); });

  // Additional OS probe endpoints
  server.on("/success.txt", HTTP_GET, []()
            { server.send(200, "text/plain", "success"); });
  server.on("/canonical.html", HTTP_GET, []()
            { server.send(200, "text/html", "Success"); });

  // Favicon: return 204 to stop browsers retrying (P0 #3)
  server.on("/favicon.ico", HTTP_GET, []()
            { server.send(204, "image/x-icon", ""); });

  server.onNotFound([]()
                    { handleFileRequest(server.uri()); });
  server.on("/odmr_act", HTTP_POST, handleOdmrAct);
  server.on("/measure", HTTP_GET, handleMeasure);
  server.on("/intensity", HTTP_GET, handleIntensity);
  server.on("/webserial_check", HTTP_GET, handleWebSerialCheck);
  server.on("/tsl/settings", HTTP_GET, handleGetTSLSettings);
  server.on("/tsl/gain", HTTP_POST, handleSetTSLGain);
  server.on("/tsl/integration_time", HTTP_POST, handleSetTSLIntegrationTime);
  server.on("/ratio", HTTP_GET, handleMeasureRatio);

  // ADF4351 enable/disable endpoints (P1 #9)
  server.on("/ADF_Enable", HTTP_POST, []()
            {
    adf.begin();
    server.send(200, "application/json", "{\"status\":\"ok\",\"adf\":\"enabled\"}"); });
  server.on("/ADF_Disable", HTTP_POST, []()
            {
    adf.stop();
    server.send(200, "application/json", "{\"status\":\"ok\",\"adf\":\"disabled\"}"); });

  // Version endpoint (P1 #10) - returns firmware version as JSON
  server.on("/version", HTTP_GET, []()
            {
    String json = "{";
    json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"build_date\":\"" + String(BUILD_DATE) + "\",";
    json += "\"build_time\":\"" + String(BUILD_TIME) + "\",";
    json += "\"git_hash\":\"" + String(GIT_HASH) + "\",";
    json += "\"git_branch\":\"" + String(GIT_BRANCH) + "\"";
    json += "}";
    server.send(200, "application/json", json); });

  // Collect Accept header for 404 logic
  const char* headerKeys[] = {"Accept"};
  server.collectHeaders(headerKeys, 1);

  server.begin();
  adf.stop(); // Disable output initially
}

void loop()
{
  // Process DNS requests for captive portal
  dnsServer.processNextRequest();

  server.handleClient();

  // Periodic sensor read for cached value (P0 #6)
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL)
  {
    lastSensorRead = millis();
    cachedIR = readIR();
  }

  // Update LED status indicators
  updateLEDs();

  // Check if any clients are connected to determine LED status
  if (WiFi.softAPgetStationNum() > 0)
  {
    if (currentLEDStatus == LED_NO_CLIENT)
    {
      setLEDStatus(LED_CONNECTED);
    }
    // Check if intensity monitoring has timed out
    if (currentLEDStatus == LED_INTENSITY &&
        (millis() - lastIntensityRequest) > INTENSITY_TIMEOUT)
    {
      setLEDStatus(LED_CONNECTED);
    }
  }
  else
  {
    setLEDStatus(LED_NO_CLIENT);
  }

  // Read TSL2591 sensor (light intensity)
  // uint32_t lux = readTSL2591();
  // uint32_t lux = readIR(); // Read IR instead of light for demonstration

  // catch serial commands and process them
  // Format should be:
  // COMMAND PARAMETER
  // e.g. MEASURE 2500
  while (Serial.available())
  { // collect one line
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (rxBuf.length() > 0)
      {
        rxBuf.trim();

        if (rxBuf.startsWith("MEASURE"))
        { // MEASURE <freq>
          // Format: MEASURE 2500
          float f = rxBuf.substring(7).toFloat();
          if (f >= ADF_FREQ_MIN && f <= ADF_FREQ_MAX)
          {
            setLEDStatus(LED_MEASURING);  // Set LED to red
            adf.updateFrequency(f * 1e6); // tune synthesiser
            delay(10);                    // Allow settling time
            uint32_t i = readIR();        // intensity
            Serial.printf("DATA %.1f %lu\n", f, i);
            setLEDStatus(LED_CONNECTED); // Return to connected state
            adf.stop();                  // Disable output after measurement
          }
          else
          {
            setLEDStatus(LED_MEASURING); // Set LED to red
            uint32_t i = readIR();       // intensity
            Serial.printf("DATA %.1f %lu\n", f, i);
            setLEDStatus(LED_CONNECTED); // Return to connected state

            // Serial.printf("ERR range: %.1f not in [%.1f, %.1f] MHz\n", f, ADF_FREQ_MIN, ADF_FREQ_MAX);
          }
        }
        else if (rxBuf == "STATUS")
        {
          // Add status command for debugging
          Serial.printf("STATUS connected:%d freq_range:[%.1f,%.1f] led:%d\n",
                        WiFi.softAPgetStationNum(), ADF_FREQ_MIN, ADF_FREQ_MAX, (int)currentLEDStatus);
        }
        else if (rxBuf.length() > 0)
        {
          Serial.printf("ERR unknown command: %s\n", rxBuf.c_str());
        }
        else if (rxBuf.startsWith("RATIO "))
        {
          // Format: RATIO f1 f2
          int sp1 = rxBuf.indexOf(' ');
          int sp2 = rxBuf.indexOf(' ', sp1 + 1);
          if (sp2 > sp1)
          {
            float f1 = rxBuf.substring(sp1 + 1, sp2).toFloat();
            float f2 = rxBuf.substring(sp2 + 1).toFloat();

            setLEDStatus(LED_MEASURING);
            uint32_t I1 = measureIntensityAtFrequency(f1);
            uint32_t I2 = measureIntensityAtFrequency(f2);
            float r12 = (I1 + I2 > 0)
                            ? ((float)I1 - (float)I2) / ((float)I1 + (float)I2)
                            : 0.0f;
            setLEDStatus(LED_CONNECTED);

            Serial.printf("RATIO %.3f %.3f %lu %lu %.6f\n",
                          f1, f2, I1, I2, r12);
          }
          else
          {
            Serial.println("ERR RATIO syntax");
          }
        }
      }
      rxBuf = "";
    }
    else if (c >= 32 && c <= 126) // Only accept printable characters
    {
      if (rxBuf.length() < MAX_SERIAL_BUFFER)
      {
        rxBuf += c;
      }
      else
      {
        // Buffer overflow protection
        Serial.println("ERR buffer overflow");
        rxBuf = "";
      }
    }
    // Ignore other characters (control characters, etc.)
  }
}
