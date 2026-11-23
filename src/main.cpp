// Reloj de Hyrule — reloj de escritorio con recordatorios estilo Zelda
// XIAO ESP32-S3 + OLED 128x64 I2C + buzzer pasivo + 3 botones

#if !defined(PINFINDER) && !defined(BUZZTEST)
#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <time.h>
#include <vector>

#include "config.h"
#include "music.h"
#include "net.h"
#include "reminders.h"
#include "songs.h"
#include "sprites.h"

// ----------------------------------------------------------------------------
// Hardware
// ----------------------------------------------------------------------------
#if OLED_TYPE == 1
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, PIN_OLED_RES, PIN_OLED_SCL, PIN_OLED_SDA);
#elif OLED_TYPE == 2
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, PIN_OLED_RES, PIN_OLED_SCL, PIN_OLED_SDA);
#else
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, PIN_OLED_RES, PIN_OLED_SCL, PIN_OLED_SDA);
#endif

MusicPlayer music;
Reminders reminders;
Preferences prefs;

struct Button {
  uint8_t pin;
  bool down = false, longFired = false;
  uint32_t changed = 0, pressedAt = 0;
  bool press = false, shortRel = false, longPress = false;  // eventos de este frame

  explicit Button(uint8_t p) : pin(p) {}
  void begin() { pinMode(pin, BTN_TO_GND ? INPUT_PULLUP : INPUT_PULLDOWN); }
  void poll(uint32_t now) {
    press = shortRel = longPress = false;
    bool raw = digitalRead(pin) == (BTN_TO_GND ? LOW : HIGH);
    if (raw != down && now - changed > 25) {
      changed = now;
      down = raw;
      if (down) {
        press = true;
        pressedAt = now;
        longFired = false;
      } else if (!longFired) {
        shortRel = true;
      }
    }
    if (down && !longFired && now - pressedAt > 650) {
      longFired = true;
      longPress = true;
    }
  }
};

Button btnPrev(PIN_BTN_PREV), btnOk(PIN_BTN_OK), btnNext(PIN_BTN_NEXT);

// ----------------------------------------------------------------------------
// Utilidades de dibujo
// ----------------------------------------------------------------------------
static float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
static float easeOut(float t) {
  t = clamp01(t);
  float u = 1 - t;
  return 1 - u * u * u;
}

static void centerText(int y, const char* s) {
  u8g2.drawUTF8((128 - u8g2.getUTF8Width(s)) / 2, y, s);
}

static void drawSprite(int x, int y, const char* const* rows, int h) {
  for (int r = 0; r < h; r++)
    for (int c = 0; rows[r][c]; c++)
      if (rows[r][c] == '#') u8g2.drawPixel(x + c, y + r);
}

static void drawSparkle(int x, int y, int size) {
  if (size <= 0) return;
  u8g2.drawPixel(x, y);
  if (size >= 2) {
    u8g2.drawPixel(x - 1, y); u8g2.drawPixel(x + 1, y);
    u8g2.drawPixel(x, y - 1); u8g2.drawPixel(x, y + 1);
  }
  if (size >= 3) {
    u8g2.drawPixel(x - 2, y); u8g2.drawPixel(x + 2, y);
    u8g2.drawPixel(x, y - 2); u8g2.drawPixel(x, y + 2);
  }
}

// Trifuerza: cx = centro, ty = punta superior, h = alto de cada triángulo.
static void drawTriforce(int cx, int ty, int h, const int* dx = nullptr, const int* dy = nullptr) {
  int hb = (h * 577) / 1000;
  int ox[3] = {0, 0, 0}, oy[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    if (dx) ox[i] = dx[i];
    if (dy) oy[i] = dy[i];
  }
  u8g2.drawTriangle(cx + ox[0], ty + oy[0], cx - hb + ox[0], ty + h + oy[0], cx + hb + ox[0], ty + h + oy[0]);
  u8g2.drawTriangle(cx - hb + ox[1], ty + h + oy[1], cx - 2 * hb + ox[1], ty + 2 * h + oy[1], cx + ox[1], ty + 2 * h + oy[1]);
  u8g2.drawTriangle(cx + hb + ox[2], ty + h + oy[2], cx + ox[2], ty + 2 * h + oy[2], cx + 2 * hb + ox[2], ty + 2 * h + oy[2]);
}

static const int SPARKLES[][2] = {{8, 8},   {119, 10}, {16, 34}, {111, 36},
                                  {28, 18}, {100, 22}, {6, 56},  {121, 56}};

static void drawSparkleField(uint32_t t) {
  for (int i = 0; i < 8; i++) {
    int phase = ((t + i * 170) / 110) % 9;
    int size = phase < 3 ? phase + 1 : (phase < 5 ? 5 - phase : 0);
    drawSparkle(SPARKLES[i][0], SPARKLES[i][1], size);
  }
}

