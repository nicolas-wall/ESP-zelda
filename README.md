# Reloj de Hyrule

Reloj de escritorio estilo Zelda con recordatorios que cargás desde el teléfono. Funciona con una **XIAO ESP32-S3**, una **OLED de 128x64** y un **buzzer pasivo**.

- **Reloj:** arriba, el momento del día ("Mañana", "Atardecer", "Noche") entre dos Trifuerzas. En el centro, la hora dentro de un marco, con una Trifuerza como separador y estrellas de noche. Abajo, la fecha con un sol o una luna, y una campanita 🔔 si hay recordatorios, que titila cuando falta poco. Cada 3 minutos un hada cruza la pantalla.
- **Recordatorios desde el teléfono:** el reloj sirve una página web en tu WiFi. Ahí cargás recordatorios para una vez, todos los días o de lunes a viernes, y también podés mandar un mensaje al instante.
- **Avisos en la cueva:** cuando toca un recordatorio aparece un sabio entre dos fuegos, con una espada, y el mensaje se escribe en mayúsculas letra por letra. Si no lo atendés, vuelve a llamar cada 30 s, hasta 3 veces.
- **Extras del menú:** ocarina con 6 canciones, cofre con mensaje secreto, corazones y el hada como protector de pantalla.

## Usar desde el teléfono

1. **Primera vez, si no tiene WiFi guardado:** desde el teléfono conectate a la red **Hyrule-Setup** y elegí tu WiFi en la página que se abre.
2. En el reloj, entrá a **Menú → Conexión** y vas a ver la dirección, por ejemplo `http://192.168.100.52`.
3. Abrí esa dirección en el navegador del teléfono, que tiene que estar en el mismo WiFi. Te conviene agregarla a la pantalla de inicio.

**Atajo:** abrir `http://<ip>/msg?text=Hola` manda el mensaje al instante. Sirve para crear un atajo en iOS (Atajos) o en Android (HTTP Shortcuts).

> La página no tiene contraseña: cualquiera en tu red WiFi puede cargar recordatorios.

## Controles

| Pantalla | Izquierda | OK (medio) | Derecha |
|---|---|---|---|
| Reloj | abrir menú | abrir menú | abrir menú |
| Listas | subir | elegir | bajar |
| Aviso | — | leer / cerrar | — |

- **Mantener izquierda:** silenciar o activar el sonido. Se guarda, y en el reloj aparece una ✕ abajo, junto a la campanita.
- **Mantener OK:** volver al reloj.
- **Mantener derecha** en *Conexión*: borrar el WiFi guardado y volver a configurarlo.
- Después de 1 minuto sin tocar nada, vuelve solo al reloj.

## Conexiones

| Componente | Pin del módulo | XIAO ESP32-S3 |
|---|---|---|
| OLED | GND / VCC | GND / 3V3 |
| OLED | SCL / SDA | **D5 / D4** |
| OLED | RES | sin conectar a GPIO |
| Buzzer HW-508 | S / medio / − | **D3** / 3V3 / GND |
| Botón izquierdo | entre pin y GND | **D0** |
| Botón del medio (OK) | entre pin y GND | **D8** |
| Botón derecho | entre pin y GND | **D2** |

Si tu cableado es distinto, usá el detector: `pio run -e pinfinder -t upload` encuentra la OLED y los botones. Para el buzzer, `pio run -e buzztest -t upload` muestra en pantalla el pin que está sonando.

## Personalizar

En [`include/config.h`](include/config.h) podés cambiar:

- **`TIMEZONE`:** la zona horaria. Viene configurada para Argentina, y el archivo trae ejemplos de otros países.
- **`HOURLY_CHIME`:** un jingle en cada hora en punto. Viene apagado.
- **`NOTIFY_REPEAT_S` / `NOTIFY_REPEATS`:** cada cuánto y cuántas veces insiste el hada.
- **`CHEST_MESSAGE` y `HEARTS_*`:** los textos del cofre y de los corazones del menú.
- **`OLED_TYPE`:** el modelo de pantalla. 1 = SSD1309, 2 = SSD1306, 3 = SH1106.

## Compilar y subir

```bash
pio run -e xiao_esp32s3 -t upload
```

Si no aparece el puerto, mantené apretado el botón **B** (BOOT) de la XIAO mientras la conectás.

---
Proyecto hecho por un fan, sin relación con Nintendo. The Legend of Zelda es una marca de Nintendo.
