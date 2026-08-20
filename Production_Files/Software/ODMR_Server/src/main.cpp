#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <Adafruit_TSL2591.h>  // Adafruit TSL2591 light sensor
#include <Adafruit_NeoPixel.h> // Neopixel-Bibliothek einbinden



#include "adf4351.h"

// Version info (auto-generated)
#include "version_info.h"

// Web assets are served from SPIFFS (data/, gzipped by scripts/gzip_assets.py)
// via server.serveStatic() — no longer embedded in PROGMEM. Run `pio run -t
// uploadfs` after flashing to upload them.

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

// Timestamp of last serial command, used so the status LEDs react to a
// serial-only host (no WiFi client) the same way they react to a web client.
unsigned long lastSerialActivity = 0;
const unsigned long SERIAL_ACTIVITY_TIMEOUT = 5000; // 5 s "connected" window

// Forward declarations for the serial command interface (mirrors the HTTP API
// so the device can be driven entirely over USB-CDC serial, no WiFi required).
void initialize_tsl(); // defined later, but used by readTSL2591() above it
bool applyTSLGain(int gainValue);
bool applyTSLIntegration(int timeValue);
void processSerialLine(String line);
void serialSweep(float fBegin, float fEnd, float fStep, uint8_t averages, uint16_t settle_ms);
int pickBestChannel();

// Laser pin example
static const int LASER_PIN = 10;
bool laserState = false;

// canans ADF Library modified by bene
ADF4351 adf(clock, data, LE, CE);

// TSL2591 sensor
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// TSL2591 settings storage
tsl2591Gain_t currentGain = TSL2591_GAIN_HIGH;
tsl2591IntegrationTime_t currentIntegrationTime = TSL2591_INTEGRATIONTIME_100MS;

// Cached sensor value for non-blocking reads (P0 #6)
volatile uint16_t cachedIR = 0;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 100; // Read sensor every 100ms

// Sweep data buffer for broken-connection recovery
// Stores the last sweep results so they can be retrieved via /sweep_buffer
struct SweepDataPoint {
  float frequency;
  uint32_t intensity;
};
static const int MAX_SWEEP_BUFFER = 600;
SweepDataPoint sweepBuffer[MAX_SWEEP_BUFFER];
int sweepBufferCount = 0;
bool sweepInProgress = false;
volatile bool sweepStopRequested = false;

// Async web server on port 80 + Server-Sent-Events channel for the sweep.
AsyncWebServer server(80);
AsyncEventSource sweepSource("/sweep");

// DNS Server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

// --- Async request parameter helpers -------------------------------------
// The old synchronous WebServer exposed hasArg()/arg() that transparently read
// either the query string or a form-urlencoded POST body. These mirror that so
// the handlers below convert almost mechanically. (The frontend passes all
// parameters in the query string, even on POSTs, but we check both to be safe.)
static inline bool reqHas(AsyncWebServerRequest *r, const char *key)
{
  return r->hasParam(key) || r->hasParam(key, true);
}
static inline String reqArg(AsyncWebServerRequest *r, const char *key)
{
  if (r->hasParam(key))       return r->getParam(key)->value();
  if (r->hasParam(key, true)) return r->getParam(key, true)->value();
  return String();
}

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
bool tsl_is_initialized = false;
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

  // Breathing brightness: sinusoidal pulse between ~20 and 100 over ~3 seconds
  float breathPhase = (float)(millis() % 3000) / 3000.0f; // 0..1 over 3 s
  float breathVal   = (sin(breathPhase * 2.0f * PI) + 1.0f) / 2.0f; // 0..1
  uint8_t breathBri = (uint8_t)(20 + breathVal * 80); // range 20..100

  switch (currentLEDStatus)
  {
  case LED_NO_CLIENT:
    // White breathing for no client connected
    strip.setBrightness(breathBri);
    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, strip.Color(100, 100, 100));
    }
    break;

  case LED_CONNECTED:
    // Rainbow effect when client connected but idle
    strip.setBrightness(100);
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
    // Red breathing when measuring frequency sweep
    strip.setBrightness(breathBri);
    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, strip.Color(100, 0, 0));
    }
    break;

  case LED_INTENSITY:
    // Blue breathing when monitoring intensity for alignment
    strip.setBrightness(breathBri);
    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, strip.Color(0, 0, 100));
    }
    break;
  }

  strip.show();
}

