#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include "webpage.h"  // UI Pro con Chart.js embebido

// Wi-Fi
const char* ssid = "CC1101-Scanner";
const char* password = "12345678";

WebServer server(80);

// SPI pins
constexpr uint8_t PIN_SCK  = 21;
constexpr uint8_t PIN_MOSI = 22;
constexpr uint8_t PIN_MISO = 20;
constexpr uint8_t CS1 = 10;
constexpr uint8_t CS2 = 11;
constexpr uint8_t CS3 = 23;

#define FXOSC 26.0f

// Band configs
float f1 = 300.0, f2 = 430.0, f3 = 860.0;
float step1 = 0.5f;
float step2 = 0.05f;   // Más fino en UHF
float step3 = 0.5f;

float b1min = 300.0, b1max = 360.0;
float b2min = 430.0, b2max = 470.0;
float b3min = 860.0, b3max = 920.0;

void computeFreqRegs(float mhz, uint8_t &f2r,uint8_t &f1r,uint8_t &f0r) {
  uint32_t fr=(uint32_t)((mhz*1000000.0/(FXOSC*1000000.0))*65536.0);
  f2r=(fr>>16)&0xFF; f1r=(fr>>8)&0xFF; f0r=fr&0xFF;
}

void ccWrite(uint8_t cs,uint8_t addr,uint8_t val){
  digitalWrite(cs,LOW); SPI.transfer(addr); SPI.transfer(val); digitalWrite(cs,HIGH);
}
uint8_t ccRead(uint8_t cs,uint8_t addr){
  digitalWrite(cs,LOW); SPI.transfer(addr|0x80); uint8_t v=SPI.transfer(0); digitalWrite(cs,HIGH); return v;
}
void strobe(uint8_t cs,uint8_t cmd){
  digitalWrite(cs,LOW); SPI.transfer(cmd); digitalWrite(cs,HIGH);
}

int16_t readRSSI(uint8_t cs){
  uint8_t raw=ccRead(cs,0x34);
  return(raw>=128?((int16_t)raw-256)/2-74:(raw/2)-74);
}

void initCC1101(uint8_t cs){
  strobe(cs,0x30); delay(5);
  // Aumentar ancho de banda (~812 kHz)
  ccWrite(cs,0x10,0x88);  // MDMCFG4
  ccWrite(cs,0x11,0x83);  // data rate
  ccWrite(cs,0x12,0x13);  // modulación  
  ccWrite(cs,0x07,0x06);
  ccWrite(cs,0x06,0x45);
  ccWrite(cs,0x08,0x05);
  strobe(cs,0x34);
}

void setFreq(uint8_t cs,float mhz){
  uint8_t f2r,f1r,f0r;
  computeFreqRegs(mhz,f2r,f1r,f0r);
  ccWrite(cs,0x0D,f2r);
  ccWrite(cs,0x0E,f1r);
  ccWrite(cs,0x0F,f0r);
}

// Handler /data
void handleData(){
  static int r1=-74,r2=-74,r3=-74;
  setFreq(CS1,f1);
  setFreq(CS2,f2);
  setFreq(CS3,f3);

  strobe(CS1,0x34); strobe(CS2,0x34); strobe(CS3,0x34);
  delay(4); // menos delay para más velocidad (ajustable)
  
  r1=readRSSI(CS1);
  r2=readRSSI(CS2);
  r3=readRSSI(CS3);

  // Next steps
  f1+=step1; if(f1>b1max) f1=b1min;
  f2+=step2; if(f2>b2max) f2=b2min;
  f3+=step3; if(f3>b3max) f3=b3min;

  char buf[200];
  snprintf(buf,sizeof(buf),
    "{\"f1\":%.2f,\"rssi1\":%d,\"f2\":%.2f,\"rssi2\":%d,\"f3\":%.2f,\"rssi3\":%d}",
    f1,r1,f2,r2,f3,r3);
  server.send(200,"application/json",buf);
}

void setup(){
  Serial.begin(115200);
  WiFi.softAP(ssid,password);
  Serial.print("\nAP: "); Serial.println(ssid);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());

  SPI.begin(PIN_SCK,PIN_MISO,PIN_MOSI);
  pinMode(CS1,OUTPUT); pinMode(CS2,OUTPUT); pinMode(CS3,OUTPUT);
  digitalWrite(CS1,HIGH); digitalWrite(CS2,HIGH); digitalWrite(CS3,HIGH);

  initCC1101(CS1);
  initCC1101(CS2);
  initCC1101(CS3);

  server.on("/",[](){server.send_P(200,"text/html",HTML_PAGE);});
  server.on("/data",handleData);
  server.begin();
}

void loop(){ server.handleClient(); }
