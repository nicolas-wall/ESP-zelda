#pragma once
#include <stdint.h>

// Una nota: número MIDI (0 = silencio) + duración en "ticks" de la canción.
struct Note {
  uint8_t midi;
  uint8_t ticks;
};

struct Song {
  const char* name;
  const Note* notes;
  uint16_t count;
  uint16_t msPerTick;
  const char* ocarina;  // botones de ocarina: 'A' 'v' '>' '<' '^'  ("" = ninguno)
};

// Nombres de notas en minúscula (para no chocar con A0..A5 de Arduino)
namespace nt {
enum : uint8_t {
  r = 0,
  c4 = 60, cs4, d4, ds4, e4, f4, fs4, g4, gs4, a4, as4, b4,
  c5, cs5, d5, ds5, e5, f5, fs5, g5, gs5, a5, as5, b5,
  c6, cs6, d6, ds6, e6
};
}

#define SONG(var, title, ms, oca, ...)                                   \
  static const Note var##_notes[] = {__VA_ARGS__};                        \
  static const Song var = {title, var##_notes,                            \
                           sizeof(var##_notes) / sizeof(Note), ms, oca};

using namespace nt;

// ------------------------- Canciones de ocarina -------------------------

SONG(SONG_LULLABY, "Canción de Zelda", 230, "<^><^>",
  {b4,4},{d5,2},{a4,4},{g4,1},{a4,1},{b4,4},{d5,2},{a4,6},
  {b4,4},{d5,2},{a5,4},{g5,2},{d5,4},{c5,1},{b4,1},{a4,6},
  {b4,4},{d5,2},{a4,4},{g4,1},{a4,1},{b4,4},{d5,2},{a4,6},
  {b4,4},{d5,2},{a5,4},{g5,2},{d6,12})

SONG(SONG_EPONA, "Canción de Epona", 190, "^<>^<>",
  {d5,1},{b4,1},{a4,4},{d5,1},{b4,1},{a4,4},
  {d5,1},{b4,1},{a4,2},{b4,2},{a4,6},
  {d5,1},{b4,1},{a4,4},{d5,1},{b4,1},{a4,4},
  {d5,1},{b4,1},{a4,2},{b4,2},{a4,6})

SONG(SONG_SARIA, "Canción de Saria", 150, "v><v><",
  {f4,1},{a4,1},{b4,2},{f4,1},{a4,1},{b4,2},
  {f4,1},{a4,1},{b4,1},{e5,1},{d5,2},{b4,1},{c5,1},
  {b4,1},{g4,1},{e4,5},{r,1},{d4,1},{e4,1},{g4,1},{e4,5},{r,2},
  {f4,1},{a4,1},{b4,2},{f4,1},{a4,1},{b4,2},
  {f4,1},{a4,1},{b4,1},{e5,1},{d5,2},{b4,1},{c5,1},
  {e5,1},{b4,1},{g4,5},{r,1},{b4,1},{g4,1},{d4,1},{e4,6})

SONG(SONG_STORMS, "Canción de la Tormenta", 140, "Av^Av^",
  {d4,1},{f4,1},{d5,4},{d4,1},{f4,1},{d5,4},
  {e5,3},{f5,1},{e5,1},{f5,1},{e5,1},{c5,1},{a4,4},
  {a4,2},{d4,2},{f4,1},{g4,1},{a4,6},
  {a4,2},{d4,2},{f4,1},{g4,1},{e4,6},
  {d4,1},{f4,1},{d5,4},{d4,1},{f4,1},{d5,4},
  {e5,3},{f5,1},{e5,1},{f5,1},{e5,1},{c5,1},{a4,4},
  {a4,2},{d4,2},{f4,1},{g4,1},{a4,4},{a4,2},{d4,6})

SONG(SONG_TIME, "Canción del Tiempo", 240, ">Av>Av",
  {a4,2},{d4,3},{f4,1},{a4,2},{d4,3},{f4,1},
  {a4,1},{c5,1},{b4,2},{g4,2},{f4,1},{g4,1},{a4,2},{d4,2},
  {c4,1},{e4,1},{d4,6},
  {a4,2},{d4,3},{f4,1},{a4,2},{d4,3},{f4,1},
  {a4,1},{c5,1},{b4,2},{g4,2},{d5,4},{r,2},{c5,6})

SONG(SONG_OVERWORLD, "Tema de Hyrule", 95, "",
  {as4,8},{f4,6},{as4,2},
  {as4,1},{c5,1},{d5,1},{ds5,1},{f5,12},
  {f5,2},{f5,2},{fs5,2},{gs5,2},{as5,8},
  {as5,2},{as5,2},{gs5,2},{fs5,2},{gs5,3},{fs5,1},{f5,12},{r,4})

// ------------------------------ Jingles ------------------------------

SONG(SONG_SECRET, "¡Correcto!", 110, "",
  {g5,1},{fs5,1},{ds5,1},{a4,1},{gs4,1},{e5,1},{gs5,1},{c6,6})

SONG(SONG_ITEM, "Item", 140, "",
  {a4,1},{as4,1},{b4,1},{c5,8})

SONG(SONG_OPEN, "Abrir", 60, "",
  {c5,1},{e5,1},{g5,1},{c6,2})

SONG(SONG_HEART, "Corazón", 40, "",
  {a5,1},{e6,2})

SONG(SONG_BLIP, "Blip", 14, "",
  {e6,1})

SONG(SONG_HEY, "Hey", 70, "",
  {d6,1},{r,1},{a5,1},{d6,2})

static const Song* const OCARINA_SONGS[] = {
  &SONG_LULLABY, &SONG_EPONA, &SONG_SARIA, &SONG_STORMS, &SONG_TIME, &SONG_OVERWORLD,
};
static const int OCARINA_SONG_COUNT = sizeof(OCARINA_SONGS) / sizeof(OCARINA_SONGS[0]);