// Small built-in page shown only when SPIFFS has no index.html — i.e. the
// firmware was flashed but the filesystem image was never uploaded. Prevents a
// confusing wall of 404s and tells the user exactly what to do.
static const char FS_MISSING_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>openUC2 ODMR</title><style>body{font-family:sans-serif;max-width:640px;"
    "margin:3rem auto;padding:0 1rem;color:#023773}code{background:#eef;padding:.15rem "
    ".4rem;border-radius:4px}</style></head><body><h1>&#128300; openUC2 ODMR</h1>"
    "<p>The firmware is running, but the web interface files are not on the device "
    "filesystem yet.</p><p>Upload them once with:</p>"
    "<pre><code>pio run -t uploadfs</code></pre>"
    "<p>The device can still be controlled over USB serial in the meantime.</p>"
    "</body></html>";

// True once we've confirmed SPIFFS actually contains the web UI.
bool webAssetsPresent = false;

// Example: read TSL2591 sensor (light intensity)
uint32_t readTSL2591()
{
  // reinitialize the light sensor 
  if (not tsl_is_initialized){
    if (!tsl.begin())
      {
        tsl_is_initialized = false;
        Serial.println("TSL2591 not found");
      }
      else
      {
        tsl_is_initialized = true;
        initialize_tsl();
      }
  }
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

// (Legacy /laser_act and /odmr_act demo handlers removed — the laser control
// was dropped and neither endpoint is used by the web UI or serial interface.)

// Example /measure?frequenz=xxxx
// We set the frequency on the ADF4351 and read the TSL2591 sensor
// Return "freq intensity magnetfield" style data (just placeholders here)
void handleMeasure(AsyncWebServerRequest *request)
{
  if (!reqHas(request, "frequenz"))
  {
    Serial.println("No frequency set");
    request->send(400, "text/plain", "Error: no 'frequenz' param");
    return;
  }
  float freqRequested = reqArg(request, "frequenz").toFloat();
  // Make sure it's within ADF4351 range
  if (freqRequested < ADF_FREQ_MIN || freqRequested > ADF_FREQ_MAX)
  {
    Serial.println("Freq out of range");
    request->send(400, "text/plain", "Freq out of range");
    return;
  }

  // Set the frequency on the ADF4351
  Serial.println("Setting frequency: " + String(freqRequested, 1));
  setLEDStatus(LED_MEASURING);              // Set LED to red while measuring
  adf.updateFrequency(freqRequested * 1e6); // Set frequency in Hz

  // Read intensity
  uint32_t intensity = readIR(); // Read IR instead of light for demonstration

  // If you have a magnetometer or something, read that too
  float exampleMagVal = 123.0f; // Placeholder

  // Return space-separated: freq intensity magnetfield
  String reply = String(freqRequested, 1) + " " + intensity + " " + exampleMagVal;
  request->send(200, "text/plain", reply);

  // Return to connected status after measurement
  setLEDStatus(LED_CONNECTED);
}

// Live intensity reading for photodiode alignment - uses cached value (P0 #6)
void handleIntensity(AsyncWebServerRequest *request)
{
  // Set LED to blue for intensity monitoring mode and track timestamp
  setLEDStatus(LED_INTENSITY);
  lastIntensityRequest = millis();

  // Return cached value for instant response
  uint32_t intensity = cachedIR;
  String response = String("{\"intensity\":") + intensity + "}";
  request->send(200, "application/json", response);
}

// ---------------------------------------------------------------------------
// Shared TSL2591 setting helpers (used by both the HTTP handlers and the serial
// command interface). Accept the same encoding the web UI sends: gain as
// 0x00/0x10/0x20/0x30 and integration time as 0x00..0x05. Return false on an
// invalid value so callers can report an error.
// ---------------------------------------------------------------------------
bool applyTSLGain(int gainValue)
{
  tsl2591Gain_t newGain;
  switch (gainValue)
  {
  case 0x00: newGain = TSL2591_GAIN_LOW;  break;
  case 0x10: newGain = TSL2591_GAIN_MED;  break;
  case 0x20: newGain = TSL2591_GAIN_HIGH; break;
  case 0x30: newGain = TSL2591_GAIN_MAX;  break;
  default: return false;
  }
  currentGain = newGain;
  tsl.setGain(currentGain);
  Serial.printf("TSL2591 Gain set to: 0x%02X\n", (int)currentGain);
  return true;
}

bool applyTSLIntegration(int timeValue)
{
  tsl2591IntegrationTime_t newTime;
  switch (timeValue)
  {
  case 0x00: newTime = TSL2591_INTEGRATIONTIME_100MS; break;
  case 0x01: newTime = TSL2591_INTEGRATIONTIME_200MS; break;
  case 0x02: newTime = TSL2591_INTEGRATIONTIME_300MS; break;
  case 0x03: newTime = TSL2591_INTEGRATIONTIME_400MS; break;
  case 0x04: newTime = TSL2591_INTEGRATIONTIME_500MS; break;
  case 0x05: newTime = TSL2591_INTEGRATIONTIME_600MS; break;
  default: return false;
  }
  currentIntegrationTime = newTime;
  tsl.setTiming(currentIntegrationTime);
  Serial.printf("TSL2591 Integration Time set to: 0x%02X\n", (int)currentIntegrationTime);
  return true;
}

// Get current TSL2591 settings
void handleGetTSLSettings(AsyncWebServerRequest *request)
{
  String response = "{\"gain\":";
  response += String((int)currentGain);
  response += ",\"integration_time\":";
  response += String((int)currentIntegrationTime);
  response += "}";
  request->send(200, "application/json", response);
}

// Set TSL2591 gain
void handleSetTSLGain(AsyncWebServerRequest *request)
{
  if (!reqHas(request, "gain"))
  {
    request->send(400, "application/json", "{\"error\":\"no gain parameter\"}");
    return;
  }

  int gainValue = reqArg(request, "gain").toInt();
  if (!applyTSLGain(gainValue))
  {
    request->send(400, "application/json", "{\"error\":\"invalid gain value\"}");
    return;
  }

  String response = "{\"status\":\"ok\",\"gain\":";
  response += String((int)currentGain);
  response += "}";
  request->send(200, "application/json", response);
}

// Set TSL2591 integration time
void handleSetTSLIntegrationTime(AsyncWebServerRequest *request)
{
  if (!reqHas(request, "integration_time"))
  {
    request->send(400, "application/json", "{\"error\":\"no integration_time parameter\"}");
    return;
  }

  int timeValue = reqArg(request, "integration_time").toInt();
  if (!applyTSLIntegration(timeValue))
  {
    request->send(400, "application/json", "{\"error\":\"invalid integration time value\"}");
    return;
  }

  String response = "{\"status\":\"ok\",\"integration_time\":";
  response += String((int)currentIntegrationTime);
  response += "}";
  request->send(200, "application/json", response);
}

void initialize_tsl(){ 
    tsl.setGain(currentGain);
    tsl.setTiming(currentIntegrationTime);
    // turn off led on TSL2591
    tsl.enableAutoRange(true);
    Serial.println("TSL2591 initialized");
    Serial.printf("TSL2591 Gain: 0x%02X, Integration Time: 0x%02X\n", (int)currentGain, (int)currentIntegrationTime);
}

// Check if WebSerial should be enabled (not on local AP interface)
void handleWebSerialCheck(AsyncWebServerRequest *request)
{
  // Check if request is coming from the local AP (192.168.4.x)
  IPAddress clientIP = request->client()->remoteIP();
  bool isLocalAP = (clientIP[0] == 192 && clientIP[1] == 168 && clientIP[2] == 4);

  String response = String("{\"webserial_enabled\":") + (!isLocalAP ? "true" : "false") + "}";
  request->send(200, "application/json", response);
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
                                     uint8_t averages = 2,
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
  uint32_t intensity = (uint32_t)(sum / averages);
  Serial.println(String("Measured intensity at ") + String(freqMHz, 3) + " MHz: " + intensity);
  return intensity;
}

// /ratio?f1=2865&f2=2875[&f3=2855][&avg=5]
// Liefert Intensitäten und normierte Ratios als JSON
void handleMeasureRatio(AsyncWebServerRequest *request)
{
  if (!reqHas(request, "f1") || !reqHas(request, "f2"))
  {
    request->send(400, "application/json",
                "{\"error\":\"need f1 and f2 in MHz\"}");
    return;
  }

  float f1 = reqArg(request, "f1").toFloat();
  float f2 = reqArg(request, "f2").toFloat();

  bool hasF3 = reqHas(request, "f3");
  float f3 = hasF3 ? reqArg(request, "f3").toFloat() : 0.0f;

  uint8_t averages = 3;
  if (reqHas(request, "avg"))
  {
    int tmp = reqArg(request, "avg").toInt();
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

  request->send(200, "application/json", json);

  setLEDStatus(LED_CONNECTED);
}

// ===========================================================================
// Streaming frequency sweep over Server-Sent Events (async).
//
// The old implementation blocked loop() for the whole sweep. This version is
// non-blocking: the browser opens `new EventSource('/sweep?f_begin=..&f_end=..
// &f_step=..[&avg=..][&settle=..]')`, which the AsyncEventSource `sweepSource`
// accepts. Parameters are parsed in authorizeConnect() (the only SSE callback
// with access to the request); the actual measuring runs one point at a time
// in serviceHttpSweep() from loop(), and each point is pushed to the client
// with sweepSource.send(). This keeps ALL SPI/I2C hardware access on the main
// loop (never the AsyncTCP task) and lets other requests be served meanwhile.
//
// The frontend contract is unchanged: `onmessage` receives
//   {"f":..,"I":..,"idx":..,"total":..}  per point, then  {"done":true}.
// ===========================================================================
struct HttpSweepState
{
  volatile bool active = false;   // measuring right now (driven by loop())
  volatile bool pending = false;  // params accepted, waiting for SSE onConnect
  volatile bool needsInit = false;// loop() must do the SPI/LED setup on start
  uint32_t pendingSince = 0;      // for a safety timeout on 'pending'
  float fBegin = 0, fEnd = 0, fStep = 1;
  uint8_t averages = 1;
  uint16_t settle_ms = 10;
  int totalPoints = 0;
  int pointIndex = 0;
  bool waitingSettle = false;     // PLL is settling before we read this point
  uint32_t settleUntil = 0;       // millis() deadline for the settle wait
};
HttpSweepState httpSweep;

// Finish/clean up a sweep. Always called from loop() context so it is safe to
// touch the ADF (SPI) here.
static void finishHttpSweep(bool sendDone)
{
  if (sendDone && sweepSource.count() > 0)
    sweepSource.send("{\"done\":true}"); // default event -> browser onmessage
  adf.stop();
  httpSweep.active = false;
  httpSweep.pending = false;
  httpSweep.waitingSettle = false;
  sweepInProgress = false;
  sweepStopRequested = false;
  setLEDStatus(LED_CONNECTED);
  Serial.printf("Sweep complete: %d point(s)\n", httpSweep.pointIndex);
}

// Step the non-blocking sweep. Call once per loop() iteration.
void serviceHttpSweep()
{
  // Safety: if params were accepted but no SSE client connected shortly after,
  // clear the pending state so a future sweep isn't blocked.
  if (httpSweep.pending && !httpSweep.active &&
      (millis() - httpSweep.pendingSince > 3000))
  {
    httpSweep.pending = false;
    Serial.println("Sweep: pending connection timed out");
  }

  if (!httpSweep.active)
    return;

  // First iteration after the SSE client connected: do the hardware setup here
  // in loop() context (never from the AsyncTCP callback — SPI/I2C must stay on
  // the main thread to avoid racing the sensor reads).
  if (httpSweep.needsInit)
  {
    sweepBufferCount = 0;
    httpSweep.pointIndex = 0;
    httpSweep.waitingSettle = false;
    sweepStopRequested = false;
    sweepInProgress = true;
    setLEDStatus(LED_MEASURING);
    adf.begin();
    httpSweep.needsInit = false;
  }

  // Stop conditions: explicit stop, or the browser closed the stream.
  if (sweepStopRequested || sweepSource.count() == 0)
  {
    Serial.println(sweepStopRequested ? "Sweep: stop requested"
                                      : "Sweep: client disconnected, stopping");
    finishHttpSweep(false);
    return;
  }

  float curF = httpSweep.fBegin + (float)httpSweep.pointIndex * httpSweep.fStep;

  if (!httpSweep.waitingSettle)
  {
    // Begin this point: set frequency and start the (non-blocking) settle wait.
    adf.updateFrequency(curF * 1e6); // MHz -> Hz
    httpSweep.settleUntil = millis() + httpSweep.settle_ms;
    httpSweep.waitingSettle = true;
    return;
  }

  if ((int32_t)(millis() - httpSweep.settleUntil) < 0)
    return; // still settling

  // Settle done — take the averaged reading (a few ms, fine in loop()).
  uint64_t sum = 0;
  for (uint8_t i = 0; i < httpSweep.averages; i++)
  {
    sum += readIR();
    if (httpSweep.averages > 1)
      delay(1);
  }
  uint32_t intensity = (uint32_t)(sum / httpSweep.averages);

  if (sweepBufferCount < MAX_SWEEP_BUFFER)
  {
    sweepBuffer[sweepBufferCount].frequency = curF;
    sweepBuffer[sweepBufferCount].intensity = intensity;
    sweepBufferCount++;
  }

  String msg = "{\"f\":" + String(curF, 1) + ",\"I\":" + String(intensity) +
               ",\"idx\":" + String(httpSweep.pointIndex) +
               ",\"total\":" + String(httpSweep.totalPoints) + "}";
  sweepSource.send(msg.c_str()); // default event -> browser onmessage

  httpSweep.pointIndex++;
  httpSweep.waitingSettle = false;

  if (httpSweep.pointIndex >= httpSweep.totalPoints)
    finishHttpSweep(true); // all points sent -> {"done":true}
}

// Configure the SSE endpoint. Called once from setup().
void setupSweepSource()
{
  // authorizeConnect is the only SSE hook that receives the request, so we
  // parse + validate the sweep parameters here. Returning false rejects the
  // EventSource connection (bad params, or a sweep already running).
  sweepSource.authorizeConnect([](AsyncWebServerRequest *request) -> bool {
    if (httpSweep.active || httpSweep.pending)
      return false; // one sweep at a time
    if (!request->hasParam("f_begin") || !request->hasParam("f_end") ||
        !request->hasParam("f_step"))
      return false;

    float fBegin = request->getParam("f_begin")->value().toFloat();
    float fEnd = request->getParam("f_end")->value().toFloat();
    float fStep = request->getParam("f_step")->value().toFloat();
    uint8_t averages = 1;
    if (request->hasParam("avg"))
      averages = (uint8_t)constrain(request->getParam("avg")->value().toInt(), 1, 20);
    uint16_t settle_ms = 10;
    if (request->hasParam("settle"))
      settle_ms = (uint16_t)constrain(request->getParam("settle")->value().toInt(), 1, 200);

    if (fBegin < ADF_FREQ_MIN || fEnd > ADF_FREQ_MAX || fStep <= 0 || fBegin >= fEnd)
      return false;
    int totalPoints = (int)((fEnd - fBegin) / fStep) + 1;
    if (totalPoints < 1 || totalPoints > MAX_SWEEP_BUFFER)
      return false;

    httpSweep.fBegin = fBegin;
    httpSweep.fEnd = fEnd;
    httpSweep.fStep = fStep;
    httpSweep.averages = averages;
    httpSweep.settle_ms = settle_ms;
    httpSweep.totalPoints = totalPoints;
    httpSweep.pending = true;
    httpSweep.pendingSince = millis();
    Serial.printf("Sweep accepted: %.1f -> %.1f MHz, step %.1f, avg %d, settle %d ms, %d pts\n",
                  fBegin, fEnd, fStep, averages, settle_ms, totalPoints);
    return true;
  });

  // Client connected: just arm the state machine. The actual SPI/LED setup and
  // measuring happen in serviceHttpSweep() on the main loop — this callback
  // runs on the AsyncTCP task and must not touch the ADF/TSL hardware. Set
  // needsInit before active so loop() always sees the init flag once active.
  sweepSource.onConnect([](AsyncEventSourceClient *client) {
    if (!httpSweep.pending)
      return;
    httpSweep.pending = false;
    httpSweep.needsInit = true;
    httpSweep.active = true; // serviceHttpSweep() takes over from here
  });

  // Client closed the stream: request a stop; loop() does the actual cleanup.
  sweepSource.onDisconnect([](AsyncEventSourceClient *client) {
    if (httpSweep.active)
      sweepStopRequested = true;
  });

  server.addHandler(&sweepSource);
}

// Endpoint to retrieve buffered sweep data (for broken-connection recovery)
void handleSweepBuffer(AsyncWebServerRequest *request)
{
  String json = "{\"in_progress\":";
  json += sweepInProgress ? "true" : "false";
  json += ",\"count\":";
  json += String(sweepBufferCount);
  json += ",\"data\":[";
  for (int i = 0; i < sweepBufferCount; i++)
  {
    if (i > 0) json += ",";
    json += "{\"f\":";
    json += String(sweepBuffer[i].frequency, 1);
    json += ",\"I\":";
    json += String(sweepBuffer[i].intensity);
    json += "}";
  }
  json += "]}";
  request->send(200, "application/json", json);
}

// Endpoint to explicitly stop a running sweep
void handleSweepStop(AsyncWebServerRequest *request)
{
  if (sweepInProgress)
  {
    sweepStopRequested = true;
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"stop requested\"}");
  }
  else
  {
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"no sweep running\"}");
  }
}

// ===========================================================================
//  SERIAL COMMAND INTERFACE
//  Mirrors the full HTTP/web API over the USB-CDC serial link so the device can
//  be controlled from a standalone Web-Serial website without any WiFi.
//
//  Protocol (one command per line, '\n' terminated, case-insensitive keyword):
//    PING                         -> PONG
//    VERSION                      -> VERSION {json}
//    STATUS                       -> STATUS  {json}
//    HELP                         -> CMDS ...
//    MEASURE <f>                  -> DATA <f> <intensity> <bfield>
//    INTENSITY                    -> INT <intensity>            (cached, fast)
//    RATIO <f1> <f2> <f3|0> <avg> -> RATIO {json}
//    SWEEP <fb> <fe> <fs> [avg] [settle]
//                                 -> SWEEP START {json}
//                                    SWEEP DATA <idx> <total> <f> <intensity>
//                                    SWEEP DONE <count>  (or SWEEP STOP <count>)
//    SWEEPSTOP                    -> stops a running sweep
//    GAIN <0x00|0x10|0x20|0x30>   -> OK GAIN 0xXX
//    INTTIME <0..5>               -> OK INTTIME <v>
//    GETTSL                       -> TSL {json}
//    ADFON / ADFOFF               -> OK ADF ON|OFF
// ===========================================================================

// Parse up to maxN space-separated floats from s. Returns the count parsed.
static int parseFloats(const String &s, float *out, int maxN)
{
  int count = 0;
  int start = 0;
  int len = s.length();
  while (count < maxN && start < len)
  {
    while (start < len && s.charAt(start) == ' ')
      start++;
    if (start >= len)
      break;
    int end = s.indexOf(' ', start);
    if (end < 0)
      end = len;
    out[count++] = s.substring(start, end).toFloat();
    start = end + 1;
  }
  return count;
}

// Non-blocking check for a stop command while a blocking serial sweep runs.
// Consumes pending serial bytes and returns true if a STOP line was received.
static bool serialSweepStopCheck()
{
  static String buf;
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      buf.trim();
      bool stop = buf.equalsIgnoreCase("SWEEPSTOP") || buf.equalsIgnoreCase("STOP");
      buf = "";
      if (stop)
        return true;
    }
    else if (c >= 32 && c <= 126)
    {
      buf += c;
      if (buf.length() > 32)
        buf = "";
    }
  }
  return false;
}

