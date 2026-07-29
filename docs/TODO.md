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

- [x] **FIXED**: Reconexion WebSocket duplicaba el estado inicial (connectionId guard + clear data on connect)
- [x] **FIXED**: En modo simulación, el contador se resetea al reconectar (info-samples actualizado en connect())
- [x] **FIXED**: Renderizado producía diagonales en vez de escalones digitales (HTML desactualizado en /opt/)
- [x] **FIXED**: RUN no reanuda tras STOP con trigger (pre-trigger depth=0 + binario viejo)
- [ ] **DEADLOCK**: STOP deja el servidor inerte (no responde RUN/SINGLE/STOP)
- [ ] **SINGLE mode**: no muestra nada si el trigger nunca dispara (pre-trigger bloquea el buffer principal)
- [ ] **Señal incompleta**: en timebases largos (>50ms/div) solo se ve ~20% de la forma de onda

### ✅ FIXED: Reconexión WebSocket duplica el estado inicial

**Solución**: Se agregó un contador `wsConnectionId` que se incrementa en cada `connect()`.
Los handlers `onopen`, `onmessage`, `onclose`, `onerror` capturan el `myId` al crearse y
verifican `if (myId !== wsConnectionId) return;` para ignorar eventos de conexiones
obsoletas. Además, se anulan los handlers viejos antes de cerrar el WS previo.

- **Causa raíz**: Múltiples conexiones WS simultáneas alimentando datos al mismo
  `state.timestamps`/`state.states`. El flag `pending_reset_` del servidor es global
  y solo lo consume una conexión, las demás reciben datos sin `reset: true`.
- **Fix**: `web/index.html` — `wsConnectionId` + nullificar handlers viejos + clear data

### ✅ FIXED: En modo simulación el contador se resetea al reconectar

**Solución**: Al limpiar los datos viejos en `connect()`, se actualiza inmediatamente
el elemento `#info-samples` a `"0"` en lugar de dejar el valor stale hasta que llegue
el primer mensaje `waveform` del servidor.

- **Causa raíz**: `connect()` limpiaba `state.timestamps = []` pero no actualizaba
  el DOM. Entre la reconexión y la llegada del primer batch de datos, `info-samples`
  seguía mostrando el valor anterior (stale).
- **Fix**: Agregar `document.getElementById('info-samples').textContent = '0'` en `connect()`

### ✅ FIXED: Renderizado produce diagonales en vez de escalones digitales

**Solución**: El renderizado stair-step ya existía en `web/index.html` del repo, pero el
servicio systemd servía un HTML viejo (cacheado en el binario de las 07:15). Se copió el
HTML actualizado a `/opt/logic-analyzer/server/web/` y se reinició el servicio.

- **Causa raíz**: Binario desactualizado en `/opt/logic-analyzer/server/logic_server`
  (no reflejaba las compilaciones en `/home/joy/src/logic-analyzer/server/`)
- **Fix**: `sudo cp .../web/index.html /opt/logic-analyzer/server/web/` + `sudo systemctl restart`

### ✅ FIXED: RUN no reanuda adquisición tras STOP (cuando hay trigger configurado)

**Solución**: El `polling_loop` acumulaba muestras en el pre-trigger buffer con `continue`
incluso cuando `pre_trig_depth = 0` (Off). Con `pt_max = 0`, el buffer secundario nunca
se llenaba y jamás se vaciaba al buffer principal. Se agregó guarda `if (pt_max > 0)`
alrededor de la acumulación pre-trigger + `continue`.

- **Causa raíz**: `if (pt_max > 0)` faltante → con depth=0 todas las muestras se perdían
  en el pre-trigger buffer
- **Fix**: `server/core/server.cpp` — rodear bloque pre-trigger con `if (pt_max > 0)`

### 🔴 DEADLOCK: STOP deja el servidor inerte

**Síntoma**: Al presionar STOP la pantalla se congela (correcto), pero RUN/SINGLE no
responden nunca más. El servidor queda inerte hasta reiniciarlo.

**Causa raíz**: `handle_client_read()` (line 439) lockea `clients_mutex_` y llama a
`handle_ws_frame()`, que para el comando STOP lockea `clients_mutex_` **nuevamente**
(line 731). `std::mutex` no es recursivo → **deadlock** inmediato en el hilo de epoll.
El hilo principal queda trabado para siempre, no procesa más EPOLLIN/EPOLLOUT.

**Fix** (`server/core/server.cpp:730`): Eliminar el `std::lock_guard` redundante en
el handler de STOP — `clients_mutex_` ya está retenido por `handle_client_read`.
Además, agregar `single_request_.store(false)` en el handler RUN para limpiar
un posible SINGLE previo (evita que el broadcast loop re-pause tras RUN).

### 🔴 SINGLE mode: no muestra nada si el trigger no dispara

**Síntoma**: En modo SINGLE con trigger configurado y pre-trigger > 0, no llega
ninguna muestra al cliente si el trigger nunca se cumple.

**Causa raíz**: En SINGLE mode, el `polling_loop` envía muestras al pre-trigger
buffer y hace `continue` (saltándose el buffer principal) hasta que el trigger
dispare y vuelque todas las acumuladas. Si la condición de trigger jamás ocurre
en la señal de entrada, el buffer principal nunca recibe datos.

**Fix propuesto**: Timeout en SINGLE mode: si pasan N ciclos sin disparo, vaciar
el pre-trigger al buffer principal igual y pausar. O agregar un watchdog.

### 🔴 Señal incompleta en timebases largos

**Síntoma**: A 500ms/div solo se ve el ~20% inicial de la forma de onda.
A 100ms/div se ve más (~40%). A 10ms/div se ve completa.

**Causa raíz**: El cliente limita el buffer a `state.maxSamples = 65536`.
A 500kHz sample rate, 65536 muestras = 131ms de datos. El viewport a
500ms/div × 10 div = 5000ms necesita ~2.5M muestras para llenarse.
El buffer del cliente solo tiene 65536 (~2.6% del viewport).

**Fix propuesto**: Aumentar `maxSamples` dinámicamente según el timebase,
o implementar compresión/decimalización en el servidor para enviar menos
muestras en timebases lentos (ej: enviar solo transiciones).

## Mejoras continuas

- [ ] Flag `--quiet` / `--verbose` al arrancar: controlar `Logger::min_level_` (ej: LOG_WARN en producción, LOG_DEBUG en debug). El logger sincrónico (`fputs` + `std::cerr` por cada `LOG_INFO`) suma latencia en `handle_client_write` (~30 calls/seg).
- [ ] Agregar `static_assert` para tamanos de struct
- [ ] Reemplazar `std::map` con `std::unordered_map` en clientes
- [ ] Usar memory pool para evitar allocaciones en polling loop
- [ ] Documentar todas las funciones con Doxygen
- [ ] Cubrir bordes: buffer vacio, overflow, clientes lentos
