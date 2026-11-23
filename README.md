# Cofre de Hyrule

Un regalito para fans de Zelda hecho con una **XIAO ESP32-S3**, una **OLED de 128x64** y un **buzzer pasivo**.

- **Arranque:** la Trifuerza se arma en pantalla, destella, suena el jingle de "secreto" y aparece la pantalla de título con el nombre de la persona y la Canción de Zelda.
- **Ocarina:** 6 canciones. Se ven los botones de la ocarina, que se iluminan mientras suenan, y las notas pasando por un pentagrama.
- **Abrir el cofre:** el cofre se abre con rayos de luz, sube una rupia, suena la fanfarria y aparece tu mensaje escrito letra por letra.
- **Corazones:** los contenedores de corazón se llenan uno por uno, cada uno con su sonidito.
- **Hada:** un hada que revolotea con una estela de brillos y cada tanto grita "¡Hey!". Aparece sola después de 1 minuto sin tocar nada.

## Conexiones

| Componente | Pin del módulo | XIAO ESP32-S3 |
|---|---|---|
| OLED | GND | GND |
| OLED | VCC | 3V3 |
| OLED | SCL | **D5** |
| OLED | SDA | **D4** |
| OLED | RES | **D3** |
| Buzzer HW-508 | S | **D2** |
| Buzzer HW-508 | pin del medio | 3V3 |
| Buzzer HW-508 | − | GND |
| Botón izquierdo (anterior) | una pata / la otra | **D0** / GND |
| Botón del medio (OK) | una pata / la otra | **D1** / GND |
| Botón derecho (siguiente) | una pata / la otra | **D8** / GND |

Los botones usan las resistencias pull-up internas del ESP32, así que no necesitan resistencias externas. Conectá cada uno entre el pin y GND; si el botón es de 4 patas, usá dos patas en diagonal.

## Controles

- **Izquierda / derecha:** moverse por las listas.
- **OK:** elegir, abrir el cofre, pasar de página o cortar una canción.
- **Mantener OK:** volver al menú desde cualquier pantalla.

## Personalizar

Todo se cambia en [`include/config.h`](include/config.h):

- `GIFT_TITLE` y `GIFT_NAME`: el texto de la pantalla de título.
- `CHEST_MESSAGE`: el mensaje del cofre. Puede ser largo: se pagina solo, y `\n` fuerza un salto de línea.
- `HEARTS_COUNT` y `HEARTS_MESSAGE`: la cantidad de corazones (máximo 20) y el texto que aparece al llenarlos.
- `OLED_TYPE`: 1 = SSD1309 (1.54"), 2 = SSD1306 (0.96"), 3 = SH1106 (1.3").

Las canciones están en [`include/songs.h`](include/songs.h). Cada nota es `{nota, duración}`, y agregar una canción nueva es copiar un bloque `SONG(...)`.

## Compilar y subir

Con [PlatformIO](https://platformio.org/):

```bash
pio run -t upload
```

Si no aparece el puerto, mantené apretado el botón **B** (BOOT) de la XIAO mientras la conectás por USB.

## Problemas comunes

- **La pantalla queda negra:** revisá que RES esté conectado a D3 y probá otro `OLED_TYPE`.
- **El buzzer solo hace "click" o un tono fijo:** puede ser un buzzer activo, y esos no tocan melodías. El HW-508 es pasivo y funciona bien.
- **Suena muy fuerte:** poné una resistencia de 100–220 Ω en serie con el pin S.

---
Proyecto hecho por un fan, sin relación con Nintendo. The Legend of Zelda es una marca de Nintendo.
