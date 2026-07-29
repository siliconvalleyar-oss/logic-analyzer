# Solucion de Problemas

> Los problemas de conexion y protocolo son los mismos para las tres plataformas (Web, Qt, Flutter) porque todas usan el mismo WebSocket JSON.

## Problemas de Conexion

### No se detecta la Pico por USB

| Causa | Solucion |
|-------|----------|
| Cable USB solo de carga | Usa un cable USB que soporte datos |
| Driver no instalado | En Windows, instalar driver USB CDC del RP2040 |
| Puerto ocupado | Cierra otras aplicaciones que usen el puerto serial |
| Pico no tiene firmware | Carga el firmware `.uf2` correctamente |

**Verificacion**:
```bash
# Linux
ls /dev/ttyACM* /dev/ttyUSB*
dmesg | grep -i pico

# macOS
ls /dev/cu.usb*
system_profiler SPUSBDataType | grep -A 5 "RP2"

# Windows (PowerShell)
Get-PnpDevice | Where-Object {$_.FriendlyName -like "*Pico*"}
```

### No se conecta al WebSocket

```javascript
// Verificar desde la consola del navegador
const ws = new WebSocket('ws://scoppy-web.local:8090');
ws.onopen = () => console.log('Conectado');
ws.onerror = (e) => console.error('Error:', e);
```

| Sintoma | Causa | Solucion |
|---------|-------|----------|
| `ERR_CONNECTION_REFUSED` | Puerto incorrecto o servidor no iniciado | Verifica puertos con `nmap -p 80,8090 <ip>` |
| `ERR_CONNECTION_TIMEOUT` | Firewall o red incorrecta | Desactiva firewall, verifica que estes en la misma red |
| `ERR_SSL_PROTOCOL_ERROR` | Usando HTTPS en lugar de HTTP | La URL debe ser `ws://` no `wss://` |
| Conexion intermitente | Interferencia Wi-Fi | Acerca la Pico W al router, cambia canal |

### La Pico W no se conecta al Wi-Fi

**Sintomas**: LED parpadea constantemente, nunca se pone fijo.

1. Verifica el SSID y password en la configuracion.
2. Asegurate de que el router use 2.4 GHz (Pico W no soporta 5 GHz).
3. Verifica el pais Wi-Fi (afecta los canales disponibles).
4. Prueba en modo AP primero:
   ```
   1. Conectate a la red SCOPPY-WEB-XXXX desde tu telefono/PC
   2. Abre http://192.168.4.1
   ```

## Problemas de Visualizacion

### La forma de onda no aparece

| Causa | Solucion |
|-------|----------|
| Sin senal en la entrada | Conecta una senal, o usa el test signal (GPIO 22 → GPIO 26) |
| Escala incorrecta | Ajusta Volts/Div y Timebase a valores que correspondan a tu senal |
| Trigger mal configurado | Cambia trigger a modo "Auto" |
| Canal desactivado | Verifica que CH1 o CH2 este activado en los controles |
| Muestreo muy lento | Aumenta la frecuencia de muestreo |

### La forma de onda se ve distorsionada

```
Problema:  ▁▁▁▁███▄▄▄▄▄▄███▁▁▁▁  (escalones)
Causa:    Muy pocas muestras por ciclo
Solucion: Aumenta la frecuencia de muestreo o disminuye la frecuencia de la senal

Problema:  ~~~~~~  (ruido excesivo)
Causa:    Senal debil sin apantallamiento, o fuente de ruido cercana
Solucion: Usa cable coaxial, agrega capacitor de 100nF en la entrada

Problema:  ████████████████████  (recortada en el tope)
Causa:    Senal fuera del rango del ADC (> 3.3V)
Solucion: Usa un divisor de voltaje o front-end atenuador

Problema:  ╱╱╱╱╱╱╱╱╱╱╱╱╱╱╱╱╱  (linea recta diagonal)
Causa:    Offset DC en la senal o calibracion incorrecta
Solucion: Verifica el acoplamiento DC/AC, recalibra
```

### La interfaz web va lenta

1. **Demasiadas muestras por trama**: Reduce `adc_samples_per_frame` en la configuracion.
2. **Renderizado ineficiente**: Activa el modo de compresion por columnas (dibujar min/max por pixel).
3. **Muchas ventanas abiertas**: Cada cliente recibe todos los datos. Cierra las que no uses.
4. **Wi-Fi congestionado**: Cambia la Pico W a modo AP o usa USB serial en su lugar.

### La trama se congela o se salta

| Causa | Solucion |
|-------|----------|
| Buffer lleno en el cliente | Aumenta el tamano del RingBuffer |
| Perdida de paquetes Wi-Fi | Reduce la tasa de muestreo o usa USB |
| GC del navegador | Reduce la frecuencia de renderizado (usa 30 fps en vez de 60) |