// Hada: cuerpo brillante + alas que aletean.
static void drawFairyAt(int x, int y, uint32_t now) {
  u8g2.drawDisc(x, y, 3);
  if ((now / 150) % 2) u8g2.drawCircle(x, y, 6, U8G2_DRAW_ALL);
  bool flap = (now / 70) % 2;
  u8g2.drawEllipse(x - 6, y - 3, 3, flap ? 2 : 1);
  u8g2.drawEllipse(x + 6, y - 3, 3, flap ? 2 : 1);
}

// Corta un texto en líneas que entren en `width` píxeles (usa la fuente actual).
static std::vector<String> wrapText(const String& text, int width) {
  std::vector<String> lines;
  String src = text + '\n';
  String para;
  for (unsigned i = 0; i < src.length(); i++) {
    char ch = src[i];
    if (ch != '\n') {
      para += ch;
      continue;
    }
    String line;
    int start = 0;
    while (start <= (int)para.length()) {
      int sp = para.indexOf(' ', start);
      if (sp < 0) sp = para.length();
      String word = para.substring(start, sp);
      String test = line.length() ? line + " " + word : word;
      if (line.length() && u8g2.getUTF8Width(test.c_str()) > width) {
        lines.push_back(line);
        line = word;
      } else {
        line = test;
      }
      start = sp + 1;
    }
    lines.push_back(line);
    para = "";
  }
  return lines;
}

// Caja de texto que escribe letra por letra, con páginas (como en el juego).
struct TypeBox {
  std::vector<String> lines;
  int perPage = 3, page = 0, typed = 0;
  uint32_t last = 0;

  void set(const String& text, int width, int per) {
    u8g2.setFont(u8g2_font_6x10_tf);
    lines = wrapText(text, width);
    perPage = per;
    page = 0;
    typed = 0;
  }
  int pages() const { return (lines.size() + perPage - 1) / perPage; }
  int pageBytes() const {
    int total = 0;
    for (int i = 0; i < perPage; i++) {
      int li = page * perPage + i;
      if (li < (int)lines.size()) total += lines[li].length();
    }
    return total;
  }
  uint8_t byteAt(int k) const {
    for (int i = 0; i < perPage; i++) {
      int li = page * perPage + i;
      if (li >= (int)lines.size()) break;
      int len = lines[li].length();
      if (k < len) return (uint8_t)lines[li][k];
      k -= len;
    }
    return 0;
  }
  bool full() const { return typed >= pageBytes(); }
  void tick(uint32_t now) {
    if (now - last < 35 || full()) return;
    last = now;
    int total = pageBytes();
    typed++;
    while (typed < total && (byteAt(typed) & 0xC0) == 0x80) typed++;
  }
  // OK: completa la página, pasa a la siguiente o devuelve true si terminó.
  bool ok() {
    if (!full()) {
      typed = pageBytes();
      return false;
    }
    if (page + 1 < pages()) {
      page++;
      typed = 0;
      return false;
    }
    return true;
  }
  void draw(int x, int y0, int lineH, uint32_t now) const {
    u8g2.setFont(u8g2_font_6x10_tf);
    int remaining = typed;
    for (int i = 0; i < perPage && remaining > 0; i++) {
      int li = page * perPage + i;
      if (li >= (int)lines.size()) break;
      int n = min(remaining, (int)lines[li].length());
      u8g2.drawUTF8(x, y0 + i * lineH, lines[li].substring(0, n).c_str());
      remaining -= n;
    }
    if (full() && (now / 300) % 2 == 0) u8g2.drawTriangle(116, 57, 123, 57, 119, 61);
  }
};

// ----------------------------------------------------------------------------
// Escenas
// ----------------------------------------------------------------------------
enum Scene {
  SC_BOOT, SC_CLOCK, SC_MENU, SC_REMINDERS, SC_NET, SC_NOTIFY,
  SC_OCA_LIST, SC_OCA_PLAY, SC_CHEST, SC_HEARTS, SC_FAIRY
};
Scene scene = SC_BOOT;
uint32_t sceneStart = 0, lastInput = 0;

void enterScene(Scene s);

static bool localTime(tm& t) { return timeValid() && getLocalTime(&t, 0); }

// ---------- Notificaciones (las manda un recordatorio o el teléfono) ----------
std::vector<String> pendingNotes;
TypeBox noteBox;
uint32_t noteChimeAt = 0;
int noteChimes = 0;

void pushNotification(const String& text) {
  if (pendingNotes.size() < 6) pendingNotes.push_back(text);
}

static void startNextNotification(uint32_t now) {
  noteBox.set(pendingNotes.front(), 116, 4);
  pendingNotes.erase(pendingNotes.begin());
  noteChimes = 1;
  noteChimeAt = now + NOTIFY_REPEAT_S * 1000UL;
  music.play(&SONG_HEY);
}

static void updateNotify(uint32_t now) {
  noteBox.tick(now);
  if (noteChimes < NOTIFY_REPEATS && (int32_t)(now - noteChimeAt) >= 0) {
    noteChimes++;
    noteChimeAt = now + NOTIFY_REPEAT_S * 1000UL;
    music.play(&SONG_HEY);
  }
  if (btnOk.shortRel && noteBox.ok()) {
    if (!pendingNotes.empty()) startNextNotification(now);
    else enterScene(SC_CLOCK);
  }
}

