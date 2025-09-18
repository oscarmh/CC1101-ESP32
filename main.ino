#include <SPI.h>

// --- Pines ---
constexpr uint8_t PIN_SCK  = 21;
constexpr uint8_t PIN_MOSI = 22;
constexpr uint8_t PIN_MISO = 20;

constexpr uint8_t CS1 = 10; // MOD 1
constexpr uint8_t CS2 = 11; // MOD 2
constexpr uint8_t CS3 = 23; // MOD 3

// --- Config. CC1101 ---
#define FXOSC 26.0f // MHz cristal CC1101

// --- Calcular FREQ bytes ---
void computeFreqRegs(float mhz, uint8_t &f2, uint8_t &f1, uint8_t &f0) {
  uint32_t freqReg = (uint32_t)((mhz * 1000000.0 / (FXOSC * 1000000.0)) * 65536.0);
  f2 = (freqReg >> 16) & 0xFF;
  f1 = (freqReg >> 8) & 0xFF;
  f0 = freqReg & 0xFF;
}

// --- SPI utils ---
void cc1101Write(uint8_t cs, uint8_t addr, uint8_t value) {
  digitalWrite(cs, LOW);
  SPI.transfer(addr);
  SPI.transfer(value);
  digitalWrite(cs, HIGH);
}

uint8_t cc1101Read(uint8_t cs, uint8_t addr) {
  digitalWrite(cs, LOW);
  SPI.transfer(addr | 0x80); // read
  uint8_t v = SPI.transfer(0);
  digitalWrite(cs, HIGH);
  return v;
}

// Strobe (e.g., SRX, STX, SCAL, SRES)
void cc1101Strobe(uint8_t cs, uint8_t strobe) {
  digitalWrite(cs, LOW);
  SPI.transfer(strobe);
  digitalWrite(cs, HIGH);
}

// RSSI lectura
int16_t cc1101ReadRSSI(uint8_t cs) {
  uint8_t raw = cc1101Read(cs, 0x34); // RSSI register
  int16_t rssi;
  if (raw >= 128) rssi = ((int16_t)raw - 256) / 2 - 74;
  else rssi = (raw / 2) - 74;
  return rssi;
}

// --- Inicialización de cada chip ---
void initCC1101(uint8_t cs) {
  cc1101Strobe(cs, 0x30); // SRES reset
  delay(5);

  // Config mínima:
  cc1101Write(cs, 0x0B, 0x0C); // FSCTRL1 freq offset
  cc1101Write(cs, 0x0C, 0x00); // FSCTRL0
  cc1101Write(cs, 0x0A, 0x00); // CHANNR=0 siempre

  // BW, Rate, etc (modo básico ASK/OOK 2-FSK)
  cc1101Write(cs, 0x12, 0x06); // MDMCFG4 (BW)
  cc1101Write(cs, 0x13, 0x83); // MDMCFG3 (data rate)
  cc1101Write(cs, 0x08, 0x05); // PKTLEN
  cc1101Write(cs, 0x07, 0x06); // PKTCTRL1
  cc1101Write(cs, 0x06, 0x45); // PKTCTRL0

  cc1101Write(cs, 0x0E, 0x34); // FIFOTHR
  cc1101Write(cs, 0x17, 0x07); // MC1
  cc1101Write(cs, 0x18, 0x30); // MC0

  // Modo: siempre RX
  cc1101Strobe(cs, 0x34); // SRX
}

void setFrequency(uint8_t cs, float mhz) {
  uint8_t f2, f1, f0;
  computeFreqRegs(mhz, f2, f1, f0);
  cc1101Write(cs, 0x0D, f2);
  cc1101Write(cs, 0x0E, f1);
  cc1101Write(cs, 0x0F, f0);
}

// --- Config inicial ---
float f1 = 300.0, f2 = 430.0, f3 = 860.0; // inicial
float step1 = 0.5, step2 = 0.5, step3 = 1.0;
float band1_min = 300.0, band1_max = 360.0;
float band2_min = 430.0, band2_max = 470.0;
float band3_min = 860.0, band3_max = 920.0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== ESCÁNER MULTIBANDA TOCHO ===");

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  pinMode(CS1, OUTPUT); digitalWrite(CS1, HIGH);
  pinMode(CS2, OUTPUT); digitalWrite(CS2, HIGH);
  pinMode(CS3, OUTPUT); digitalWrite(CS3, HIGH);

  initCC1101(CS1);
  initCC1101(CS2);
  initCC1101(CS3);

  setFrequency(CS1, f1);
  setFrequency(CS2, f2);
  setFrequency(CS3, f3);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 200) { // cada 200 ms
    last = millis();

    setFrequency(CS1, f1);
    setFrequency(CS2, f2);
    setFrequency(CS3, f3);

    cc1101Strobe(CS1, 0x34); // SRX
    cc1101Strobe(CS2, 0x34);
    cc1101Strobe(CS3, 0x34);

    delay(10); // settle

    int rssi1 = cc1101ReadRSSI(CS1);
    int rssi2 = cc1101ReadRSSI(CS2);
    int rssi3 = cc1101ReadRSSI(CS3);

    Serial.printf("[M1 %.2fMHz RSSI=%ddBm]  [M2 %.2fMHz RSSI=%ddBm]  [M3 %.2fMHz RSSI=%ddBm]\n",
                  f1, rssi1, f2, rssi2, f3, rssi3);

    // Barrido
    f1 += step1; if (f1 > band1_max) f1 = band1_min;
    f2 += step2; if (f2 > band2_max) f2 = band2_min;
    f3 += step3; if (f3 > band3_max) f3 = band3_min;
  }
}