// Blocking frequency sweep that streams results over serial. Mirrors the SSE
// handleSweep() endpoint. Honours SWEEPSTOP received mid-sweep.
void serialSweep(float fBegin, float fEnd, float fStep, uint8_t averages, uint16_t settle_ms)
{
  if (fBegin < ADF_FREQ_MIN || fEnd > ADF_FREQ_MAX || fStep <= 0 || fBegin >= fEnd)
  {
    Serial.println("SWEEP ERR {\"error\":\"invalid sweep parameters\"}");
    return;
  }

  int totalPoints = (int)((fEnd - fBegin) / fStep) + 1;
  if (totalPoints > MAX_SWEEP_BUFFER)
  {
    Serial.printf("SWEEP ERR {\"error\":\"too many points (max %d)\"}\n", MAX_SWEEP_BUFFER);
    return;
  }

  sweepBufferCount = 0;
  sweepInProgress = true;
  sweepStopRequested = false;
  setLEDStatus(LED_MEASURING);
  adf.begin();

  Serial.printf("SWEEP START {\"f_begin\":%.1f,\"f_end\":%.1f,\"f_step\":%.1f,\"avg\":%d,\"settle\":%d,\"total\":%d}\n",
                fBegin, fEnd, fStep, averages, settle_ms, totalPoints);

  int pointIndex = 0;
  for (float f = fBegin; f <= fEnd + 0.001f; f += fStep)
  {
    if (serialSweepStopCheck())
      sweepStopRequested = true;
    if (sweepStopRequested)
      break;

    adf.updateFrequency(f * 1e6); // MHz -> Hz
    delay(settle_ms);             // PLL settle

    uint64_t sum = 0;
    for (uint8_t i = 0; i < averages; i++)
    {
      sum += readIR();
      if (averages > 1)
        delay(1);
    }
    uint32_t intensity = (uint32_t)(sum / averages);

    if (sweepBufferCount < MAX_SWEEP_BUFFER)
    {
      sweepBuffer[sweepBufferCount].frequency = f;
      sweepBuffer[sweepBufferCount].intensity = intensity;
      sweepBufferCount++;
    }

    Serial.printf("SWEEP DATA %d %d %.1f %lu\n", pointIndex, totalPoints, f, intensity);
    pointIndex++;
    updateLEDs();
  }

  adf.stop();
  if (sweepStopRequested)
    Serial.printf("SWEEP STOP %d\n", pointIndex);
  else
    Serial.printf("SWEEP DONE %d\n", pointIndex);

  sweepInProgress = false;
  sweepStopRequested = false;
  setLEDStatus(LED_CONNECTED);
}

