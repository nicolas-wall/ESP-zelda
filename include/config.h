#pragma once

// =====================================================================
//  PERSONALIZÁ TU REGALO ACÁ
// =====================================================================

// Pantalla de título (después de la animación de la Trifuerza)
#define GIFT_TITLE      "¡Feliz cumple!"
#define GIFT_NAME       "Héroe"            // <- poné el nombre de la persona

// Mensaje que aparece al abrir el cofre.
// Se acomoda solo en la caja de texto; usá \n para forzar un salto de línea.
// Si es largo se divide en varias páginas (OK pasa de página).
#define CHEST_MESSAGE   "¡Encontraste un regalo!\n" \
                        "Que este año esté lleno de aventuras, cofres y rupias. " \
                        "¡Feliz cumpleaños!"

// Pantalla de corazones
#define HEARTS_COUNT    20                 // máximo 20 (dos filas de 10)
#define HEARTS_MESSAGE  "¡Corazón lleno de cariño!"

// Tiempo sin tocar nada en el menú antes de que aparezca el hada (ms)
#define IDLE_TO_FAIRY_MS 60000

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
#define PIN_OLED_RES    D3
#define PIN_BUZZER      D2
#define PIN_BTN_PREV    D0   // botón izquierdo  (anterior)
#define PIN_BTN_OK      D1   // botón del medio  (OK / mantener = volver)
#define PIN_BTN_NEXT    D8   // botón derecho    (siguiente)

// 1 = la otra pata de los botones va a GND, 0 = va a 3V3
#define BTN_TO_GND      1