static void drawNotify(uint32_t now) {
  float s = now / 1000.0f;
  drawFairyAt(10 + (int)(3 * sinf(s * 2.1f)), 7 + (int)(2 * sinf(s * 3.3f)), now);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawUTF8(24, 10, "¡Hey! ¡Escucha!");
  if (!pendingNotes.empty()) {
    char more[8];
    snprintf(more, sizeof(more), "+%d", (int)pendingNotes.size());
    u8g2.drawStr(127 - u8g2.getStrWidth(more), 10, more);
  }
  u8g2.drawRFrame(1, 15, 126, 49, 3);
  noteBox.draw(6, 27, 11, now);
}

// ---------- Reloj ----------
static const char* const DAYS[] = {"dom", "lun", "mar", "mié", "jue", "vie", "sáb"};
static const char* const MONTHS[] = {"ene", "feb", "mar", "abr", "may", "jun",
                                     "jul", "ago", "sep", "oct", "nov", "dic"};
static const char* const SPR_BELL[] = {
  "...#...",
  "..###..",
  ".#####.",
  ".#####.",
  ".#####.",
  "#######",
  "...#...",
};
static const int STARS[][2] = {{5, 3},  {19, 9},  {33, 2},  {47, 7},  {61, 3},
                               {75, 10}, {89, 4}, {103, 8}, {117, 2}, {124, 11}};
int lastChimeHour = -1;

static void drawSun(int x, int y, uint32_t now) {
  u8g2.drawDisc(x, y, 3);
  float a0 = (now % 4000) / 4000.0f * PI / 2;
  for (int k = 0; k < 4; k++) {
    float a = a0 + k * PI / 2;
    u8g2.drawLine(x + cosf(a) * 5, y + sinf(a) * 5, x + cosf(a) * 7, y + sinf(a) * 7);
  }
}

static void drawMoon(int x, int y) {
  u8g2.drawDisc(x, y, 4);
  u8g2.setDrawColor(0);
  u8g2.drawDisc(x + 2, y - 1, 3);
  u8g2.setDrawColor(1);
}

static void drawCloud(int x, int y) {
  u8g2.drawDisc(x, y, 3);
  u8g2.drawDisc(x + 4, y - 2, 4);
  u8g2.drawDisc(x + 9, y, 3);
  u8g2.drawHLine(x, y + 3, 10);
}

static void updateClock(uint32_t now) {
  if (btnOk.shortRel) enterScene(SC_MENU);
  if (btnNext.shortRel) enterScene(SC_REMINDERS);
#if HOURLY_CHIME
  tm t;
  if (localTime(t) && t.tm_min == 0 && t.tm_hour != lastChimeHour) {
    lastChimeHour = t.tm_hour;
    if (t.tm_hour >= 9 && t.tm_hour <= 21) music.play(&SONG_ITEM);
  }
#endif
}

static void drawClock(uint32_t now) {
  tm t;
  if (!localTime(t)) {
    drawTriforce(64, 2, 10);
    u8g2.setFont(u8g2_font_6x10_tf);
    if (netPortalActive()) {
      centerText(34, "Configurá el WiFi:");
      centerText(46, "conectate a la red");
      centerText(58, WIFI_SETUP_AP);
    } else {
      centerText(40, netConnected() ? "Buscando la hora..." : "Conectando WiFi...");
      if ((now / 400) % 2) centerText(54, "...");
    }
    return;
  }

  // Cielo: sol de 7 a 20, luna y estrellas de noche.
  float hf = t.tm_hour + t.tm_min / 60.0f;
  bool day = hf >= 7 && hf < 20;
  float p = day ? (hf - 7) / 13.0f : fmodf(hf - 20 + 24, 24) / 11.0f;
  int sx = 6 + (int)(p * 116);
  int sy = 13 - (int)(sinf(p * PI) * 9);
  if (day) {
    drawCloud((int)((now / 180) % 170) - 20, 9);
    drawCloud((int)((now / 260 + 90) % 170) - 20, 5);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(sx, sy, 8);
    u8g2.setDrawColor(1);
    drawSun(sx, sy, now);
  } else {
    for (int i = 0; i < 10; i++)
      if (((now / 600) + i * 3) % 7) u8g2.drawPixel(STARS[i][0], STARS[i][1]);
    drawMoon(sx, sy);
  }

  // Hora grande con los dos puntos titilando
  char hh[3], mm[3];
  snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
  snprintf(mm, sizeof(mm), "%02d", t.tm_min);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  int wh = u8g2.getStrWidth(hh);
  u8g2.drawStr(58 - wh, 46, hh);
  if (t.tm_sec % 2 == 0) u8g2.drawStr(60, 44, ":");
  u8g2.drawStr(70, 46, mm);

  // Abajo: fecha y próximo recordatorio
  u8g2.setFont(u8g2_font_6x10_tf);
  char date[24];
  snprintf(date, sizeof(date), "%s %d %s", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon]);
  u8g2.drawUTF8(0, 63, date);

  int idx[MAX_REMINDERS];
  if (reminders.sorted(t, idx) > 0) {
    const Reminder& r = reminders.items[idx[0]];
    char hm[8];
    snprintf(hm, sizeof(hm), "%02d:%02d", r.hour, r.minute);
    int w = u8g2.getStrWidth(hm);
    u8g2.drawStr(128 - w, 63, hm);
    drawSprite(128 - w - 10, 55, SPR_BELL, 7);
  }
  if (music.muted()) {
    u8g2.drawLine(121, 1, 126, 6);
    u8g2.drawLine(121, 6, 126, 1);
  }
}