// Parse and execute a single serial command line.
void processSerialLine(String line)
{
  line.trim();
  if (line.length() == 0)
    return;

  lastSerialActivity = millis();

  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String args = (sp < 0) ? String("") : line.substring(sp + 1);
  cmd.toUpperCase();
  args.trim();

  if (cmd == "PING")
  {
    Serial.println("PONG");
  }
  else if (cmd == "HELP" || cmd == "?")
  {
    Serial.println("CMDS PING VERSION STATUS MEASURE INTENSITY RATIO SWEEP SWEEPSTOP GAIN INTTIME GETTSL ADFON ADFOFF");
  }
  else if (cmd == "VERSION" || cmd == "VER")
  {
    Serial.printf("VERSION {\"version\":\"%s\",\"build_date\":\"%s\",\"build_time\":\"%s\",\"git_hash\":\"%s\",\"git_branch\":\"%s\"}\n",
                  FIRMWARE_VERSION, BUILD_DATE, BUILD_TIME, GIT_HASH, GIT_BRANCH);
  }
  else if (cmd == "STATUS")
  {
    Serial.printf("STATUS {\"clients\":%d,\"fmin\":%.1f,\"fmax\":%.1f,\"led\":%d,\"sweep\":%s,\"tsl\":%s,\"gain\":%d,\"integration_time\":%d}\n",
                  WiFi.softAPgetStationNum(), ADF_FREQ_MIN, ADF_FREQ_MAX, (int)currentLEDStatus,
                  sweepInProgress ? "true" : "false", tsl_is_initialized ? "true" : "false",
                  (int)currentGain, (int)currentIntegrationTime);
  }
  else if (cmd == "MEASURE")
  {
    float f = args.toFloat();
    if (f >= ADF_FREQ_MIN && f <= ADF_FREQ_MAX)
    {
      setLEDStatus(LED_MEASURING);
      adf.updateFrequency(f * 1e6);
      delay(10);
      uint32_t i = readIR();
      Serial.printf("DATA %.1f %lu 0.0\n", f, i);
      setLEDStatus(LED_CONNECTED);
      adf.stop();
    }
    else
    {
      // Out-of-range (e.g. f=0 used for a plain live read) -> intensity only
      uint32_t i = readIR();
      Serial.printf("DATA %.1f %lu 0.0\n", f, i);
    }
  }
  else if (cmd == "INTENSITY" || cmd == "INT")
  {
    setLEDStatus(LED_INTENSITY);
    lastIntensityRequest = millis();
    Serial.printf("INT %u\n", cachedIR);
  }
  else if (cmd == "RATIO")
  {
    // RATIO <f1> <f2> <f3|0> <avg>   (f3 = 0 -> 2-point mode)
    float v[4] = {0, 0, 0, 3};
    int n = parseFloats(args, v, 4);
    if (n < 2)
    {
      Serial.println("RATIO ERR {\"error\":\"need f1 f2\"}");
    }
    else
    {
      float f1 = v[0], f2 = v[1];
      bool hasF3 = (n >= 3 && v[2] > 0.0f);
      float f3 = hasF3 ? v[2] : 0.0f;
      uint8_t averages = (n >= 4) ? (uint8_t)constrain((int)v[3], 1, 20) : 3;

      setLEDStatus(LED_MEASURING);
      uint32_t I1 = measureIntensityAtFrequency(f1, averages);
      uint32_t I2 = measureIntensityAtFrequency(f2, averages);
      uint32_t I3 = hasF3 ? measureIntensityAtFrequency(f3, averages) : 0;
      setLEDStatus(LED_CONNECTED);

      float r12 = (I1 + I2 > 0) ? ((float)I1 - (float)I2) / ((float)I1 + (float)I2) : 0.0f;
      float r13 = 0.0f, r23 = 0.0f;
      if (hasF3)
      {
        if (I1 + I3 > 0) r13 = ((float)I1 - (float)I3) / ((float)I1 + (float)I3);
        if (I2 + I3 > 0) r23 = ((float)I2 - (float)I3) / ((float)I2 + (float)I3);
      }

      String json = "RATIO {\"avg\":" + String(averages) + ",\"points\":[";
      json += "{\"f\":" + String(f1, 1) + ",\"I\":" + String(I1) + "},";
      json += "{\"f\":" + String(f2, 1) + ",\"I\":" + String(I2) + "}";
      if (hasF3)
        json += ",{\"f\":" + String(f3, 1) + ",\"I\":" + String(I3) + "}";
      json += "],\"r12\":" + String(r12, 6);
      json += ",\"r13\":" + String(r13, 6);
      json += ",\"r23\":" + String(r23, 6) + "}";
      Serial.println(json);
    }
  }
  else if (cmd == "SWEEP")
  {
    // SWEEP <fb> <fe> <fs> [avg] [settle]
    float v[5] = {0, 0, 0, 1, 10};
    int n = parseFloats(args, v, 5);
    if (n < 3)
    {
      Serial.println("SWEEP ERR {\"error\":\"need f_begin f_end f_step\"}");
    }
    else
    {
      uint8_t averages = (n >= 4) ? (uint8_t)constrain((int)v[3], 1, 20) : 1;
      uint16_t settle = (n >= 5) ? (uint16_t)constrain((int)v[4], 1, 200) : 10;
      serialSweep(v[0], v[1], v[2], averages, settle);
    }
  }
  else if (cmd == "SWEEPSTOP" || cmd == "STOP")
  {
    sweepStopRequested = true;
    Serial.println("OK SWEEPSTOP");
  }
  else if (cmd == "GAIN")
  {
    int g = (int)strtol(args.c_str(), nullptr, 0); // accepts 32 or 0x20
    if (applyTSLGain(g))
      Serial.printf("OK GAIN 0x%02X\n", (int)currentGain);
    else
      Serial.println("ERR invalid gain (use 0x00,0x10,0x20,0x30)");
  }
  else if (cmd == "INTTIME")
  {
    int t = (int)strtol(args.c_str(), nullptr, 0);
    if (applyTSLIntegration(t))
      Serial.printf("OK INTTIME %d\n", (int)currentIntegrationTime);
    else
      Serial.println("ERR invalid integration time (use 0..5)");
  }
  else if (cmd == "GETTSL")
  {
    Serial.printf("TSL {\"gain\":%d,\"integration_time\":%d}\n",
                  (int)currentGain, (int)currentIntegrationTime);
  }
  else if (cmd == "ADFON")
  {
    adf.begin();
    Serial.println("OK ADF ON");
  }
  else if (cmd == "ADFOFF")
  {
    adf.stop();
    Serial.println("OK ADF OFF");
  }
  else
  {
    Serial.printf("ERR unknown command: %s\n", cmd.c_str());
  }
}

