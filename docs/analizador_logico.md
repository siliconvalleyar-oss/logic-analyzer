# PROMPT: Analizador Logico en Tiempo Real (Web)

## Objetivo
Generar un analizador logico de **8+ canales digitales** que lea pines GPIO en tiempo real desde una **Raspberry Pi Pico W o Pi 2W/4** y muestre el estado logico (0/1) de cada pin en una **pagina web** accesible desde cualquier dispositivo de la red. Los datos se transmiten por **WebSocket** y se renderizan como forma de onda digital en **Canvas nativo**.

## Arquitectura

```
[Pines GPIO] → [Raspberry Pi (C/Python)] → [WebSocket :8091] → [Web Browser → Canvas digital]
```

## Hardware

### Opcion A: Raspberry Pi Pico W (recomendada para analisis digital puro)
- Leer hasta **16 pines GPIO** simultaneamente via PIO o polling rapido
- Pines: GP0 a GP15 como entradas digitales (3.3V logic)
- Muestreo: hasta **10 MSps** con PIO (o ~100 kSps con polling)
- Buffer: 1024 muestras por canal
- Enviar por Wi-Fi WebSocket

### Opcion B: Raspberry Pi 2W / Pi 4
- Leer hasta **26 pines GPIO** (GPIO 2-27) usando `libgpiod` o `/dev/gpiochip0`
- Muestreo: hasta ~1 MSps con `libgpiod` edge detection + timestamp
- Buffer circular de 2048 muestras
- Enviar por WebSocket

### Conexion de ejemplo
```
Pin D0 (GPIO17) ← Señal digital 1 (ej: UART TX)
Pin D1 (GPIO22) ← Señal digital 2 (ej: SPI CLK)
Pin D2 (GPIO23) ← Señal digital 3 (ej: I2C SDA)
Pin D3 (GPIO24) ← Señal digital 4
Pin D4 (GPIO27) ← Señal digital 5
GND ← GND común
```

## Funcionalidades requeridas

### 1. Captura digital
- Leer estado HIGH/LOW de cada pin configurado
- Timestamp por muestra en nanosegundos
- Deteccion de flancos (rising, falling, both) para trigger
- Umbral: 0-0.8V = LOW, 2.0-3.3V = HIGH (TTL 3.3V)
- Modo sample: captura continua a tasa fija
- Modo timestamp: captura solo cuando cambia algun pin (mas eficiente)