// ---------- Arranque: la Trifuerza se arma + título ----------
const uint32_t BOOT_FLY = 1800, BOOT_FLASH = 700, BOOT_TITLE = 4500;
bool bootChime = false;

static void updateBoot(uint32_t now, uint32_t t) {
  if (t >= BOOT_FLY && !bootChime) {
    bootChime = true;
    music.play(&SONG_SECRET);
  }
  bool pressed = btnOk.shortRel || btnPrev.shortRel || btnNext.shortRel;
  if ((t > BOOT_FLY && pressed) || t > BOOT_FLY + BOOT_FLASH + BOOT_TITLE) enterScene(SC_CLOCK);
}

static void drawBoot(uint32_t now, uint32_t t) {
  if (t < BOOT_FLY + BOOT_FLASH) {
    float e = easeOut((float)t / BOOT_FLY);
    int dx[3] = {0, (int)(-90 * (1 - e)), (int)(90 * (1 - e))};
    int dy[3] = {(int)(-70 * (1 - e)), (int)(40 * (1 - e)), (int)(40 * (1 - e))};
    drawTriforce(64, 6, 26, dx, dy);
    if (t > BOOT_FLY && ((t - BOOT_FLY) / 90) % 2 == 0) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(0, 0, 128, 64);
      u8g2.setDrawColor(1);
    }
    return;
  }
  uint32_t tt = t - BOOT_FLY - BOOT_FLASH;
  drawTriforce(64, 1, 12);
  drawSparkleField(tt);
  if (tt > 300) {
    u8g2.setFont(u8g2_font_ncenB10_tf);
    centerText(40, GIFT_TITLE);
  }
  if (tt > 900) {
    u8g2.setFont(u8g2_font_ncenB12_tf);
    centerText(58, GIFT_NAME);
  }
}

// ---------- Listas (menú, canciones, recordatorios) ----------
static void drawList(const char* title, const char* const* items, int n, int sel, int& scroll) {
  const int rows = 4, rowH = 12, y0 = 14;
  u8g2.setFont(u8g2_font_6x10_tf);
  drawTriforce(6, 1, 5);
  drawTriforce(121, 1, 5);
  if (music.muted()) {
    u8g2.drawLine(113, 2, 118, 7);
    u8g2.drawLine(113, 7, 118, 2);
  }
  centerText(9, title);
  u8g2.drawHLine(0, 12, 128);
  if (sel < scroll) scroll = sel;
  if (sel >= scroll + rows) scroll = sel - rows + 1;
  for (int i = 0; i < rows && scroll + i < n; i++) {
    int idx = scroll + i;
    int y = y0 + i * rowH;
    if (idx == sel) {
      u8g2.drawRBox(0, y, n > rows ? 121 : 128, rowH, 2);
      u8g2.setDrawColor(0);
      drawSprite(3, y + 2, SPR_HEART, 8);
      u8g2.drawUTF8(16, y + 9, items[idx]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawUTF8(16, y + 9, items[idx]);
    }
  }
  if (n > rows) {
    int bh = 48 * rows / n;
    int by = y0 + (48 - bh) * scroll / (n - rows);
    u8g2.drawVLine(125, y0, 48);
    u8g2.drawBox(124, by, 3, bh);
  }
}

static const char* const MENU_ITEMS[] = {"Reloj",          "Recordatorios", "Ocarina",
                                         "Abrir el cofre", "Corazones",     "Hada",
                                         "Conexión"};
static const Scene MENU_TARGETS[] = {SC_CLOCK, SC_REMINDERS, SC_OCA_LIST, SC_CHEST,
                                     SC_HEARTS, SC_FAIRY, SC_NET};
const int MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
int menuSel = 0, menuScroll = 0;

static void updateMenu(uint32_t now) {
  if (btnPrev.shortRel) menuSel = (menuSel + MENU_COUNT - 1) % MENU_COUNT;
  if (btnNext.shortRel) menuSel = (menuSel + 1) % MENU_COUNT;
  if (btnOk.shortRel) enterScene(MENU_TARGETS[menuSel]);
}

// ---------- Recordatorios (lista en el reloj) ----------
String remLabels[MAX_REMINDERS];
const char* remPtrs[MAX_REMINDERS];
int remCount = 0, remSel = 0, remScroll = 0;

static void refreshReminderList() {
  tm t;
  int idx[MAX_REMINDERS];
  if (localTime(t)) {
    remCount = reminders.sorted(t, idx);
  } else {
    remCount = 0;
    for (int i = 0; i < MAX_REMINDERS; i++)
      if (reminders.items[i].used) idx[remCount++] = i;
  }
  for (int k = 0; k < remCount; k++) {
    const Reminder& r = reminders.items[idx[k]];
    char hm[8];
    snprintf(hm, sizeof(hm), "%02d:%02d ", r.hour, r.minute);
    remLabels[k] = String(hm) + r.text;
    remPtrs[k] = remLabels[k].c_str();
  }
  if (remSel >= remCount) remSel = max(0, remCount - 1);
}

static void updateReminders(uint32_t now) {
  if (remCount > 0) {
    if (btnPrev.shortRel) remSel = (remSel + remCount - 1) % remCount;
    if (btnNext.shortRel) remSel = (remSel + 1) % remCount;
  }
  if (btnOk.shortRel) enterScene(SC_CLOCK);
  if ((now - sceneStart) % 2000 < 40) refreshReminderList();  // por si llegan nuevos
}

static void drawReminders(uint32_t now) {
  if (remCount > 0) {
    drawList("Recordatorios", remPtrs, remCount, remSel, remScroll);
    return;
  }
  u8g2.setFont(u8g2_font_6x10_tf);
  centerText(9, "Recordatorios");
  u8g2.drawHLine(0, 12, 128);
  centerText(28, "No hay ninguno.");
  centerText(42, "Agregalos desde el");
  centerText(54, "teléfono en:");
  String url = netConnected() ? netIP() : String("(sin WiFi)");
  u8g2.setFont(u8g2_font_5x7_tf);
  centerText(63, url.c_str());
}

// ---------- Conexión ----------
static void updateNet(uint32_t now) {
  if (btnOk.shortRel) enterScene(SC_MENU);
  if (btnNext.longPress) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    centerText(36, "Borrando WiFi...");
    u8g2.sendBuffer();
    netResetWifi();
  }
}