// ---------------------------------------------------------------------------
// Pick the least-congested non-overlapping 2.4 GHz channel (1, 6 or 11).
//
// In 2.4 GHz each channel is 20 MHz wide but channels are only 5 MHz apart, so
// only 1/6/11 are mutually non-overlapping. A blindly random channel (the old
// behaviour) often landed on e.g. ch 4, overlapping two neighbouring APs and
// producing adjacent-channel interference — which is *worse* than sharing a
// channel. Here we do a quick station-mode scan, score each candidate by the
// number and strength of nearby APs on/around it, and return the clearest one.
// Falls back to a random 1/6/11 if scanning fails.
// ---------------------------------------------------------------------------
int pickBestChannel()
{
  const int candidates[3] = {1, 6, 11};

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);
  delay(100);

  int n = WiFi.scanNetworks(/*async*/ false, /*show_hidden*/ false);
  float score[3] = {0.0f, 0.0f, 0.0f};

  if (n > 0)
  {
    for (int i = 0; i < n; i++)
    {
      int ch = WiFi.channel(i);
      int32_t rssi = WiFi.RSSI(i);
      // Weight stronger neighbours more heavily (~ -30 dBm strong … -90 weak).
      float w = (float)(rssi + 100);
      if (w < 1.0f) w = 1.0f;
      for (int c = 0; c < 3; c++)
      {
        int d = abs(ch - candidates[c]);
        if (d == 0)      score[c] += w;                       // co-channel
        else if (d <= 4) score[c] += w * (1.0f - (float)d / 5.0f); // overlap
      }
    }
  }
  WiFi.scanDelete();

  int best = 0;
  if (n <= 0)
  {
    // Scan failed / empty — still avoid the overlapping channels.
    randomSeed(micros());
    best = random(0, 3);
  }
  else
  {
    for (int c = 1; c < 3; c++)
      if (score[c] < score[best]) best = c;
  }

  Serial.printf("Channel scan: %d APs; congestion 1=%.0f 6=%.0f 11=%.0f -> ch %d\n",
                n, score[0], score[1], score[2], candidates[best]);

  WiFi.mode(WIFI_OFF);
  delay(100);
  return candidates[best];
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
  Serial.println("=========================================");
  Serial.printf("  openUC2 ODMR Server %s\n", VERSION_STRING);
  Serial.printf("  Build : %s %s\n", BUILD_DATE, BUILD_TIME);
  Serial.printf("  Git   : %s (%s)\n", GIT_HASH, GIT_BRANCH);
  Serial.println("=========================================");

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

  // Ensure WiFi is completely disconnected and reset
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000);

