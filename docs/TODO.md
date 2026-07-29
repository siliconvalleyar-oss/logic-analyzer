# TODO — Logic Analyzer RPi

## v1.1.0 — Modularizacion del servidor C++

- [ ] Dividir `server/main.cpp` en modulos separados:
  - [ ] `server/gpio.h/.cpp` — Lectura GPIO (mmap + simulacion)
  - [ ] `server/ring_buffer.h/.cpp` — Buffer circular lock-free SPSC
  - [ ] `server/websocket.h/.cpp` — WebSocket frames + SHA1 + Base64
  - [ ] `server/trigger.h/.cpp` — Deteccion de flancos
  - [ ] `server/protocol.h/.cpp` — Mensajes JSON del protocolo
  - [ ] `server/server.h/.cpp` — Servidor HTTP + WebSocket (epoll)
  - [ ] `server/config.h/.cpp` — Configuracion desde JSON
  - [ ] `server/logger.h/.cpp` — Logging con archivo y rotacion
- [ ] Agregar Doxygen comments a todas las funciones publicas
- [ ] Hacer que `make test` compile y corra tests unitarios

## v1.2.0 — Decodificadores en C++ (server-side)

- [ ] Decodificador I2C en C++ (envia tramas decodificadas por WS)
- [ ] Decodificador UART en C++
- [ ] Decodificador SPI en C++
- [ ] Opcion para enviar datos decodificados en vez de raw bitfield
- [ ] Reducir ancho de banda en redes lentas

## v1.3.0 — Web UI mejorada

- [ ] Timeline / buffer overview (barra inferior con zoom)
- [ ] Arrastrar cursores A/B en el canvas
- [ ] Medidas automaticas: frecuencia, periodo, duty cycle
- [ ] Decodificadores I2C, UART, SPI en JavaScript
- [ ] Export CSV seleccionando rango con cursores
- [ ] Tema claro/oscuro toggle

## v1.4.0 — Performance

- [ ] Modo edge events con libgpiod (menos CPU)
- [ ] Compresion de datos (diferencial o run-length)
- [ ] Buffer de 65536 muestras en modo profundo
- [ ] Zero-copy en el broadcast a clientes
- [ ] Benchmark: verificacion de 0% perdida en 1 hora

## v1.5.0 — Features avanzadas

- [ ] Analizador de protocolo I2C en tiempo real
- [ ] Analizador de protocolo UART en tiempo real
- [ ] Analizador de protocolo SPI en tiempo real
- [ ] Buscador de patrones (ej: "1010" en cualquier canal)
- [ ] Trigger por patron en vez de solo flanco
- [ ] Mascara de canales (ocultar/mostrar)

## v1.6.0 — Trigger Activo, Pre-Trigger, Auto-Fit, Decodificación de Selección

- [x] **Trigger funcional**: Rising, Falling, High, Low, None con máquina de estados
- [x] **Pre-Trigger buffer**: muestras previas al disparo (configurable Off/64/128/256/512/1K/2K/4K)
- [x] **Run/Stop cíclico**: sin bloqueos ni resource leaks
- [x] **Decodificación de selección**: binario, hex, decimal, pulso, duty cycle con cursores A/B
- [x] **Auto-Fit en primera carga**: zoom automático al recibir datos
- [x] **Persistencia de pre-trigger**: guardado en config.json
- [x] **WebSocket dinámico**: usa el mismo puerto de la página cargada

## v1.7.0 — Multiplataforma

- [ ] Soporte para Pico W con PIO (10 MSps, 16 canales)
- [ ] Cliente Qt desktop (C++ nativo)
- [ ] Cliente Flutter mobile (Dart)
- [ ] API REST para control remoto

## v1.8.0 — Estabilidad y produccion

- [ ] Tests unitarios: buffer, trigger, decodificadores
- [ ] Test de resistencia: 24h sin perdida de datos
- [ ] CI/CD: GitHub Actions para compilar en ARM/ARM64/x64
- [ ] Package para Raspberry Pi OS (.deb)
- [ ] Dashboard web con historial de capturas