static void drawNet(uint32_t now) {
  u8g2.setFont(u8g2_font_6x10_tf);
  centerText(9, "Conexión");
  u8g2.drawHLine(0, 12, 128);
  if (netConnected()) {
    String ssid = "WiFi: " + netSSID();
    String ip = "http://" + netIP();
    u8g2.drawUTF8(0, 25, ssid.c_str());
    u8g2.drawStr(0, 37, ip.c_str());
    u8g2.drawStr(0, 49, "http://hyrule.local");
  } else if (netPortalActive()) {
    u8g2.drawUTF8(0, 25, "Sin WiFi. Desde el");
    u8g2.drawUTF8(0, 37, "teléfono conectate a:");
    u8g2.drawStr(0, 49, WIFI_SETUP_AP);
  } else {
    u8g2.drawUTF8(0, 30, "Conectando...");
  }
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawUTF8(0, 63, "Mantener > : borrar WiFi");
}

// ---------- Ocarina ----------
const char* ocaNames[OCARINA_SONG_COUNT];
int ocaSel = 0, ocaScroll = 0;
uint32_t ocaDoneAt = 0;

static void updateOcaList(uint32_t now) {
  if (btnPrev.shortRel) ocaSel = (ocaSel + OCARINA_SONG_COUNT - 1) % OCARINA_SONG_COUNT;
  if (btnNext.shortRel) ocaSel = (ocaSel + 1) % OCARINA_SONG_COUNT;
  if (btnOk.shortRel) enterScene(SC_OCA_PLAY);
}

static uint8_t ocarinaMidi(char b) {
  switch (b) {
    case 'A': return nt::d4;
    case 'v': return nt::f4;
    case '>': return nt::a4;
    case '<': return nt::b4;
    case '^': return nt::d5;
  }
  return 0;
}

static void drawOcaButton(int cx, int cy, char b, bool on) {
  if (on) u8g2.drawDisc(cx, cy, 6);
  else u8g2.drawCircle(cx, cy, 6);
  u8g2.setDrawColor(on ? 0 : 1);
  switch (b) {
    case 'A':
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(cx - 2, cy + 4, "A");
      break;
    case '^': u8g2.drawTriangle(cx, cy - 3, cx - 3, cy + 2, cx + 4, cy + 2); break;
    case 'v': u8g2.drawTriangle(cx, cy + 3, cx - 3, cy - 2, cx + 4, cy - 2); break;
    case '<': u8g2.drawTriangle(cx - 3, cy, cx + 2, cy - 3, cx + 2, cy + 4); break;
    case '>': u8g2.drawTriangle(cx + 3, cy, cx - 2, cy - 3, cx - 2, cy + 4); break;
  }
  u8g2.setDrawColor(1);
}

