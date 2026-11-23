#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

enum Repeat : uint8_t { REP_ONCE = 0, REP_DAILY = 1, REP_WEEKDAYS = 2 };

struct Reminder {
  uint8_t used, hour, minute, repeat;
  uint16_t lastDay;  // día en que ya sonó (para no repetirlo en el mismo minuto)
  char text[60];
};

const int MAX_REMINDERS = 12;

inline const char* repeatName(uint8_t r) {
  switch (r) {
    case REP_DAILY: return "todos los días";
    case REP_WEEKDAYS: return "lun a vie";
  }
  return "una vez";
}

// Copia UTF-8 sin cortar un carácter a la mitad.
inline void copyUtf8(char* dst, size_t cap, const String& src) {
  size_t n = src.length();
  if (n > cap - 1) {
    n = cap - 1;
    while (n > 0 && ((uint8_t)src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src.c_str(), n);
  dst[n] = 0;
}

class Reminders {
 public:
  Reminder items[MAX_REMINDERS];

  void begin() {
    prefs_.begin("remind", false);
    memset(items, 0, sizeof(items));
    if (prefs_.getBytesLength("list") == sizeof(items)) prefs_.getBytes("list", items, sizeof(items));
  }

  bool add(uint8_t h, uint8_t m, uint8_t rep, const String& text) {
    for (auto& r : items) {
      if (r.used) continue;
      r.used = 1;
      r.hour = h;
      r.minute = m;
      r.repeat = rep;
      r.lastDay = 0;
      copyUtf8(r.text, sizeof(r.text), text);
      save();
      return true;
    }
    return false;
  }

  void remove(int i) {
    if (i < 0 || i >= MAX_REMINDERS) return;
    items[i].used = 0;
    save();
  }

  int count() const {
    int n = 0;
    for (auto& r : items) n += r.used;
    return n;
  }

  // Si algún recordatorio toca ahora, lo devuelve en `out` (uno por llamada).
  bool check(const tm& t, String& out) {
    uint16_t day = dayKey(t);
    for (auto& r : items) {
      if (!r.used || r.hour != t.tm_hour || r.minute != t.tm_min || r.lastDay == day) continue;
      if (!matches(r.repeat, t.tm_wday)) continue;
      r.lastDay = day;
      out = r.text;
      if (r.repeat == REP_ONCE) r.used = 0;
      save();
      return true;
    }
    return false;
  }

  // Minutos hasta la próxima vez que suena (-1 si nunca).
  int minutesUntil(const Reminder& r, const tm& t) const {
    if (!r.used) return -1;
    int nowMin = t.tm_hour * 60 + t.tm_min;
    for (int d = 0; d <= 7; d++) {
      if (!matches(r.repeat, (t.tm_wday + d) % 7)) continue;
      int mins = d * 1440 + r.hour * 60 + r.minute - nowMin;
      if (mins < 0 || (mins == 0 && r.lastDay == dayKey(t))) continue;
      return mins;
    }
    return -1;
  }

  // Índices de los recordatorios ordenados por cuál suena primero.
  int sorted(const tm& t, int* idx) const {
    int n = 0;
    for (int i = 0; i < MAX_REMINDERS; i++)
      if (items[i].used) idx[n++] = i;
    for (int a = 0; a < n; a++)
      for (int b = a + 1; b < n; b++) {
        int ma = minutesUntil(items[idx[a]], t), mb = minutesUntil(items[idx[b]], t);
        if (ma < 0) ma = 1 << 20;
        if (mb < 0) mb = 1 << 20;
        if (mb < ma) std::swap(idx[a], idx[b]);
      }
    return n;
  }

 private:
  static uint16_t dayKey(const tm& t) { return (uint16_t)(t.tm_year * 366 + t.tm_yday); }
  static bool matches(uint8_t rep, int wday) {
    return rep != REP_WEEKDAYS || (wday >= 1 && wday <= 5);
  }
  void save() { prefs_.putBytes("list", items, sizeof(items)); }
  Preferences prefs_;
};
