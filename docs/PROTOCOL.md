# Analizador Logico — Protocolo WebSocket

## General
- **Puerto**: 8091 (separado del osciloscopio en 8090)
- **Formato**: JSON sobre WebSocket
- **Compresion**: ninguna (datos binarios via bitpacking)

## Comandos (Cliente → Servidor)

### Control de adquisicion
```json
{"cmd":"run"}
{"cmd":"stop"}
{"cmd":"single"}
{"cmd":"arm"}                    // arma trigger en modo SINGLE
```

### Configuración de canales
```json
{"cmd":"set_pins","pins":[17,22,23,24,27]}
{"cmd":"set_labels","labels":{"17":"CLK","22":"DATA","23":"CS"}}
{"cmd":"set_threshold","pin":17,"volts":1.65}  // solo para modelos con umbral configurable
```

### Configuración de muestreo
```json
{"cmd":"set_rate","hz":1000000}
{"cmd":"set_sample_count","count":2048}
{"cmd":"set_mode","mode":"polling"}
// mode: "polling" | "edge" | "timestamp"
```

### Trigger
```json
{"cmd":"set_trigger","pin":17,"type":"rising","hpos":50}
// type: "rising" | "falling" | "either" | "high" | "low"
// hpos: posicion horizontal del trigger en % (0-100)
```

### Timebase (zoom)
```json
{"cmd":"set_timebase","value_us":10}
// valor por division. 1us, 10us, 100us, 1ms, 10ms, 100ms
```

### Consultas
```json
{"cmd":"get_state"}
{"cmd":"get_pins"}               // lista pines disponibles
{"cmd":"get_capabilities"}       // max rate, max pins, modes
```

## Mensajes (Servidor → Cliente)

### Waveform data
```json
{
  "type":"waveform",
  "pins":[17,22,23,24],
  "timestamps":[0,10,20,30,40,50],
  "states":[2,6,7,3,1,0],
  "t0":1234567,
  "dt_us":1,
  "trigger_index":25
}
```
- `states`: array de enteros, bit k = estado del pin `pins[k]`
- `trigger_index`: indice dentro del array donde ocurrio el trigger (-1 si no hubo)
- `dt_us`: intervalo entre muestras en microsegundos

### State update
```json
{
  "type":"state",
  "mode":"run",
  "rate":1000000,
  "pins":[17,22,23,24],
  "samples_captured":4096,
  "clients":2,
  "buffer_usage":0.45
}
```

### Trigger event
```json
{
  "type":"trigger",
  "pin":17,
  "edge":"rising",
  "timestamp":1234567890
}
```

### Decoded data (decodificadores)
```json
{
  "type":"decoded",
  "protocol":"i2c",
  "items":[
    {"t":100,"type":"start","label":"START"},
    {"t":120,"type":"address","value":"0x48","read":false,"ack":true},
    {"t":160,"type":"data","byte":0xAB,"ack":true},
    {"t":200,"type":"stop","label":"STOP"}
  ]
}
```

```json
{
  "type":"decoded",
  "protocol":"uart",
  "items":[
    {"t":500,"byte":0x48,"char":"H","parity_ok":true},
    {"t":600,"byte":0x65,"char":"e","parity_ok":true}
  ]
}
```

### Error
```json
{"type":"error","msg":"gpiod line request failed: Device or resource busy"}
```

## Capabilities response
```json
{
  "type":"capabilities",
  "max_pins":26,
  "max_rate_hz":1000000,
  "modes":["polling","edge"],
  "protocols":["i2c","uart","spi","pwm"],
  "model":"Pi 4 Model B Rev 1.5",
  "arch":"aarch64",
  "kernel":"6.1.21-v8+"
}
```

## Diferencias con protocolo del osciloscopio (puerto 8090)

| Aspecto | Osciloscopio (:8090) | Analizador Logico (:8091) |
|---------|---------------------|--------------------------|
| Datos | voltios (float) | bits (int bitfield) |
| Canales | 2-4 analogicos | 8-26 digitales |
| Trigger | nivel de voltaje | flanco en pin |
| Mediciones | Vpp, Vavg, Freq | Frecuencia, Duty, Baud rate |
| Decodificadores | no | I2C, UART, SPI, PWM |
| Formato optimo | JSON con floats | JSON con bitfields |