static int noteY(uint8_t midi) {
  static const uint8_t dia[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
  int step = (midi / 12) * 7 + dia[midi % 12];
  return 56 - 2 * (step - 37);
}

static void updateOcaPlay(uint32_t now) {
  if (btnOk.shortRel) {
    music.stop();
    enterScene(SC_OCA_LIST);
    return;
  }
  if (!music.playing()) {
    if (!ocaDoneAt) ocaDoneAt = now;
    if (now - ocaDoneAt > 1500) enterScene(SC_OCA_LIST);
  }
}

static void drawOcaPlay(uint32_t now) {
  const Song* main = OCARINA_SONGS[ocaSel];
  const Song* cur = music.song();
  bool solved = cur != main && (ocaDoneAt || cur == &SONG_SECRET);

  u8g2.setFont(u8g2_font_6x10_tf);
  centerText(9, solved ? "¡Correcto!" : main->name);

  int n = strlen(main->ocarina);
  uint8_t sounding = music.currentMidi();
  const int spacing = 15;
  int x0 = 64 - (n - 1) * spacing / 2;
  for (int i = 0; i < n; i++) {
    char b = main->ocarina[i];
    bool on = solved || (cur == main && sounding && ocarinaMidi(b) == sounding);
    drawOcaButton(x0 + i * spacing, 21, b, on);
  }

  for (int k = 0; k < 5; k++) u8g2.drawHLine(0, 40 + 4 * k, 128);
  const int head = 20;
  for (int y = 30; y < 64; y += 2) u8g2.drawPixel(head, y);
  if (cur) {
    float ct = music.tick();
    float ppt = cur->msPerTick * 0.035f;
    uint32_t st = 0;
    for (int i = 0; i < cur->count; i++) {
      const Note& nn = cur->notes[i];
      float x = head + (st - ct) * ppt;
      if (x > 132) break;
      if (nn.midi && x > -6) {
        int y = noteY(nn.midi);
        bool active = ct >= st && ct < st + nn.ticks;
        u8g2.drawDisc((int)x, y, active ? 3 : 2);
        u8g2.drawVLine((int)x + (active ? 3 : 2), y - 9, 9);
      }
      st += nn.ticks;
    }
  }
  if (solved) drawSparkleField(now);
}

// ---------- Cofre ----------
int chestPhase = 0;  // 0 cerrado, 1 abriéndose, 2 texto
uint32_t chestT0 = 0;
bool chestItemSfx = false;
TypeBox chestBox;

static void updateChest(uint32_t now) {
  if (chestPhase == 0) {
    if (btnOk.shortRel) {
      chestPhase = 1;
      chestT0 = now;
      chestItemSfx = false;
      music.play(&SONG_OPEN);
    }
  } else if (chestPhase == 1) {
    uint32_t t = now - chestT0;
    if (t >= 900 && !chestItemSfx) {
      chestItemSfx = true;
      music.play(&SONG_ITEM);
    }
    if (t > 2300) {
      chestPhase = 2;
      chestBox.set(CHEST_MESSAGE, 116, 3);
    }
  } else {
    chestBox.tick(now);
    if (btnOk.shortRel && chestBox.ok()) chestPhase = 0;
  }
}

static void drawChestBox(float o) {
  u8g2.drawFrame(40, 34, 48, 24);
  u8g2.drawVLine(50, 34, 24);
  u8g2.drawVLine(77, 34, 24);
  u8g2.drawHLine(40, 55, 48);
  int lh = 14 - (int)(9 * o);
  int ly = 34 - lh - (int)(6 * o);
  u8g2.drawRFrame(40, ly, 48, lh + 1, lh > 6 ? 4 : 1);
  u8g2.drawVLine(50, ly, lh);
  u8g2.drawVLine(77, ly, lh);
  if (o < 0.05f) {
    u8g2.drawBox(60, 30, 8, 8);
    u8g2.setDrawColor(0);
    u8g2.drawBox(63, 32, 2, 2);
    u8g2.drawVLine(63, 34, 2);
    u8g2.setDrawColor(1);
  } else {
    u8g2.drawVLine(40, ly + lh, 34 - (ly + lh));
    u8g2.drawVLine(87, ly + lh, 34 - (ly + lh));
  }
}

static void drawRays(uint32_t now, int cx, int cy, float len) {
  for (int k = 0; k < 7; k++) {
    if (((now / 90) + k) % 3 == 0) continue;
    float a = -PI * (k + 1) / 8.0f;
    float c = cosf(a), s = sinf(a);
    u8g2.drawLine(cx + c * 7, cy + s * 7, cx + c * (7 + len), cy + s * (7 + len));
  }
}

static void drawChest(uint32_t now) {
  if (chestPhase == 0) {
    u8g2.setFont(u8g2_font_6x10_tf);
    if ((now / 600) % 2 == 0) centerText(12, "Pulsá OK");
    drawChestBox(0);
    int s = (now / 120) % 6;
    drawSparkle(34, 30, s < 3 ? s + 1 : 0);
    drawSparkle(94, 40, s >= 3 ? s - 2 : 0);
    return;
  }
  if (chestPhase == 1) {
    uint32_t t = now - chestT0;
    if (t > 250) drawRays(now, 64, 34, 34 * easeOut((t - 250) / 900.0f));
    drawChestBox(easeOut(t / 600.0f));
    if (t > 700) {
      int y = 30 - (int)(28 * easeOut((t - 700) / 1100.0f));
      u8g2.setDrawColor(0);
      u8g2.drawBox(58, y - 1, 13, 17);
      u8g2.setDrawColor(1);
      drawSprite(60, y, SPR_RUPEE, 15);
    }
    return;
  }
  drawSprite(60, 2, SPR_RUPEE, 15);
  int s = (now / 110) % 8;
  drawSparkle(48, 8, s < 4 ? s : 0);
  drawSparkle(80, 12, s >= 4 ? s - 4 : 0);
  u8g2.drawRFrame(1, 20, 126, 44, 3);
  chestBox.draw(6, 32, 11, now);
}

// ---------- Corazones ----------
int heartsFilled = 0;
uint32_t heartsLast = 0;
std::vector<String> heartsLines;

static void updateHearts(uint32_t now) {
  const int total = HEARTS_COUNT > 20 ? 20 : HEARTS_COUNT;
  if (heartsFilled < total && (int32_t)(now - heartsLast) > 170) {
    heartsLast = now;
    heartsFilled++;
    if (heartsFilled == total) music.play(&SONG_HEART, &SONG_SECRET);
    else music.play(&SONG_HEART);
  }
  if (btnOk.shortRel) {
    heartsFilled = 0;
    heartsLast = now;
  }
}

static void drawHearts(uint32_t now) {
  const int total = HEARTS_COUNT > 20 ? 20 : HEARTS_COUNT;
  u8g2.setFont(u8g2_font_5x7_tf);
  centerText(7, "- VIDA -");
  for (int i = 0; i < total; i++) {
    int row = i / 10, col = i % 10;
    int perRow = min(10, total - row * 10);
    int x = 64 - (perRow * 11 - 2) / 2 + col * 11;
    int y = 11 + row * 11;
    bool full = i < heartsFilled;
    if (full && i == heartsFilled - 1 && now - heartsLast < 120) y -= 1;
    drawSprite(x, y, full ? SPR_HEART : SPR_HEART_EMPTY, 8);
  }
  if (heartsFilled >= total) {
    u8g2.setFont(u8g2_font_6x10_tf);
    for (size_t i = 0; i < heartsLines.size() && i < 2; i++) centerText(46 + i * 11, heartsLines[i].c_str());
    drawSparkleField(now);
  }
}

// ---------- Hada (protector de pantalla) ----------
const int TRAIL = 14;
int trailX[TRAIL], trailY[TRAIL], trailHead = 0;
uint32_t heyUntil = 0, nextHey = 0;

static void updateFairy(uint32_t now) {
  if (btnOk.press || btnPrev.shortRel || btnNext.press) {
    enterScene(SC_MENU);
    return;
  }
  if (now > nextHey) {
    heyUntil = now + 1400;
    nextHey = now + 7000 + random(6000);
    music.play(&SONG_HEY);
  }
}

static void drawFairy(uint32_t now) {
  float s = now / 1000.0f;
  int x = 64 + (int)(46 * sinf(s * 0.9f) + 8 * sinf(s * 3.1f));
  int y = 32 + (int)(18 * sinf(s * 1.3f + 1) + 5 * cosf(s * 2.7f));
  trailHead = (trailHead + 1) % TRAIL;
  trailX[trailHead] = x;
  trailY[trailHead] = y;
  for (int i = 0; i < 8; i++)
    if (((now / 400) + i) % 4) u8g2.drawPixel(SPARKLES[i][0], SPARKLES[i][1]);
  for (int i = 2; i < TRAIL; i++) {
    int k = (trailHead - i + TRAIL) % TRAIL;
    if (random(TRAIL) < TRAIL - i) u8g2.drawPixel(trailX[k] + random(-1, 2), trailY[k] + random(-1, 2));
  }
  drawFairyAt(x, y, now);
  if (now < heyUntil) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char* msg = "¡Hey!";
    int w = u8g2.getUTF8Width(msg) + 6;
    int bx = constrain(x + 8, 0, 128 - w);
    int by = constrain(y - 22, 0, 64 - 13);
    u8g2.setDrawColor(0);
    u8g2.drawBox(bx, by, w, 13);
    u8g2.setDrawColor(1);
    u8g2.drawRFrame(bx, by, w, 13, 3);
    u8g2.drawUTF8(bx + 3, by + 10, msg);
  }
}

