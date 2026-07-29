# Analizador Logico — Decodificadores de Protocolo

## General
Los decodificadores corren en **JavaScript en el navegador** (no en la Pi). Toman el array de estados y timestamps, y generan anotaciones de protocolo que se dibujan sobre las formas de onda.

## Decodificador I2C

### Pines requeridos
- SCL (clock) — pin del DUT conectado a GPIO X
- SDA (data) — pin del DUT conectado a GPIO Y

### Logica de decodificacion
```
1. Detectar START: SDA baja mientras SCL esta HIGH
2. Leer 8 bits de direccion (MSB first) + bit R/W
3. Verificar ACK: SDA baja en 9no pulso de clock
4. Leer 8 bits de datos + ACK
5. Repetir hasta detectar STOP: SDA sube mientras SCL HIGH
6. Si hay REPEATED START: no generar STOP, continuar
```

### Formato de salida
```javascript
// items para dibujar sobre el canvas
{ t: inicio_us, type: "start", label: "START", color: "#4CAF50" }
{ t: addr_us, type: "address", label: "0x48 W", ack: true, color: "#2196F3" }
{ t: data_us, type: "data", byte: 0xAB, char: null, ack: true, color: "#FF9800" }
{ t: stop_us, type: "stop", label: "STOP", color: "#F44336" }
```

### Configuracion
- Ninguna (auto-detecta velocidad hasta 1 MHz)

## Decodificador UART

### Pines requeridos
- TX (o RX) — pin del DUT conectado a GPIO X

### Logica de decodificacion
```
1. Esperar START bit (linea baja)
2. Muestrear 8 bits de datos en el centro de cada bit
3. Verificar parity bit (opcional)
4. Esperar STOP bit (linea alta)
5. Convertir byte a caracter ASCII
```

### Configuracion
```json
{"protocol":"uart","tx_pin":17,"baud":115200,"bits":8,"parity":"none","stop":1}
```

### Formato de salida
```javascript
{ t: byte_start_us, byte: 0x48, char: "H", parity_ok: true }
{ t: next_byte_us, byte: 0x65, char: "e", parity_ok: true }
```

## Decodificador SPI

### Pines requeridos
- SCLK (clock), MOSI (Master Out Slave In), MISO (Master In Slave Out), CS (Chip Select)

### Logica de decodificacion
```
1. Esperar CS activo (bajo, o alto segun config)
2. Por cada flanco de clock:
   - Leer MOSI → 1 bit
   - Leer MISO → 1 bit
3. Ensamblar bytes (MSB first o LSB first)
4. Al desactivar CS, finalizar trama
```

### Configuracion
```json
{"protocol":"spi","sclk":22,"mosi":23,"miso":24,"cs":27,"cpol":0,"cpha":0,"bits":8}
```
- `cpol`: clock polarity (0=idle low, 1=idle high)
- `cpha`: clock phase (0=muestreo en primer flanco, 1=segundo flanco)

### Formato de salida
```javascript
{ t: cs_active_us, mosi: 0xAB, miso: 0xCD, label: "MOSI:0xAB MISO:0xCD" }
```

## Decodificador PWM

### Pines requeridos
- 1 pin con senal PWM

### Logica
```
1. Medir tiempo HIGH (ton)
2. Medir tiempo LOW (toff)
3. Periodo = ton + toff
4. Frecuencia = 1/periodo
5. Duty cycle = ton / periodo * 100
```

### Configuracion
```json
{"protocol":"pwm","pin":17}
```

### Formato de salida
```javascript
{ t: 0, freq_hz: 1000, duty_pct: 75, ton_us: 750, toff_us: 250 }
```

## Decodificador 1-Wire

### Pines requeridos
- DQ (data) — 1 pin

### Logica
```
1. Detectar presence pulse: master baja 480us, libera, slave baja 60-240us
2. Leer bits: slot de 60us, bit=0 si linea baja, bit=1 si linea alta
3. Leer ROM command, direccion 64-bit, datos
```

## Decodificador Manchester / NRZ
Decodificacion de codificacion Manchester (Ethernet 10BASE-T, RFID):
- Transicion en medio del bit: 0 = baja→sube, 1 = sube→baja

## Selection Decoding (Análisis Manual con Cursores)

Además de los decodificadores automáticos de protocolo, el analizador incluye
**análisis manual** de la región seleccionada con los cursores A/B.

### Cómo funciona
1. Habilitar cursores (botón `Cursors` en el toolbar o tecla `C`)
2. Hacer clic en el waveform para posicionar cursor A y cursor B
3. El panel de cursores muestra automáticamente:

### Información mostrada

| Campo | Descripción | Ejemplo |
|-------|-------------|---------|
| **Bits** | Cantidad de transiciones y GPIO analizado | `12 (GPIO17)` |
| **Bin** | Representación binaria de las transiciones | `10100101` |
| **Hex** | Hexadecimal agrupado de a 4 bits | `0xA5` |
| **Dec** | Decimal (hasta 53 bits) | `165` |
| **Pulse** | Ancho HIGH y LOW del pulso | `H=750.0µs L=250.0µs` |
| **Duty** | Duty cycle porcentual | `75.0%` |

### Detalles técnicos
- Usa el **primer canal habilitado** para el análisis
- El bitstream se genera comprimiendo muestras consecutivas del mismo valor
- Límite de **64 transiciones** mostradas (con indicador si se excede)
- El hexadecimal trunca a 12 dígitos si es muy largo
- El decimal soporta hasta 53 bits (límite de `parseInt` con precisión double)
- El análisis de pulso mide: ancho HIGH total, ancho LOW total, duty cycle,
  período promedio entre flancos y frecuencia derivada
- Los cursores no necesitan estar en orden (A puede estar a la izquierda o derecha de B)

### Casos de uso
- Verificar tramas digitales manualmente (ej: `10100101` = `0xA5` = 165)
- Medir anchos de pulso y duty cycle de señales PWM
- Analizar períodos de reloj entre transiciones
- Inspeccionar protocolos no soportados por los decodificadores automáticos

## Agregar decodificadores personalizados
Los decodificadores son funciones JS registradas en runtime:
```javascript
registerDecoder("custom_proto", {
  requiredPins: 2,
  decode: function(timestamps, states, config) {
    // timestamps: array de us
    // states: array de enteros (bitfield)
    // config: objeto con configuracion
    // return: array de decoded items
    return [{t: 0, label: "DECODED", color: "#FFF"}];
  }
});
```

## Renderizado en Canvas
Cada item decodificado se dibuja como:
- **Texto** encima de las formas de onda (en la fila del canal o en un carril de anotaciones)
- **Color** segun tipo: START verde, direccion azul, datos naranja, STOP rojo, ACK/NACK circulo
- **Tooltip** al pasar mouse: detalle completo del byte/valor
- **Zoom**: al hacer zoom, los textos se re-posicionan para no solaparse