## Bugs conocidos

- [ ] Reconexion WebSocket a veces duplica el estado inicial
- [ ] En modo simulacion, el contador se resetea al reconectar
- [ ] El servidor no responde a `{"cmd":"stop"}` (siempre streaming)

### BUG CRITICO: Renderizado produce diagonales en vez de escalones digitales

**Sintoma**: Las señales digitales se dibujan con líneas diagonales/rampas en vez de
escalones rectangulares (solo segmentos H y V).

**Causa raíz**: El algoritmo stair-step en `web/index.html:1865-1932` tiene dos problemas:
1. La optimización por columna de píxel (`if (pxX !== lastPxX)`) salta muestras que
   caen en el mismo píxel, perdiendo transiciones de estado que ocurren a alta velocidad.
   Cuando se retoma en un nuevo píxel, `ctx.lineTo(x, y)` conecta desde la última
   posición conocida, creando diagonales si hubo transiciones intermedias perdidas.
2. `ctx.moveTo(leftX, y0)` coloca el lápiz en el margen izquierdo con el estado de la
   primera muestra. Si hay un gap temporal grande entre muestras, la línea horizontal
   desde `leftX` al primer sample visible es correcta, pero cuando se combina con el
   problema #1 se generan diagonales.

**Fix requerido**: Reescribir el renderizado con el algoritmo clásico de stair-step:
```
for cada par (i, i+1):
  línea horizontal de (t_i, s_i) a (t_{i+1}, s_i)
  if s_i != s_{i+1}: línea vertical de (t_{i+1}, s_i) a (t_{i+1}, s_{i+1})
```
Eliminar la optimización por píxel. Usar `ctx.beginPath()` por canal y dibujar
cada segmento explícitamente sin depender de `moveTo` en el borde izquierdo.

### BUG CRITICO: RUN no reanuda adquisición tras STOP (cuando hay trigger configurado)

**Sintoma**: STOP funciona (el log muestra "Cmd: stop — pausing"), pero al presionar
RUN nunca vuelven a llegar datos al cliente. Es necesario reiniciar el servicio.

**Causa raíz**: En `server.cpp:handle_ws_frame`, el comando RUN rearma el trigger:
```cpp
if (trigger_.pin >= 0 && trigger_.type != TriggerType::NONE) {
    trigger_armed_.store(true, ...);
    trigger_triggered_.store(false, ...);
}
```
Esto causa que el polling loop entre al bloque de pre-trigger (`server.cpp:163`):
`trigger_armed_=true && !trigger_triggered_` → todas las muestras van al
pre-trigger buffer con `continue` saltándose `buffer_.push()`.
Si la condición de trigger nunca ocurre (o no ocurre inmediatamente), el buffer
principal permanece vacío y el cliente no recibe datos.

**Flujo del bug**:
```
RUN → trigger se arma → polling loop envía muestras al pre-trigger buffer
                        (buffer principal vacío) → broadcast loop no encuentra datos
                        → cliente ve pantalla congelada

STOP → paused_=true → polling loop duerme

RUN → paused_=false, trigger se RE-ARMA → mismo problema: pre-trigger intercepta todo
```

**Fix requerido**: En modo RUN (no SINGLE), las muestras deben ir SIEMPRE al buffer
principal. El pre-trigger buffer debe ser un mecanismo separado que solo acumula
para determinar la posición del trigger, sin interceptar el flujo de datos.

Solución: eliminar el `continue` en el bloque "not fired" del trigger
(`server.cpp:245`), o mejor, usar `single_request_` como guarda: solo hacer
pre-trigger blocking en modo SINGLE, no en RUN.

## Mejoras continuas

- [ ] Agregar `static_assert` para tamanos de struct
- [ ] Reemplazar `std::map` con `std::unordered_map` en clientes
- [ ] Usar memory pool para evitar allocaciones en polling loop
- [ ] Documentar todas las funciones con Doxygen
- [ ] Cubrir bordes: buffer vacio, overflow, clientes lentos
