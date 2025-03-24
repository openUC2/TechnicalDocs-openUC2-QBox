

/*
ODMR_Server.INO 
Autor: Nils Haverkamp, Institut für Didaktik der Physik, Uni Münser
Mit diesem Sketch spannt ein ESP32 ein WLAN-Netzwerk auf. In diesem Netzwerk wird eine Webseite gehostet, über die sich der ESP32
und das zugehörige Experiment steuern lassen. Der ESP32 ist an einen PLL angeschlossen, der als Frequenz-Synthesizer dient. Diese
Mit der Frequenz kann eine Resonanz in NV-Zentren erzielt werden, die zu einer Abnahme der Fluoreszenz führt. Der Esp32 misst diese
Abnahme der Resonanz mit einem TSL 2591 Lichtstärkesensor. Dabei wird v.A. der Infrarot-Kanal ausgelesen. Die Intensität wird wieder
an die Webseite geschickt und dort dargestellt. 

Weitere Informationen: O3Q.de 
 */
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "adf4351.h"
#include "Adafruit_TSL2591.h"    
const char* ssid     = "ODMR1";
const char* password = "";
int channel          = 8;
int ssid_hidden      =0;
int max_connection   =2;
const int BPin=33;            
const int LaserPin=25;
AsyncWebServer server(80);
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
ADF4351  synth(PIN_LE, SPI_MODE0, 5000000UL , MSBFIRST);

void setup(){
  Serial.begin(115200);
  Wire.begin() ;
  tslbegin();
  synth.begin();
  SPIFFS.begin();
  pinMode(BPin,INPUT);
  pinMode(LaserPin,OUTPUT);
  analogReadResolution(12);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, channel, ssid_hidden, max_connection);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  HandleHTTPRequests();
  server.on("/measure", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam("frequenz")) {
      float f;
      f = request->getParam("frequenz")->value().toFloat();
      String I=getIR(f);
      String B=getB();
      String Message=String(f) + " " + I +" "+B;
      request->send_P(200, "text/plain", Message.c_str());
    }
  });
  server.on("/Laser_An", HTTP_POST, [] (AsyncWebServerRequest *request){
    digitalWrite(LaserPin,HIGH);
    request->send_P(200, "text/plain", "OK");
  });
    server.on("/Laser_Aus", HTTP_POST, [] (AsyncWebServerRequest *request){
    digitalWrite(LaserPin,LOW);    
    request->send_P(200, "text/plain", "OK");
  });
  server.begin();
  synth.enable();
  delay(500);
  synth.setf(3000);
  for (int i=0;i<6;i++){
    Serial.println(synth.getReg(i));
  }
  Serial.println("Ready");
}
String getIR(float f){
  synth.changef(f);
  uint16_t x = tsl.getLuminosity(TSL2591_INFRARED);
  Serial.println(x);
  return String(x);
}
String getB(){
  int B_Analog=analogRead(BPin);
  float B_Volt=B_Analog*5.0/4096;  
  float B=-77*B_Volt+177;
  return String(B);
}
void loop() {}
void HandleHTTPRequests(){
  server.on("/"             , HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/index.html"  );});
  server.on("/index.html"   , HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/index.html"  );});
  server.on("/style.css"    , HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/style.css"   );});
  server.on("/justage.html" , HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/justage.html");});
  server.on("/messung.html" , HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/messung.html");});
  server.on("/infos.html"   , HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/infos.html"  );});
  server.on("/NVGitter.png" , HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/NVGitter.png");});
}

void tslbegin(){
  if (tsl.begin()){}
  else{Serial.println(F("No sensor found"));}
  tsl.setGain(TSL2591_GAIN_MAX);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS); 
}