// ---------- Silencio (mantener botón izquierdo) ----------
uint32_t muteToastUntil = 0;

static void drawSpeaker(int x, int y, bool muted) {
  u8g2.drawBox(x, y + 3, 3, 4);
  u8g2.drawTriangle(x + 2, y + 5, x + 7, y, x + 7, y + 10);
  if (muted) {
    u8g2.drawLine(x + 10, y + 2, x + 16, y + 8);
    u8g2.drawLine(x + 10, y + 8, x + 16, y + 2);
  } else {
    u8g2.drawCircle(x + 7, y + 5, 4, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
    u8g2.drawCircle(x + 7, y + 5, 7, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
  }
}

static void drawMuteToast() {
  const int w = 84, h = 24, x = (128 - w) / 2, y = 20;
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(x, y, w, h, 3);
  drawSpeaker(x + 6, y + 7, music.muted());
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawUTF8(x + 28, y + 16, music.muted() ? "Silencio" : "Sonido");
}

// ----------------------------------------------------------------------------
// Máquina de escenas
// ----------------------------------------------------------------------------
void enterScene(Scene s) {
  uint32_t now = millis();
  scene = s;
  sceneStart = now;
  lastInput = now;
  switch (s) {
    case SC_OCA_PLAY:
      ocaDoneAt = 0;
      music.play(OCARINA_SONGS[ocaSel], &SONG_SECRET);
      break;
    case SC_CHEST: chestPhase = 0; break;
    case SC_HEARTS:
      heartsFilled = 0;
      heartsLast = now + 300;
      break;
    case SC_FAIRY:
      nextHey = now + 2500;
      heyUntil = 0;
      for (int i = 0; i < TRAIL; i++) trailX[i] = trailY[i] = -10;
      break;
    case SC_REMINDERS: refreshReminderList(); break;
    case SC_NOTIFY: startNextNotification(now); break;
    default: break;
  }
}

void setup() {
  Serial.begin(115200);
  btnPrev.begin();
  btnOk.begin();
  btnNext.begin();
  music.begin(PIN_BUZZER);
  prefs.begin("zelda", false);
  music.setMuted(prefs.getBool("muted", false));
  reminders.begin();

  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setFontMode(1);

  for (int i = 0; i < OCARINA_SONG_COUNT; i++) ocaNames[i] = OCARINA_SONGS[i]->name;
  u8g2.setFont(u8g2_font_6x10_tf);
  heartsLines = wrapText(HEARTS_MESSAGE, 124);

  // Pantalla mientras se conecta (puede tardar unos segundos)
  u8g2.clearBuffer();
  drawTriforce(64, 6, 20);
  u8g2.setFont(u8g2_font_6x10_tf);
  centerText(60, "Conectando...");
  u8g2.sendBuffer();
  netBegin();

  randomSeed(esp_random());
  enterScene(SC_BOOT);
}

void loop() {
  uint32_t now = millis();
  netLoop();
  btnPrev.poll(now);
  btnOk.poll(now);
  btnNext.poll(now);
  if (btnPrev.press || btnOk.press || btnNext.press) lastInput = now;

  // Mantener izquierda = silenciar / activar sonido (se guarda)
  if (btnPrev.longPress) {
    music.setMuted(!music.muted());
    prefs.putBool("muted", music.muted());
    muteToastUntil = now + 1200;
  }
  // Mantener OK = volver al reloj
  if (btnOk.longPress && scene != SC_BOOT && scene != SC_CLOCK && scene != SC_NOTIFY) {
    music.stop();
    enterScene(SC_CLOCK);
    btnOk.shortRel = false;
  }

  // ¿Toca algún recordatorio?
  tm t;
  String due;
  if (localTime(t) && reminders.check(t, due)) pushNotification(due);
  if (!pendingNotes.empty() && scene != SC_NOTIFY && scene != SC_BOOT) {
    enterScene(SC_NOTIFY);
    btnOk.shortRel = false;
  }

  uint32_t st = now - sceneStart;
  switch (scene) {
    case SC_BOOT: updateBoot(now, st); break;
    case SC_CLOCK: updateClock(now); break;
    case SC_MENU: updateMenu(now); break;
    case SC_REMINDERS: updateReminders(now); break;
    case SC_NET: updateNet(now); break;
    case SC_NOTIFY: updateNotify(now); break;
    case SC_OCA_LIST: updateOcaList(now); break;
    case SC_OCA_PLAY: updateOcaPlay(now); break;
    case SC_CHEST: updateChest(now); break;
    case SC_HEARTS: updateHearts(now); break;
    case SC_FAIRY: updateFairy(now); break;
  }

  // Sin tocar nada por un rato → vuelve al reloj
  bool idleable = scene == SC_MENU || scene == SC_REMINDERS || scene == SC_NET ||
                  scene == SC_OCA_LIST || scene == SC_HEARTS || (scene == SC_CHEST && chestPhase == 0);
  if (idleable && now - lastInput > IDLE_TO_CLOCK_MS) enterScene(SC_CLOCK);

  st = now - sceneStart;
  u8g2.clearBuffer();
  switch (scene) {
    case SC_BOOT: drawBoot(now, st); break;
    case SC_CLOCK: drawClock(now); break;
    case SC_MENU: drawList("Hyrule", MENU_ITEMS, MENU_COUNT, menuSel, menuScroll); break;
    case SC_REMINDERS: drawReminders(now); break;
    case SC_NET: drawNet(now); break;
    case SC_NOTIFY: drawNotify(now); break;
    case SC_OCA_LIST: drawList("Ocarina", ocaNames, OCARINA_SONG_COUNT, ocaSel, ocaScroll); break;
    case SC_OCA_PLAY: drawOcaPlay(now); break;
    case SC_CHEST: drawChest(now); break;
    case SC_HEARTS: drawHearts(now); break;
    case SC_FAIRY: drawFairy(now); break;
  }
  if ((int32_t)(muteToastUntil - now) > 0) drawMuteToast();
  u8g2.sendBuffer();
}
#endif
