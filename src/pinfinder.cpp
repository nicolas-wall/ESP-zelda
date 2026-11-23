// Detector de pines: descubre cómo está cableado todo.
//   1) Prueba todas las combinaciones de pines hasta encontrar la OLED (SDA/SCL)
//      y después busca cuál pin es RES.
//   2) Hace sonar cada pin libre: el que suena es el del buzzer.
//   3) Queda escuchando botones: apretá cada uno y muestra en qué pin está.
// Todo se muestra en la OLED (si la encuentra) y por el monitor serie.
//
// Compilar/subir:  pio run -e pinfinder -t upload
#ifdef PINFINDER
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

struct Pin {
  uint8_t gpio;
  const char* name;
};
static const Pin PINS[] = {{D0, "D0"}, {D1, "D1"}, {D2, "D2"}, {D3, "D3"},
                           {D4, "D4"}, {D5, "D5"}, {D6, "D6"}, {D7, "D7"},
                           {D8, "D8"}, {D9, "D9"}, {D10, "D10"}};
static const int N = sizeof(PINS) / sizeof(PINS[0]);

int sdaIdx = -1, sclIdx = -1, resIdx = -1;
uint8_t oledAddr = 0;
U8G2* oled = nullptr;

String buttonsFound;   // "D0(GND) D1(GND) ..."
bool used[N];          // pines ya identificados

static void allPullup() {
  for (int i = 0; i < N; i++) pinMode(PINS[i].gpio, INPUT_PULLUP);
}

static bool probe(int sda, int scl, uint8_t& addr) {
  Wire.begin(PINS[sda].gpio, PINS[scl].gpio, 100000);
  Wire.setTimeOut(20);
  bool ok = false;
  const uint8_t addrs[] = {0x3C, 0x3D};
  for (uint8_t a : addrs) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      addr = a;
      ok = true;
      break;
    }
  }
  Wire.end();
  return ok;
}

static void show(const char* l1, const char* l2 = "", const char* l3 = "", const char* l4 = "") {
  Serial.printf("%s %s %s %s\n", l1, l2, l3, l4);
  if (!oled) return;
  oled->clearBuffer();
  oled->setFont(u8g2_font_6x10_tf);
  oled->drawStr(0, 10, l1);
  oled->drawStr(0, 24, l2);
  oled->drawStr(0, 38, l3);
  oled->drawStr(0, 52, l4);
  oled->sendBuffer();
}

static void findOled() {
  for (int s = 0; s < N && sdaIdx < 0; s++)
    for (int c = 0; c < N; c++) {
      if (s == c) continue;
      allPullup();
      uint8_t a;
      if (probe(s, c, a)) {
        sdaIdx = s;
        sclIdx = c;
        oledAddr = a;
        break;
      }
    }
  if (sdaIdx < 0) {
    Serial.println("OLED: NO ENCONTRADA en ninguna combinacion de pines");
    return;
  }
  // Buscar RES: el pin que, puesto en LOW, hace que la OLED deje de responder.
  for (int r = 0; r < N; r++) {
    if (r == sdaIdx || r == sclIdx) continue;
    allPullup();
    pinMode(PINS[r].gpio, OUTPUT);
    digitalWrite(PINS[r].gpio, LOW);
    delay(5);
    uint8_t a;
    bool alive = probe(sdaIdx, sclIdx, a);
    digitalWrite(PINS[r].gpio, HIGH);
    delay(5);
    if (!alive) {
      resIdx = r;
      break;
    }
  }
  allPullup();
  used[sdaIdx] = used[sclIdx] = true;
  if (resIdx >= 0) used[resIdx] = true;

  uint8_t res = resIdx >= 0 ? PINS[resIdx].gpio : U8X8_PIN_NONE;
  oled = new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(U8G2_R0, res, PINS[sclIdx].gpio, PINS[sdaIdx].gpio);
  oled->setI2CAddress(oledAddr * 2);
  oled->begin();
}

static void sweepBuzzer() {
  for (int round = 0; round < 2; round++)
    for (int i = 0; i < N; i++) {
      if (used[i]) continue;
      char l2[32];
      snprintf(l2, sizeof(l2), "Probando %s ...", PINS[i].name);
      show("BUZZER: escucha", l2, "El que suena es", "el pin del buzzer");
      tone(PINS[i].gpio, 1000 + i * 150);
      delay(900);
      noTone(PINS[i].gpio);
      pinMode(PINS[i].gpio, INPUT_PULLUP);
      delay(250);
    }
}

// Lee cada pin libre alternando pull-up / pull-down para detectar
// botones conectados a GND o a 3V3.
bool baseUp[N], baseDown[N], pressed[N];
uint32_t lastPrint = 0;

static void readModes(bool* up, bool* down) {
  for (int i = 0; i < N; i++) if (!used[i]) pinMode(PINS[i].gpio, INPUT_PULLUP);
  delayMicroseconds(300);
  for (int i = 0; i < N; i++) up[i] = !used[i] && digitalRead(PINS[i].gpio);
  for (int i = 0; i < N; i++) if (!used[i]) pinMode(PINS[i].gpio, INPUT_PULLDOWN);
  delayMicroseconds(300);
  for (int i = 0; i < N; i++) down[i] = !used[i] && digitalRead(PINS[i].gpio);
}

static void printSummary() {
  Serial.println("===== RESUMEN =====");
  if (sdaIdx >= 0)
    Serial.printf("OLED: SDA=%s SCL=%s RES=%s ADDR=0x%02X\n", PINS[sdaIdx].name, PINS[sclIdx].name,
                  resIdx >= 0 ? PINS[resIdx].name : "ninguno", oledAddr);
  else
    Serial.println("OLED: no encontrada");
  Serial.printf("BOTONES: %s\n", buttonsFound.length() ? buttonsFound.c_str() : "(apreta cada boton)");
  Serial.println("===================");
}

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println("\n*** Detector de pines ***");
  allPullup();
  findOled();
  if (oled) {
    char l2[32], l3[32];
    snprintf(l2, sizeof(l2), "SDA=%s SCL=%s", PINS[sdaIdx].name, PINS[sclIdx].name);
    snprintf(l3, sizeof(l3), "RES=%s  0x%02X", resIdx >= 0 ? PINS[resIdx].name : "-", oledAddr);
    show("OLED encontrada!", l2, l3, "");
    delay(3000);
  }
  sweepBuzzer();
  readModes(baseUp, baseDown);
  show("BOTONES:", "Apreta cada boton", "de a uno", "");
}

void loop() {
  bool up[N], down[N];
  readModes(up, down);
  for (int i = 0; i < N; i++) {
    if (used[i]) continue;
    bool toGnd = baseUp[i] && !up[i];     // con pull-up baja a 0 => botón a GND
    bool toVcc = !baseDown[i] && down[i]; // con pull-down sube a 1 => botón a 3V3
    bool now = toGnd || toVcc;
    if (now && !pressed[i]) {
      String tag = String(PINS[i].name) + (toGnd ? "(GND)" : "(3V3)");
      if (buttonsFound.indexOf(tag) < 0) buttonsFound += tag + " ";
      char l2[32];
      snprintf(l2, sizeof(l2), "Boton en %s", tag.c_str());
      show("BOTONES:", l2, "Encontrados:", buttonsFound.c_str());
    }
    pressed[i] = now;
  }
  if (millis() - lastPrint > 3000) {
    lastPrint = millis();
    printSummary();
  }
  delay(20);
}
#endif