#ifdef SERIAL_ONLY_MODE
  // Serial-only firmware build: keep the radio off entirely. The full command
  // set is still available over USB-CDC serial (see processSerialLine()).
  WiFi.mode(WIFI_OFF);
  Serial.println("SERIAL_ONLY_MODE: WiFi disabled — serial command interface only");
#else
  Serial.println("Starting WiFi Access Point...");
  if (IS_WIFI_AP_MODE)
  {
    // Pick the least-congested non-overlapping channel (1/6/11) instead of a
    // blind random channel — see pickBestChannel(). Random 1..11 frequently
    // landed on an overlapping channel, causing adjacent-channel interference.
    int wifiChannel = pickBestChannel();
    Serial.print("Using WiFi channel: ");
    Serial.println(wifiChannel);

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
#endif // SERIAL_ONLY_MODE

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

  // Detect whether the web UI was actually uploaded (pio run -t uploadfs).
  // If not, all routes fall back to a helpful "upload the filesystem" page.
  webAssetsPresent = SPIFFS.exists("/index.html") || SPIFFS.exists("/index.html.gz");
  if (!webAssetsPresent)
    Serial.println("WARNING: web assets not found in SPIFFS — run 'pio run -t uploadfs'");

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
    tsl_is_initialized = false;
    Serial.println("TSL2591 not found");
  }
  else
  {
    tsl_is_initialized = true;
    initialize_tsl();
  }

  // ADF4351 init
  Serial.println("ADF4351 init");
  adf.begin();
  // adf.updateFrequency(1800.0f); // Some initial frequency (example)
  // adf.updateFrequency(2.2e9); // 2.2 GHz // 1.800 GHz ─ writes R5…R0

  // print build time and date
  Serial.println("Build date: " + String(BUILD_DATE));
  Serial.println("Build time: " + String(BUILD_TIME));

  // Captive portal page (shown by the OS "sign in to network" popup). A plain
  // <a> link works reliably in every captive-portal WebView (no intent:// URIs
  // or window.open, which fail in those restricted contexts). Static storage,
  // so the non-capturing route lambdas below can reference it directly.
  static const char PORTAL_HTML[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>openUC2 ODMR</title>"
    "<style>"
    "body{font-family:sans-serif;text-align:center;padding:2.5rem 1rem;"
    "background:#1a4fa0;color:#fff;margin:0}"
    "h1{font-size:1.5rem;margin-bottom:.5rem}"
    "p{opacity:.85;margin-bottom:1.5rem;font-size:.95rem}"
    ".btn{display:inline-block;background:#fff;color:#1a4fa0;"
    "text-decoration:none;padding:.75rem 2.5rem;border-radius:8px;"
    "font-weight:bold;font-size:1.1rem;box-shadow:0 2px 8px rgba(0,0,0,.2)}"
    ".hint{margin-top:1.5rem;font-size:.82rem;opacity:.6}"
    "</style></head><body>"
    "<h1>&#128300; openUC2 ODMR</h1>"
    "<p>Connected to NV-Experiment device.</p>"
    "<a class='btn' href='http://192.168.4.1/'>Open Dashboard</a>"
    "<p class='hint'>If this page stays inside a small popup, open your<br>"
    "browser and navigate to: <b>http://192.168.4.1</b></p>"
    "</body></html>";

  // --- API / data endpoints (registered before serveStatic so they win) -----
  server.on("/measure", HTTP_GET, handleMeasure);
  server.on("/intensity", HTTP_GET, handleIntensity);
  server.on("/webserial_check", HTTP_GET, handleWebSerialCheck);
  server.on("/tsl/settings", HTTP_GET, handleGetTSLSettings);
  server.on("/tsl/gain", HTTP_POST, handleSetTSLGain);
  server.on("/tsl/integration_time", HTTP_POST, handleSetTSLIntegrationTime);
  server.on("/ratio", HTTP_GET, handleMeasureRatio);
  server.on("/sweep_buffer", HTTP_GET, handleSweepBuffer);
  server.on("/sweep_stop", HTTP_POST, handleSweepStop);
  // Note: /sweep is the AsyncEventSource — see setupSweepSource() below.

  // ADF4351 enable/disable endpoints (P1 #9)
  server.on("/ADF_Enable", HTTP_POST, [](AsyncWebServerRequest *request) {
    adf.begin();
    request->send(200, "application/json", "{\"status\":\"ok\",\"adf\":\"enabled\"}");
  });
  server.on("/ADF_Disable", HTTP_POST, [](AsyncWebServerRequest *request) {
    adf.stop();
    request->send(200, "application/json", "{\"status\":\"ok\",\"adf\":\"disabled\"}");
  });

  // Version endpoint (P1 #10) - returns firmware version as JSON
  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"build_date\":\"" + String(BUILD_DATE) + "\",";
    json += "\"build_time\":\"" + String(BUILD_TIME) + "\",";
    json += "\"git_hash\":\"" + String(GIT_HASH) + "\",";
    json += "\"git_branch\":\"" + String(GIT_BRANCH) + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  // --- Captive portal detection endpoints (P0 #1) ---------------------------
  // Android: return 200 + HTML so the "Sign in to network" notification shows.
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", PORTAL_HTML); });
  // Windows: expected plain-text bodies so it doesn't flag "limited" networks.
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Microsoft Connect Test"); });
  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Microsoft NCSI"); });
  server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", PORTAL_HTML); });
  // Apple iOS / macOS: "Success" tells the OS the network has internet, which
  // keeps the connection stable (users open Safari -> 192.168.4.1 themselves).
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", "Success"); });
  server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", "Success"); });
  server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "success"); });
  server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", PORTAL_HTML); });
  // Favicon: 204 so browsers stop retrying (P0 #3)
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(204, "image/x-icon", ""); });

  // --- Server-Sent-Events sweep channel at /sweep ---------------------------
  setupSweepSource();

  // --- Static web assets from SPIFFS ----------------------------------------
  // serveStatic transparently serves the pre-gzipped <file>.gz with
  // Content-Encoding: gzip (see scripts/gzip_assets.py). Large third-party
  // assets never change -> cache them "forever"; the HTML pages get a short
  // cache so UI updates still show up.
  server.serveStatic("/bootstrap.min.css", SPIFFS, "/bootstrap.min.css").setCacheControl("max-age=31536000, immutable");
  server.serveStatic("/bootstrap.bundle.min.js", SPIFFS, "/bootstrap.bundle.min.js").setCacheControl("max-age=31536000, immutable");
  server.serveStatic("/NVGitter.png", SPIFFS, "/NVGitter.png").setCacheControl("max-age=31536000, immutable");
  server.serveStatic("/style.css", SPIFFS, "/style.css").setCacheControl("max-age=86400");
  // Catch-all: every other path (incl. "/") -> SPIFFS, defaulting to index.html.
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setCacheControl("max-age=3600");

  // --- Fallback for anything not matched above ------------------------------
  server.onNotFound([](AsyncWebServerRequest *request) {
    String uri = request->url();
    // Silently 404 known OS background noise (Windows Update cert fetches, etc.)
    if (uri.startsWith("/msdownload/") || uri.endsWith(".cab") ||
        uri.startsWith("/GTSLT") || uri.startsWith("/ocsp") ||
        uri.startsWith("/en-GB/") || uri.startsWith("/en-US/"))
    {
      request->send(404, "text/plain", "");
      return;
    }
    // Android/Windows append a UUID, e.g. /generate_204_abc123 -> portal page.
    if (uri.startsWith("/generate_204"))
    {
      request->send(200, "text/html", PORTAL_HTML);
      return;
    }
    // Web UI files aren't uploaded yet -> tell the user how to fix it.
    if (!webAssetsPresent)
    {
      request->send_P(200, "text/html", FS_MISSING_HTML);
      return;
    }
    // HTML navigation to an unknown path -> serve the SPA shell (index.html).
    String accept = request->hasHeader("Accept") ? request->getHeader("Accept")->value() : "";
    if (uri.endsWith(".html") || accept.indexOf("text/html") >= 0)
    {
      request->send(SPIFFS, "/index.html", "text/html");
      return;
    }
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
  adf.stop(); // Disable output initially
}

void loop()
{
  // Process DNS requests for captive portal
  dnsServer.processNextRequest();

  // The async web server handles requests on its own task — no handleClient().
  // Drive the non-blocking frequency sweep (streams points over SSE) here.
  serviceHttpSweep();

  // Periodic sensor read for cached value (P0 #6)
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL)
  {
    lastSensorRead = millis();
    cachedIR = readIR();
  }

  // Update LED status indicators
  updateLEDs();

  // Determine "connected" state from either a WiFi client or recent serial
  // activity, so the status LEDs work in serial-only operation too.
  bool hasClient = (WiFi.softAPgetStationNum() > 0) ||
                   (millis() - lastSerialActivity < SERIAL_ACTIVITY_TIMEOUT);
  if (hasClient)
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

  // Catch serial commands and dispatch them to the unified parser
  // (see processSerialLine() for the full command set / protocol).
  while (Serial.available())
  { // collect one line
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (rxBuf.length() > 0)
      {
        processSerialLine(rxBuf);
        rxBuf = "";
      }
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
