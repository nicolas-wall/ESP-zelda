// Cofre de Hyrule — un regalo para fans de Zelda
// XIAO ESP32-S3 + OLED 128x64 I2C + buzzer pasivo + 3 botones

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <vector>

#include "config.h"
#include "music.h"
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

struct Button {
  uint8_t pin;
  bool down = false, longFired = false;
  uint32_t changed = 0, pressedAt = 0;
  bool press = false, shortRel = false, longPress = false;  // eventos de este frame

  explicit Button(uint8_t p) : pin(p) {}
  void begin() { pinMode(pin, INPUT_PULLUP); }
  void poll(uint32_t now) {
    press = shortRel = longPress = false;
    bool raw = digitalRead(pin) == LOW;
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
// dx/dy desplazan cada pieza (0 = arriba, 1 = abajo-izq, 2 = abajo-der).
static void drawTriforce(int cx, int ty, int h, const int* dx = nullptr, const int* dy = nullptr) {
  int hb = (h * 577) / 1000;  // media base de un triángulo equilátero
  int ox[3] = {0, 0, 0}, oy[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    if (dx) ox[i] = dx[i];
    if (dy) oy[i] = dy[i];
  }
  u8g2.drawTriangle(cx + ox[0], ty + oy[0],
                    cx - hb + ox[0], ty + h + oy[0],
                    cx + hb + ox[0], ty + h + oy[0]);
  u8g2.drawTriangle(cx - hb + ox[1], ty + h + oy[1],
                    cx - 2 * hb + ox[1], ty + 2 * h + oy[1],
                    cx + ox[1], ty + 2 * h + oy[1]);
  u8g2.drawTriangle(cx + hb + ox[2], ty + h + oy[2],
                    cx + ox[2], ty + 2 * h + oy[2],
                    cx + 2 * hb + ox[2], ty + 2 * h + oy[2]);
}

// Corta un texto en líneas que entren en `width` píxeles (usa la fuente actual).
static std::vector<String> wrapText(const char* text, int width) {
  std::vector<String> lines;
  String para;
  String src(text);
  src += '\n';
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

// ----------------------------------------------------------------------------
// Escenas
// ----------------------------------------------------------------------------
enum Scene { SC_BOOT, SC_MENU, SC_OCA_LIST, SC_OCA_PLAY, SC_CHEST, SC_HEARTS, SC_FAIRY };
Scene scene = SC_BOOT;
uint32_t sceneStart = 0, lastInput = 0;

void enterScene(Scene s);

// ---------- Arranque: la Trifuerza se arma + pantalla de título ----------
const uint32_t BOOT_FLY = 1800, BOOT_FLASH = 700;
bool bootChime = false;

static const int SPARKLES[][2] = {{8, 8},   {119, 10}, {16, 34}, {111, 36},
                                  {28, 18}, {100, 22}, {6, 56},  {121, 56}};

static void drawSparkleField(uint32_t t) {
  for (int i = 0; i < 8; i++) {
    int phase = ((t + i * 170) / 110) % 9;
    int size = phase < 3 ? phase + 1 : (phase < 5 ? 5 - phase : 0);
    drawSparkle(SPARKLES[i][0], SPARKLES[i][1], size);
  }
}

static void updateBoot(uint32_t now, uint32_t t) {
  if (t >= BOOT_FLY && !bootChime) {
    bootChime = true;
    music.play(&SONG_SECRET, &SONG_LULLABY);
  }
  if (t > BOOT_FLY && (btnOk.shortRel || btnPrev.press || btnNext.press)) enterScene(SC_MENU);
}

static void drawBoot(uint32_t now, uint32_t t) {
  if (t < BOOT_FLY + BOOT_FLASH) {
    float e = easeOut((float)t / BOOT_FLY);
    int dx[3] = {0, (int)(-90 * (1 - e)), (int)(90 * (1 - e))};
    int dy[3] = {(int)(-70 * (1 - e)), (int)(40 * (1 - e)), (int)(40 * (1 - e))};
    drawTriforce(64, 6, 26, dx, dy);
    if (t > BOOT_FLY && ((t - BOOT_FLY) / 90) % 2 == 0) {
      u8g2.setDrawColor(2);  // XOR = destello
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
    centerText(56, GIFT_NAME);
  }
  if (tt > 1800 && (tt / 500) % 2 == 0) {
    u8g2.setFont(u8g2_font_5x7_tf);
    centerText(64, "- OK -");
  }
}

// ---------- Listas (menú principal y canciones) ----------
static void drawList(const char* title, const char* const* items, int n, int sel, int& scroll) {
  const int rows = 4, rowH = 12, y0 = 14;
  u8g2.setFont(u8g2_font_6x10_tf);
  drawTriforce(6, 1, 5);
  drawTriforce(121, 1, 5);
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

static const char* const MENU_ITEMS[] = {"Ocarina", "Abrir el cofre", "Corazones", "Hada"};
const int MENU_COUNT = 4;
int menuSel = 0, menuScroll = 0;

static void updateMenu(uint32_t now) {
  if (btnPrev.press) menuSel = (menuSel + MENU_COUNT - 1) % MENU_COUNT;
  if (btnNext.press) menuSel = (menuSel + 1) % MENU_COUNT;
  if (btnOk.shortRel) {
    const Scene targets[] = {SC_OCA_LIST, SC_CHEST, SC_HEARTS, SC_FAIRY};
    enterScene(targets[menuSel]);
  }
}

// ---------- Ocarina ----------
const char* ocaNames[OCARINA_SONG_COUNT];
int ocaSel = 0, ocaScroll = 0;
uint32_t ocaDoneAt = 0;

static void updateOcaList(uint32_t now) {
  if (btnPrev.press) ocaSel = (ocaSel + OCARINA_SONG_COUNT - 1) % OCARINA_SONG_COUNT;
  if (btnNext.press) ocaSel = (ocaSel + 1) % OCARINA_SONG_COUNT;
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

// Posición vertical de una nota sobre el pentagrama (Mi4 = línea de abajo).
static int noteY(uint8_t midi) {
  static const uint8_t dia[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
  int step = (midi / 12) * 7 + dia[midi % 12];
  return 56 - 2 * (step - 37);
}

static void updateOcaPlay(uint32_t now) {
  if (btnOk.shortRel) {  // cortar
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

  // Pentagrama con las notas pasando por el cabezal
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
uint32_t chestT0 = 0, lastType = 0;
bool chestItemSfx = false;
std::vector<String> chestLines;
int chestPage = 0, chestTyped = 0;
const int CHEST_LINES_PER_PAGE = 3;

static int chestPageCount() {
  return (chestLines.size() + CHEST_LINES_PER_PAGE - 1) / CHEST_LINES_PER_PAGE;
}

static int chestPageBytes() {
  int total = 0;
  for (int i = 0; i < CHEST_LINES_PER_PAGE; i++) {
    int li = chestPage * CHEST_LINES_PER_PAGE + i;
    if (li < (int)chestLines.size()) total += chestLines[li].length();
  }
  return total;
}

static uint8_t chestByteAt(int k) {
  for (int i = 0; i < CHEST_LINES_PER_PAGE; i++) {
    int li = chestPage * CHEST_LINES_PER_PAGE + i;
    if (li >= (int)chestLines.size()) break;
    int len = chestLines[li].length();
    if (k < len) return (uint8_t)chestLines[li][k];
    k -= len;
  }
  return 0;
}

static void chestAdvanceChar() {
  int total = chestPageBytes();
  if (chestTyped >= total) return;
  chestTyped++;
  while (chestTyped < total && (chestByteAt(chestTyped) & 0xC0) == 0x80) chestTyped++;
}

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
      chestPage = 0;
      chestTyped = 0;
      lastType = now;
    }
  } else {
    if (now - lastType > 35) {
      lastType = now;
      chestAdvanceChar();
    }
    if (btnOk.shortRel) {
      if (chestTyped < chestPageBytes()) {
        chestTyped = chestPageBytes();
      } else if (chestPage + 1 < chestPageCount()) {
        chestPage++;
        chestTyped = 0;
      } else {
        chestPhase = 0;
      }
    }
  }
}

static void drawChestBox(float o) {
  // cuerpo
  u8g2.drawFrame(40, 34, 48, 24);
  u8g2.drawVLine(50, 34, 24);
  u8g2.drawVLine(77, 34, 24);
  u8g2.drawHLine(40, 55, 48);
  // tapa
  int lh = 14 - (int)(9 * o);
  int ly = 34 - lh - (int)(6 * o);
  u8g2.drawRFrame(40, ly, 48, lh + 1, lh > 6 ? 4 : 1);
  u8g2.drawVLine(50, ly, lh);
  u8g2.drawVLine(77, ly, lh);
  if (o < 0.05f) {
    // cerradura
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
    if (((now / 90) + k) % 3 == 0) continue;  // parpadeo
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
  // caja de texto
  drawSprite(60, 2, SPR_RUPEE, 15);
  int s = (now / 110) % 8;
  drawSparkle(48, 8, s < 4 ? s : 0);
  drawSparkle(80, 12, s >= 4 ? s - 4 : 0);
  u8g2.drawRFrame(1, 20, 126, 44, 3);
  u8g2.setFont(u8g2_font_6x10_tf);
  int remaining = chestTyped;
  for (int i = 0; i < CHEST_LINES_PER_PAGE; i++) {
    int li = chestPage * CHEST_LINES_PER_PAGE + i;
    if (li >= (int)chestLines.size() || remaining <= 0) break;
    const String& line = chestLines[li];
    int nbytes = min(remaining, (int)line.length());
    u8g2.drawUTF8(6, 32 + i * 11, line.substring(0, nbytes).c_str());
    remaining -= nbytes;
  }
  if (chestTyped >= chestPageBytes() && (now / 300) % 2 == 0)
    u8g2.drawTriangle(116, 57, 123, 57, 119, 61);
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
    int x0 = 64 - (perRow * 11 - 2) / 2;
    int x = x0 + col * 11;
    int y = 11 + row * 11;
    bool full = i < heartsFilled;
    // el último corazón lleno "late"
    if (full && i == heartsFilled - 1 && now - heartsLast < 120) y -= 1;
    drawSprite(x, y, full ? SPR_HEART : SPR_HEART_EMPTY, 8);
  }
  if (heartsFilled >= total) {
    u8g2.setFont(u8g2_font_6x10_tf);
    for (size_t i = 0; i < heartsLines.size() && i < 2; i++)
      centerText(46 + i * 11, heartsLines[i].c_str());
    drawSparkleField(now);
  }
}

// ---------- Hada (protector de pantalla) ----------
const int TRAIL = 14;
int trailX[TRAIL], trailY[TRAIL], trailHead = 0;
uint32_t heyUntil = 0, nextHey = 0;

static void updateFairy(uint32_t now) {
  if (btnOk.press || btnPrev.press || btnNext.press) {
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

  // estrellitas de fondo
  for (int i = 0; i < 8; i++) {
    if (((now / 400) + i) % 4 == 0) continue;
    u8g2.drawPixel(SPARKLES[i][0], SPARKLES[i][1]);
  }
  // estela
  for (int i = 2; i < TRAIL; i++) {
    int k = (trailHead - i + TRAIL) % TRAIL;
    if (random(TRAIL) < TRAIL - i)
      u8g2.drawPixel(trailX[k] + random(-1, 2), trailY[k] + random(-1, 2));
  }
  // cuerpo brillante + alas
  u8g2.drawDisc(x, y, 3);
  if ((now / 150) % 2) u8g2.drawCircle(x, y, 6, U8G2_DRAW_ALL);
  bool flap = (now / 70) % 2;
  u8g2.drawEllipse(x - 6, y - 3, 3, flap ? 2 : 1);
  u8g2.drawEllipse(x + 6, y - 3, 3, flap ? 2 : 1);

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
    case SC_CHEST:
      chestPhase = 0;
      break;
    case SC_HEARTS:
      heartsFilled = 0;
      heartsLast = now + 300;
      break;
    case SC_FAIRY:
      nextHey = now + 2500;
      heyUntil = 0;
      for (int i = 0; i < TRAIL; i++) trailX[i] = trailY[i] = -10;
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  btnPrev.begin();
  btnOk.begin();
  btnNext.begin();
  music.begin(PIN_BUZZER);

  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setFontMode(1);

  for (int i = 0; i < OCARINA_SONG_COUNT; i++) ocaNames[i] = OCARINA_SONGS[i]->name;

  u8g2.setFont(u8g2_font_6x10_tf);
  chestLines = wrapText(CHEST_MESSAGE, 116);
  heartsLines = wrapText(HEARTS_MESSAGE, 124);

  randomSeed(esp_random());
  enterScene(SC_BOOT);
}

void loop() {
  uint32_t now = millis();
  btnPrev.poll(now);
  btnOk.poll(now);
  btnNext.poll(now);
  if (btnPrev.press || btnOk.press || btnNext.press) lastInput = now;

  // Mantener OK = volver al menú (desde cualquier escena salvo arranque/menú)
  if (btnOk.longPress && scene != SC_BOOT && scene != SC_MENU) {
    music.stop();
    enterScene(SC_MENU);
    btnOk.shortRel = false;
  }

  uint32_t t = now - sceneStart;
  switch (scene) {
    case SC_BOOT: updateBoot(now, t); break;
    case SC_MENU: updateMenu(now); break;
    case SC_OCA_LIST: updateOcaList(now); break;
    case SC_OCA_PLAY: updateOcaPlay(now); break;
    case SC_CHEST: updateChest(now); break;
    case SC_HEARTS: updateHearts(now); break;
    case SC_FAIRY: updateFairy(now); break;
  }

  if ((scene == SC_MENU || scene == SC_OCA_LIST) && now - lastInput > IDLE_TO_FAIRY_MS)
    enterScene(SC_FAIRY);

  t = now - sceneStart;
  u8g2.clearBuffer();
  switch (scene) {
    case SC_BOOT: drawBoot(now, t); break;
    case SC_MENU: drawList("Hyrule", MENU_ITEMS, MENU_COUNT, menuSel, menuScroll); break;
    case SC_OCA_LIST: drawList("Ocarina", ocaNames, OCARINA_SONG_COUNT, ocaSel, ocaScroll); break;
    case SC_OCA_PLAY: drawOcaPlay(now); break;
    case SC_CHEST: drawChest(now); break;
    case SC_HEARTS: drawHearts(now); break;
    case SC_FAIRY: drawFairy(now); break;
  }
  u8g2.sendBuffer();
}
