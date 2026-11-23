// Prueba de buzzer: recorre los pines libres mostrando el nombre en grande.
// En cada pin: 2 s de melodía (buzzer pasivo) y 1 s en HIGH (buzzer activo).
// Compilar/subir:  pio run -e buzztest -t upload
#ifdef BUZZTEST
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, D5, D4);

struct Pin {
  uint8_t gpio;
  const char* name;
};
static const Pin CANDIDATES[] = {{D1, "D1"}, {D3, "D3"}, {D6, "D6"},
                                 {D7, "D7"}, {D9, "D9"}, {D10, "D10"}};

static void screen(const char* pin, const char* mode) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 10, "Prueba de buzzer");
  oled.setFont(u8g2_font_logisoso32_tf);
  oled.drawStr((128 - oled.getStrWidth(pin)) / 2, 50, pin);
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 63, mode);
  oled.sendBuffer();
  Serial.printf("%s %s\n", pin, mode);
}

void setup() {
  Serial.begin(115200);
  oled.begin();
  for (auto& p : CANDIDATES) pinMode(p.gpio, INPUT);
}

void loop() {
  static const uint16_t melody[] = {784, 740, 622, 440, 415, 659, 831, 1047};
  for (auto& p : CANDIDATES) {
    screen(p.name, "tono (pasivo)");
    pinMode(p.gpio, OUTPUT);
    for (uint16_t f : melody) {
      tone(p.gpio, f);
      delay(220);
    }
    noTone(p.gpio);
    screen(p.name, "HIGH (activo)");
    pinMode(p.gpio, OUTPUT);
    digitalWrite(p.gpio, HIGH);
    delay(1000);
    digitalWrite(p.gpio, LOW);
    pinMode(p.gpio, INPUT);
    delay(400);
  }
}
#endif
