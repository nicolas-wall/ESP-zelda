# Reloj de Hyrule

Reloj de escritorio estilo Zelda con recordatorios que cargÃ¡s desde el telÃ©fono. Funciona con una **XIAO ESP32-S3**, una **OLED de 128x64** y un **buzzer pasivo**.

- **Reloj:** el momento del día ("Mañana", "Atardecer", "Noche") entre dos Trifuerzas, la hora en un marco con una Trifuerza como separador y estrellas de noche. Abajo, la fecha con un sol o una luna, y una campanita si hay recordatorios, que titila cuando falta poco. Cada 3 minutos un hada cruza la pantalla.
- **Recordatorios desde el telÃ©fono:** el reloj sirve una pÃ¡gina web en tu WiFi. AhÃ­ cargÃ¡s recordatorios para una vez, todos los dÃ­as o de lunes a viernes, y tambiÃ©n podÃ©s mandar un mensaje al instante.
- **Avisos en la cueva:** cuando toca un recordatorio aparece un sabio entre dos fuegos con una espada, y el mensaje se escribe en mayúsculas letra por letra. Si no lo atendés, vuelve a llamar cada 30 s, hasta 3 veces.
- **Extras del menÃº:** ocarina con 6 canciones, cofre con mensaje secreto, corazones y el hada como protector de pantalla.

## Usar desde el telÃ©fono

1. **Primera vez, si no tiene WiFi guardado:** desde el telÃ©fono conectate a la red **Hyrule-Setup** y elegÃ­ tu WiFi en la pÃ¡gina que se abre.
2. En el reloj, entrÃ¡ a **MenÃº â†’ ConexiÃ³n** y vas a ver la direcciÃ³n, por ejemplo `http://192.168.100.52`.
3. AbrÃ­ esa direcciÃ³n en el navegador del telÃ©fono, que tiene que estar en el mismo WiFi. Te conviene agregarla a la pantalla de inicio.

**Atajo:** abrir `http://<ip>/msg?text=Hola` manda el mensaje al instante. Sirve para crear un atajo en iOS (Atajos) o en Android (HTTP Shortcuts).

> La pÃ¡gina no tiene contraseÃ±a: cualquiera en tu red WiFi puede cargar recordatorios.

## Controles

| Pantalla | Izquierda | OK (medio) | Derecha |
|---|---|---|---|
| Reloj | â€” | abrir menÃº | ver recordatorios |
| Listas | subir | elegir | bajar |
| Aviso del hada | â€” | leer / cerrar | â€” |

- **Mantener izquierda:** silenciar o activar el sonido. Se guarda, y en el reloj aparece una âœ• arriba a la derecha.
- **Mantener OK:** volver al reloj.
- **Mantener derecha** en *ConexiÃ³n*: borrar el WiFi guardado y volver a configurarlo.
- DespuÃ©s de 1 minuto sin tocar nada, vuelve solo al reloj.

## Conexiones

| Componente | Pin del mÃ³dulo | XIAO ESP32-S3 |
|---|---|---|
| OLED | GND / VCC | GND / 3V3 |
| OLED | SCL / SDA | **D5 / D4** |
| OLED | RES | sin conectar a GPIO |
| Buzzer HW-508 | S / medio / âˆ’ | **D3** / 3V3 / GND |
| BotÃ³n izquierdo | entre pin y GND | **D0** |
| BotÃ³n del medio (OK) | entre pin y GND | **D8** |
| BotÃ³n derecho | entre pin y GND | **D2** |

Si tu cableado es distinto, usÃ¡ el detector: `pio run -e pinfinder -t upload` encuentra la OLED y los botones. Para el buzzer, `pio run -e buzztest -t upload` muestra en pantalla el pin que estÃ¡ sonando.

## Personalizar

En [`include/config.h`](include/config.h) podÃ©s cambiar:

- **`TIMEZONE`:** la zona horaria. Viene configurada para Argentina, y el archivo trae ejemplos de otros paÃ­ses.
- **`HOURLY_CHIME`:** un jingle en cada hora en punto. Viene apagado.
- **`NOTIFY_REPEAT_S` / `NOTIFY_REPEATS`:** cada cuÃ¡nto y cuÃ¡ntas veces insiste el hada.
- **`GIFT_TITLE`, `GIFT_NAME`, `CHEST_MESSAGE` y `HEARTS_*`:** los textos de la pantalla de inicio y de los extras.
- **`OLED_TYPE`:** el modelo de pantalla. 1 = SSD1309, 2 = SSD1306, 3 = SH1106.

## Compilar y subir

```bash
pio run -e xiao_esp32s3 -t upload
```

Si no aparece el puerto, mantenÃ© apretado el botÃ³n **B** (BOOT) de la XIAO mientras la conectÃ¡s.

---
Proyecto hecho por un fan, sin relaciÃ³n con Nintendo. The Legend of Zelda es una marca de Nintendo.