## Problemas de Hardware

| Problema | Causa | Solucion |
|----------|-------|----------|
| Pico no enciende | Alimentacion insuficiente | Usa fuente de 5V/500mA minimo |
| ADC lee siempre 0 | Senal fuera de rango o GND flojo | Verifica conexiones y rango de voltaje |
| ADC lee siempre 4095 | Senal > 3.3V | Agrega divisor de voltaje |
| Ruido excesivo (>50mV) | Sin filtro anti-aliasing | Agrega capacitor de 100nF en la entrada ADC |
| Cross-talk entre canales | Pistas paralelas largas | Separa las pistas, usa plano de tierra |
| La Pico se recalienta | Sobrecorriente o corto | Desconecta todo y prueba solo la Pico |

## Errores del Firmware

| Mensaje de Error | Significado | Solucion |
|-----------------|-------------|----------|
| `ERR_ADC_INIT` | Fallo al inicializar ADC | Reinicia la Pico, verifica conexiones |
| `ERR_DMA` | Canal DMA no disponible | El firmware ya uso todos los canales DMA |
| `ERR_WIFI_CONNECT` | No pudo conectar al AP | Verifica SSID/password, senal Wi-Fi |
| `ERR_WIFI_AP` | No pudo crear AP | Cambia de canal Wi-Fi |
| `ERR_WS_FULL` | Buffer WebSocket lleno | Reduce el numero de clientes o la tasa de datos |
| `ERR_ADC_OVERFLOW` | ADC FIFO desbordado | Reduce la frecuencia de muestreo |
| `ERR_TIMEOUT` | Trigger timeout en modo Normal | Cambia a modo Auto o ajusta el nivel |
| `ERR_FLASH_WRITE` | Fallo al escribir config | La flash puede estar desgastada |

## Depuracion por UART

Conecta un adaptador USB-UART a los GPIOs 0 (TX) y 1 (RX) de la Pico:

```
Pico GPIO 0 (TX) ──── USB-UART RX
Pico GPIO 1 (RX) ──── USB-UART TX
Pico GND        ──── USB-UART GND
```

Configura el monitor serial a 115200 baud:

```bash
screen /dev/ttyUSB0 115200
# o
minicom -D /dev/ttyUSB0 -b 115200
```

El firmware imprime mensajes de diagnostico como:
```
[SCOPPY] Firmware v1.0 iniciado
[SCOPPY] ADC: 500 kS/s, 2 canales
[SCOPPY] WiFi: Modo AP, SSID=SCOPPY-WEB-A1B2
[SCOPPY] Cliente WebSocket conectado desde 192.168.4.2
[SCOPPY] Error: ERR_ADC_OVERFLOW - buffer desbordado
```

## Logs del Servidor Node.js

```bash
# Iniciar con logs detallados
LOG_LEVEL=debug node server.js

# Ver logs en tiempo real
node server.js 2>&1 | tee scoopy-server.log
```

## Diagnostico Rapido

```bash
# 1. La Pico responde?
lsusb | grep -i "Raspberry\|RP2\|Pico"
# Deberia mostrar: "Microchip Technology Inc. RP2"

# 2. Puerto serial disponible?
ls -la /dev/ttyACM*

# 3. La Pico W esta en la red?
ping scoopy-web.local

# 4. Puertos abiertos?
nc -zv scoopy-web.local 80
nc -zv scoopy-web.local 8090

# 5. Trafico WebSocket?
websocat ws://scoppy-web.local:8090
```

## Problemas Especificos de Qt

| Problema | Causa | Solucion |
|----------|-------|----------|
| `QWebSocket` no se conecta | Qt no compilado con WebSockets | `sudo apt install qt6-websockets-dev` |
| Bajo rendimiento de renderizado | QPainter sin optimizar | Usar `QPainterPath` en vez de `drawLine` por punto |
| FFT lento | Algoritmo O(|N|^2) | Usar Ooura FFT o FFTW |
| El widget parpadea | Sin doble buffer | Activar `setAutoFillBackground(true)` |

## Problemas Especificos de Flutter

| Problema | Causa | Solucion |
|----------|-------|----------|
| WebSocket no se conecta en Android | Cleartext HTTP deshabilitado | Agregar `usesCleartextTraffic` en AndroidManifest.xml |
| CustomPainter lento | Muchas operaciones de dibujo | Usar `RepaintBoundary` y reducir puntos con downsampling |
| FFT bloquea UI | Calculo sincrono en el main isolate | Usar `compute()` para ejecutar FFT en isolate separado |
| No se ve la traza en iOS | `NSAppTransportSecurity` | Agregar excepcion en Info.plist para IP local |
