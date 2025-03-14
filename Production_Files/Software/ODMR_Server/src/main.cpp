#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <SPI.h>
#include "adf4351.h"            // Your ADF4351 library
#include <Adafruit_TSL2591.h>   // Adafruit TSL2591 light sensor

// Wi-Fi credentials
const char* SSID     = "YourSSID";
const char* PASSWORD = "YourPASS";

// Laser pin example
static const int LASER_PIN = 10; 
bool laserState = false;

// ADF4351 object (example SPI settings)
ADF4351 adf(PIN_LE, SPI_MODE0, 8000000UL, MSBFIRST);

// TSL2591 sensor
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// WebServer on port 80
WebServer server(80);

// Utility to serve static files from SPIFFS
void handleFileRequest(const String& path) {
  String actualPath = path;
  if (actualPath.endsWith("/")) {
    actualPath += "index.html"; 
  }
  File file = SPIFFS.open(actualPath, "r");
  if (!file || file.isDirectory()) {
    server.send(404, "text/plain", "Not found");
    return;
  }

  String contentType = "text/html";
  if (actualPath.endsWith(".css"))  contentType = "text/css";
  if (actualPath.endsWith(".js"))   contentType = "application/javascript";
  if (actualPath.endsWith(".png"))  contentType = "image/png";
  if (actualPath.endsWith(".jpg"))  contentType = "image/jpeg";
  if (actualPath.endsWith(".ico"))  contentType = "image/x-icon";
  if (actualPath.endsWith(".svg"))  contentType = "image/svg+xml";

  server.streamFile(file, contentType);
  file.close();
}

// Example: read TSL2591 sensor (light intensity)
uint32_t readTSL2591() {
  sensors_event_t event;
  tsl.getEvent(&event);
  // event.light is in lux, but you can read raw channels as well
  uint32_t lux = (uint32_t) event.light;
  return lux;
}

// Example /laser_act handler for JSON:
// {"task": "/laser_act", "LASERid":1, "LASERval": 0 or 1}
void handleLaserAct() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no JSON body\"}");
    return;
  }
  String body = server.arg("plain");
  // Minimal parse for demonstration
  // (Use ArduinoJson if you need more robust parsing)
  if (body.indexOf("\"LASERval\":1") >= 0) {
    laserState = true;
    digitalWrite(LASER_PIN, HIGH);
  } else {
    laserState = false;
    digitalWrite(LASER_PIN, LOW);
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Example /odmr_act handler for JSON:
// {"task": "/odmr_act", "measure":1}
void handleOdmrAct() {
  if (!server.hasArg("plain")) {
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
void handleMeasure() {
  if (!server.hasArg("frequenz")) {
    server.send(400, "text/plain", "Error: no 'frequenz' param");
    return;
  }
  float freqRequested = server.arg("frequenz").toFloat();
  // Make sure it's within ADF4351 range
  if (freqRequested < ADF_FREQ_MIN || freqRequested > ADF_FREQ_MAX) {
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
  String reply = String(freqRequested, 1) + " " + intensity + " " + exampleMagVal;
  server.send(200, "text/plain", reply);
}

void setup() {
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);

  Serial.begin(115200);

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  // ADF4351 init
  adf.begin();
  adf.setf(2800.0f); // Some initial frequency (example)

  // TSL2591 init
  if (!tsl.begin()) {
    Serial.println("TSL2591 not found");
  } else {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
  }

  // Setup routes
  server.onNotFound([]() {
    handleFileRequest(server.uri());
  });
  server.on("/laser_act", HTTP_POST, handleLaserAct);
  server.on("/odmr_act",  HTTP_POST, handleOdmrAct);
  server.on("/measure",   HTTP_GET,  handleMeasure);

  server.begin();
}

void loop() {
  server.handleClient();
}
