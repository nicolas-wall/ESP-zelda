#pragma once

// =====================================================================
//  PERSONALIZÁ TU REGALO ACÁ
// =====================================================================

// Mensaje que aparece al abrir el cofre.
// Se acomoda solo en la caja de texto; usá \n para forzar un salto de línea.
// Si es largo se divide en varias páginas (OK pasa de página).
#define CHEST_MESSAGE   "¡Encontraste una rupia verde!\n" \
                        "Es poco, pero todo héroe empieza así. " \
                        "Seguí explorando."

// Pantalla de corazones
#define HEARTS_COUNT    20                 // máximo 20 (dos filas de 10)
#define HEARTS_MESSAGE  "¡Corazón lleno de cariño!"

// Tiempo sin tocar nada antes de volver al reloj (ms)
#define IDLE_TO_CLOCK_MS 60000

// =====================================================================
//  RELOJ Y RECORDATORIOS
// =====================================================================

// Zona horaria en formato POSIX. Ejemplos:
//   Argentina / Uruguay:  "<-03>3"
//   Chile:                "<-04>4<-03>,M9.1.6/24,M4.1.6/24"
//   México (CDMX):        "CST6"
//   Colombia / Perú:      "<-05>5"
//   España:               "CET-1CEST,M3.5.0,M10.5.0/3"
#define TIMEZONE        "<-03>3"

// Nombre de la red WiFi que crea el reloj la primera vez para configurarlo
#define WIFI_SETUP_AP   "Hyrule-Setup"

// Cada cuántos segundos el hada vuelve a llamar si no atendés el aviso,
// y cuántas veces como máximo.
#define NOTIFY_REPEAT_S 30
#define NOTIFY_REPEATS  3

// Tema de Hyrule en cada hora en punto: 1 = sí, 0 = no.
// Solo suena entre HYRULE_HOURLY_FROM y HYRULE_HOURLY_TO (inclusive).
#define HYRULE_HOURLY       0
#define HYRULE_HOURLY_FROM  9
#define HYRULE_HOURLY_TO    21

// =====================================================================
//  HARDWARE
// =====================================================================

// Tipo de OLED:
//   1 = SSD1309 (las de 1.54" como "OLED_M154", la tuya)
//   2 = SSD1306 (las típicas de 0.96")
//   3 = SH1106  (las típicas de 1.3")
// Si la pantalla queda en negro o se ve corrida, probá con otro valor.
#define OLED_TYPE       1

// Pines (nombres de la XIAO ESP32-S3)
#define PIN_OLED_SDA    D4
#define PIN_OLED_SCL    D5
#define PIN_OLED_RES    U8X8_PIN_NONE   // en tu placa RES no va a ningún GPIO
#define PIN_BUZZER      D3
#define PIN_BTN_PREV    D0   // botón izquierdo  (anterior)
#define PIN_BTN_OK      D8   // botón del medio  (OK / mantener = volver)
#define PIN_BTN_NEXT    D2   // botón derecho    (siguiente)

// 1 = la otra pata de los botones va a GND, 0 = va a 3V3
#define BTN_TO_GND      1
