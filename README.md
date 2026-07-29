# Analizador Logico Digital con Raspberry Pi

## Que es
Analizador logico de **8+ canales digitales** que usa los GPIO de una **Raspberry Pi** (2W, 3, 4, 5, Zero 2W — 32 o 64 bits) para capturar senales digitales en tiempo real y visualizarlas en una **pagina web** via WebSocket.

```
[Senales Digitales] → [GPIO Pi] → [WebSocket :8091] → [Navegador → Canvas]
```

## Contenido del proyecto

| Archivo | Que contiene |
|---------|-------------|
| `architecture.md` | Metodos de sampling (polling/edge/PIO/PIO), buffer circular, formatos de datos, comparativa por modelo Pi, consideraciones 32/64 bits |
| `setup.md` | Pin mappings detallados por modelo (Pi 2W, Pi 4, Pi 5), diagramas de conexion, divisor 5V→3.3V, instalacion de software |
| `protocol.md` | Protocolo WebSocket JSON puerto 8091 (independiente del osciloscopio), comandos, formato bitfield, triggers, capabilities |
| `firmware-pi.md` | Codigo Python listo para usar (`server_logic.py`), edge events con libgpiod, metodo C++ con mmap, tabla de rendimiento por modelo, systemd service |
| `decoders.md` | Decodificadores en JavaScript: I2C, UART, SPI, PWM, 1-Wire, Manchester — logica, configuracion, renderizado |
| `analizador_logico.md` | Prompt completo para que un AI genere TODO el codigo del proyecto |
| `hardware_pinout.md` | Pinout general de Raspberry Pi (GPIOs, I2C, SPI, UART) |
| `troubleshooting.md` | Solucion de problemas general y especifico por plataforma |

## Stack recomendado

```
Primera version (funciona en horas):
  Python + gpiod (polling) + websockets → ~200 kSps, 8 canales

Segunda version (mas eficiente):
  Python + gpiod (edge events) → ~500 kSps, 16 canales, ~30% CPU

Tercera version (max rendimiento):
  C++ + /dev/gpiomem (mmap) → ~5 MSps, 26 canales
  O Pico W + PIO → 10 MSps, 16 canales
```

## Protocolos decodificables
- **I2C** — auto-detecta START/STOP, direccion, ACK, datos
- **UART** — config baud rate, bits, parity
- **SPI** — config CPOL/CPHA, MOSI+MISO
- **PWM** — frecuencia y duty cycle
- **1-Wire** — presence pulse, ROM commands
- **Manchester/NRZ** — codificacion de linea
- **Decodificadores personalizados** — funcion JS registrable

## Arranque rapido

```bash
# En la Raspberry Pi:
sudo apt install python3 python3-pip gpiod
pip install websockets gpiod

# Copiar server_logic.py y ejecutar:
python3 server_logic.py

# Desde cualquier navegador en la red:
# abrir http://raspberrypi:8080
```