### 2. Visualizacion web — Canvas digital
- **Fondo oscuro** (#1a1a2e o similar), lineas en colores vivos
- **Cada canal** como una fila horizontal, apilados verticalmente
- **Cada canal**: linea que oscila entre HIGH (parte superior de su fila) y LOW (parte inferior)
- **Colores por canal**: D0=#FF6B6B, D1=#4ECDC4, D2=#45B7D1, D3=#96CEB4, D4=#FFEAA7, D5=#DDA0DD, D6=#98D8C8, D7=#F7DC6F, etc.
- **Nombres de canal** editables: D0 → "CLK", D1 → "DATA", etc.
- **Grid vertical** con lineas verticales cada cierta cantidad de muestras
- **Eje X**: tiempo absoluto o numero de muestra abajo
- **Cursor**: linea vertical que al pasar muestra el valor de cada canal en ese punto

### 3. Disparo (Trigger)
- **Canal fuente**: seleccionable entre los activos
- **Tipo**: rising edge, falling edge, either edge, high level, low level
- **Posicion de trigger**: 25%, 50%, 75% del buffer (ajustable)
- **Pre-trigger**: muestra datos ANTES del evento de trigger
- **Hold-off**: tiempo minimo entre disparos para evitar retrigger
- Indicar punto de trigger con triangulo ▲ rojo en el canvas

### 4. Protocolo WebSocket

#### Comandos (cliente → servidor):
```json
{"cmd":"run"}
{"cmd":"stop"}
{"cmd":"single"}
{"cmd":"set_pins","pins":[17,22,23,24,27]}
{"cmd":"set_labels","labels":{"17":"CLK","22":"DATA","23":"SDA","24":"SCL","27":"CS"}}
{"cmd":"set_rate","hz":1000000}
{"cmd":"set_trigger","pin":17,"type":"rising","hpos":50}
{"cmd":"set_timebase","value_us":10}
{"cmd":"get_state"}
```

#### Datos (servidor → cliente):
```json
{
  "type":"waveform",
  "pins":[17,22,23,24,27],
  "data":[
    {"t":0,"states":[0,1,0,1,0]},
    {"t":10,"states":[0,1,1,0,0]},
    ...
  ],
  "t0":1234567,
  "dt":1
}
```
*Alternativa optimizada (bitset):*
```json
{
  "type":"waveform",
  "pins":[17,22,23,24,27],
  "timestamps":[0,10,20,30,...],
  "states":[0b01010, 0b01100, ...],  // bits: pin[0]=LSB
  "t0":1234567,
  "dt":1
}
```

#### Estado (servidor → cliente):
```json
{"type":"state","mode":"run","rate":1000000,"pins":[17,22,23,24,27],"samples":4096}
```

### 5. UI Web — Layout

```
+----------------------------------------------------------+
| [host:____] [Connect]  ● CONNECTED  1 MHz   CH:5/8      |
+----------------------------------------------------------+
|                                                            |
| [+ Add Pin] [RUN ▼] [STOP] [SINGLE] Timebase: [10us▼]   |
|                                                            |
|   CH0 CLK  ━━┯━━━━━━┯━┯━┯━━━━━━┯━━━━                     |
|              │      │ │ │      │                          |
|   CH1 DATA  ━━┷━━━━━━┷━┷━┷━━━━━━┷━━━━                     |
|                                                            |
|   CH2 SDA   ━━┯━━━┯━┯━┯━┯━┯━┯━┯━━━┯━                     |
|                │   │ │ │ │ │ │ │   │                       |
|   CH3 SCL   ━━━┷━━━┷━┷━┷━┷━┷━┷━┷━━━┷━                     |
|                                                            |
|   CH4 CS     High ━━━━━━━━━━━━━━━━━━━━━━━                  |
|                                                            |
|   ▲ Trigger at 50%   Muestras: 1024   ▲ Cursor en t=50us   |
|                                                            |
| [Bus decoder ▼] [I2C ▼] [Export CSV] [Zoom +] [Zoom -]   |
|                                                            |
+----------------------------------------------------------+
```

### 6. Decodificadores de protocolo (bus)
Implementar decodificacion de protocolos digitales comunes directamente en JavaScript:

- **UART**: configurar baud rate, bits, parity. Decodificar bytes mostrados en hex/ASCII.
- **I2C**: detectar START/STOP, direccion 7-bit, ACK/NACK, datos en SDA con clock en SCL.
- **SPI**: detectar CS active, clock polarity/phase, MOSI/MISO bytes.
- **PWM**: medir frecuencia y duty cycle.
- **1-Wire**: detectar presence pulse, ROM commands.

Cada protocolo decodificado se muestra como **anotaciones de texto** encima de las formas de onda:
```
┌─────────────────────────────────────────────────┐
│ SDA  ━━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━   │
│         │A│ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │     │
│ SCL  ━━━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷    │
│        START  0x48 W ACK  DATA  ACK        STOP  │
└─────────────────────────────────────────────────┘
```

### 7. Mediciones digitales
- **Frecuencia**: en Hz del canal seleccionado
- **Periodo**: en segundos
- **Duty cycle**: alto/bajo en %
- **Ancho de pulso**: positivo y negativo en microsegundos
- **Tasa de bits**: baud rate aproximado
- **Cantidad de flancos**: rising y falling por segundo

### 8. Zoom y navegacion
- **Rueda del raton**: zoom horizontal (acercar/alejar en tiempo)
- **Ctrl+rueda**: zoom vertical (altura de las filas)
- **Click+arrastre**: desplazar la ventana de tiempo (pan)
- **Doble click**: reset zoom

### 9. Exportacion
- **CSV**: exportar datos visibles como CSV con columnas: time, D0, D1, D2,...
- **Screenshot**: capturar canvas como PNG descargable
- **Copiar**: copiar seleccion como texto binario

### 10. Cursors
- **Cursor A** (rojo) y **Cursor B** (azul), cada uno con su linea vertical
- Mostrar diferencia: Δt = tB - tA, Δ muestras
- Mediciones entre cursores: frecuencia = 1/Δt, conteo de flancos

### 11. Analisis matematico
- Suma logica (OR) entre canales
- Producto logico (AND)
- XOR entre dos canales
- Buscar patron: resaltar ocurrencias de una secuencia de bits (ej: 1010)

## Servidor firmware

### Opcion Pico W (C/C++, max performance)
```c
// Usar PIO para sampleo a 10 MHz en 16 pines
// Buffer DMA doble (ping-pong) de 2048 muestras
// Enviar por WebSocket lwip + tcp
// Ejemplo de configuracion PIO:
// - 16 pines, 1 instruccion = sample all en 1 ciclo
// - DMA transfiere a buffer mientras otro buffer se envia
// - Frecuencia: system_clock / (1 + clock_div)
```

### Opcion Pi 2W/4 (Python, facil)
```python
import gpiod
import asyncio
import websockets
import json

# Usar gpiod para leer lineas GPIO
# Eventos: rising/falling edge con timestamp
# Buffer circular con deque
```

## Pagina web (mandatoria: todo en 1 archivo HTML)
La pagina web incluye:
- HTML5 semantico
- CSS3 tema oscuro, responsive (mobile y desktop)
- JavaScript Canvas API nativo
- WebSocket connection
- LTTB downsampling adaptado para datos digitales (preservar flancos)
- Decodificadores de protocolo
- Cursors interactivos
- Export CSV/Screenshot
- Sin dependencias externas ni CDN
- Debe funcionar offline si el HTML se guarda localmente

## Entregable
Archivos:
- `firmware_pico/` — codigo C/C++ para Pico W (PIO sample + WebSocket)
- `server_pi/` — servidor Python para Pi 2W/4 (gpiod + websockets)
- `web/` — pagina web unica `logic_analyzer.html` con TODO incluido
- `README.md` con instrucciones de instalacion y uso

El analizador debe poder capturar y mostrar **trenes de bits a 1 MHz** sin perder muestras, y permitir decodificar protocolos como I2C (100 kHz), UART (115200 baud), y SPI (1 MHz) en tiempo real.
