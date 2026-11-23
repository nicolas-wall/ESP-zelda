#pragma once
#include <Arduino.h>
#include "songs.h"

inline uint16_t midiToFreq(uint8_t m) {
  return (uint16_t)(440.0f * powf(2.0f, (m - 69) / 12.0f) + 0.5f);
}

// Reproductor que corre en su propia tarea de FreeRTOS (core 0), así la
// música no se traba mientras se dibujan las animaciones.
class MusicPlayer {
 public:
  void begin(uint8_t pin) {
    pin_ = pin;
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    xTaskCreatePinnedToCore(taskFn, "music", 4096, this, 2, nullptr, 0);
  }

  // Toca `s` y, cuando termina, encadena `then` (opcional).
  void play(const Song* s, const Song* then = nullptr) {
    reqNext_ = then;
    req_ = s;
    reqFlag_ = true;
  }
  void stop() { play(nullptr); }

  bool playing() const { return reqFlag_ || cur_ != nullptr; }
  const Song* song() const { return cur_; }
  uint8_t currentMidi() const { return sounding_ ? midi_ : 0; }
  float tick() const {
    const Song* s = cur_;
    if (!s) return 0;
    return (millis() - start_) / (float)s->msPerTick;
  }

 private:
  static void taskFn(void* arg) {
    auto* self = static_cast<MusicPlayer*>(arg);
    for (;;) {
      self->update();
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  void silence() {
    noTone(pin_);
    sounding_ = false;
  }

  void start(const Song* s, const Song* then, uint32_t now) {
    silence();
    next_ = then;
    idx_ = 0;
    start_ = now;
    noteEnd_ = now;
    soundEnd_ = now;
    cur_ = s;
  }

  void update() {
    uint32_t now = millis();
    if (reqFlag_) {
      reqFlag_ = false;
      start(req_, reqNext_, now);
    }
    const Song* s = cur_;
    if (!s) return;

    if (sounding_ && (int32_t)(now - soundEnd_) >= 0) silence();
    if ((int32_t)(now - noteEnd_) < 0) return;

    if (idx_ >= s->count) {
      const Song* n = next_;
      silence();
      cur_ = nullptr;
      if (n) start(n, nullptr, now);
      return;
    }

    const Note& n = s->notes[idx_++];
    uint32_t dur = (uint32_t)n.ticks * s->msPerTick;
    noteEnd_ += dur;  // relativo a la nota anterior: sin deriva de tempo
    if (n.midi) {
      uint32_t gap = dur > 80 ? 18 : 6;  // pequeño silencio entre notas
      midi_ = n.midi;
      tone(pin_, midiToFreq(n.midi));
      sounding_ = true;
      soundEnd_ = noteEnd_ - gap;
    } else {
      silence();
    }
  }

  uint8_t pin_ = 0;
  volatile bool reqFlag_ = false;
  const Song* volatile req_ = nullptr;
  const Song* volatile reqNext_ = nullptr;
  const Song* volatile cur_ = nullptr;
  const Song* next_ = nullptr;
  volatile uint32_t start_ = 0;
  uint32_t noteEnd_ = 0, soundEnd_ = 0;
  uint16_t idx_ = 0;
  volatile bool sounding_ = false;
  volatile uint8_t midi_ = 0;
};
