# Bug Tracking — Logic Analyzer RPi

## Resumen

| # | Bug | Estado | Gravedad | Fix en |
|---|-----|--------|----------|--------|
| 1 | RUN no reanuda tras STOP con trigger configurado | ✅ FIXED | CRITICAL | `server.cpp` — guarda `if (pt_max > 0)` |
| 2 | DEADLOCK: STOP deja el servidor inerte | ✅ FIXED | CRITICAL | `server.cpp` — mutex redundante + cleanup |
| 3 | SINGLE mode no bloquea hasta disparo | ✅ FIXED | HIGH | `server.cpp` — exchange→load |
| 4 | Señal incompleta en timebases largos | ✅ FIXED | HIGH | `server.cpp` — MAX_BURST 4096→65536 |
| 5 | Renderizado produce diagonales (stair-step) | ✅ FIXED | CRITICAL | `index.html` — algoritmo stair-step |
| 6 | Reconexión WS duplica estado inicial | ✅ FIXED | MEDIUM | `index.html` — connectionId guard |
| 7 | Contador simulación se resetea al reconectar | ✅ FIXED | LOW | `index.html` — DOM update en connect() |
| 8 | Debug logging síncrono en hot path | ✅ FIXED | LOW | `server_manager.sh` — flag `--quiet` |
| 9 | SINGLE timeout 1s (sin condición) | ✅ FIXED | HIGH | `server.cpp` — timeout 1s en polling_loop |
| 10 | Zoom/pan no se restaura al reconectar | 🔴 OPEN | LOW | Pendiente |

---

## Bugs CRITICAL (fijos)

### BUG-1: RUN no reanuda adquisición tras STOP (con trigger configurado)

**Síntoma**: STOP funciona (pausa), pero RUN nunca reanuda. Datos no llegan al
cliente. Con trigger configurado y `pre_trig_depth = 0`, el problema es
inmediato. Con depth > 0 y trigger que nunca dispara, igual.

**Causa raíz**: El `polling_loop` acumulaba muestras en el pre-trigger buffer
con `continue`, incluso cuando `pre_trig_depth = 0` (Off). Como `pt_max = 0`,
el buffer secundario nunca alcanzaba su capacidad, jamás se vaciaba al buffer
principal, y las muestras se perdían para siempre.

**Fix** (`server/core/server.cpp:235`): Rodear todo el bloque de acumulación
pre-trigger con `if (pt_max > 0)`. Cuando depth = 0, las muestras fluyen
directamente al buffer principal sin interferencia.

```diff
-// Sin guarda: siempre acumulaba y hacia continue
-pre_trig_buffer_.push_back({ts, states});
-in_pre_trig = true;
-if (single_request_) { continue; }
+if (pt_max > 0) {
+    pre_trig_buffer_.push_back({ts, states});
+    in_pre_trig = true;
+    if (single_request_) { continue; }
+}
```

### BUG-2: DEADLOCK — STOP deja el servidor inerte

**Síntoma**: Al presionar STOP la pantalla se congela (correcto), pero RUN,
SINGLE, e incluso otro STOP no responden. El servidor queda inerte hasta
reiniciarlo. No hay logs de "Cmd: run — resuming".

**Causa raíz**: `handle_client_read()` (line 439) lockea `clients_mutex_` con
`std::lock_guard` y llama a `handle_ws_frame()`. El handler del comando STOP
(introducido en la feature de pre-trigger) lockea `clients_mutex_` nuevamente
(line 731 original) para limpiar `write_buf`. `std::mutex` no es recursivo →
**deadlock inmediato** del hilo de epoll.

El hilo principal se cuelga dentro de `handle_client_read`, nunca retorna, y
no procesa más eventos EPOLLIN/EPOLLOUT. El broadcast thread también se
bloquea al intentar mandar datos (lockea `clients_mutex_`).

**Fix** (`server/core/server.cpp:730`):
1. Eliminar el `std::lock_guard<std::mutex> lk(clients_mutex_)` redundante en
   el handler STOP — `clients_mutex_` ya está retenido por `handle_client_read`.
2. Agregar `trigger_triggered_.store(true)` en STOP para evitar que al reanudar
   el trigger quede en estado "armed sin disparar" y bloquee el polling loop.
3. Agregar `single_request_.store(false)` en STOP para limpiar cualquier SINGLE
   pendiente.

```diff
-// STOP handler original: lockea clients_mutex_ otra vez → DEADLOCK
-{
-    std::lock_guard<std::mutex> lk(clients_mutex_);
-    for (auto& [fd, st] : clients_) {
-        st.write_buf.clear();
-    }
-}
+// STOP handler corregido: ya estamos bajo clients_mutex_
+for (auto& [fd, st] : clients_) {
+    st.write_buf.clear();
+}
```

### BUG-5: Renderizado produce diagonales en vez de escalones digitales

**Síntoma**: Las señales digitales se dibujan con líneas diagonales/rampas
en vez de escalones rectangulares (solo segmentos H y V).

**Causa raíz**: El algoritmo stair-step en `web/index.html:1865-1932` tenía
una optimización por columna de píxel (`if (pxX !== lastPxX)`) que saltaba
transiciones de estado que caían en el mismo píxel, y al retomar en un nuevo
píxel, `lineTo` conectaba desde la última posición conocida creando diagonales.
Además, el HTML servido por systemd era una versión vieja anterior al fix.

**Fix**:
1. Reescribir el renderizado con el algoritmo clásico de stair-step (H → V):
   - horizontal de (t_i, s_i) a (t_{i+1}, s_i)
   - si s_i ≠ s_{i+1}: vertical de (t_{i+1}, s_i) a (t_{i+1}, s_{i+1})
2. Sin optimización por píxel, `beginPath()` por canal.
3. Deploy: copiar HTML actualizado a `/opt/logic-analyzer/server/web/`.

---

## Bugs HIGH (fijos)

### BUG-3: SINGLE mode no bloquea hasta el disparo

**Síntoma**: En SINGLE mode con trigger, el comando se completaba
inmediatamente enviando 0 muestras al cliente. El trigger nunca esperaba
a disparar.

**Causa raíz**: El `broadcast_loop` ejecutaba
`single_request_.exchange(false, ...)` en cada iteración (~33ms). Esto leía
el flag SINGLE y lo limpiaba, incluso si el trigger no había disparado.
El polling loop veía `single_request_ = false` y dejaba de bloquear el
buffer principal, permitiendo que el broadcast loop completara el SINGLE
con datos vacíos.

Además, `trigger_triggered_` no se reseteaba en STOP, causando que en
un STOP→SINGLE posterior, el trigger apareciera como "ya disparado".

**Fix** (`server/core/server.cpp`):
1. `broadcast_loop`: `single_request_.exchange(false)` → `single_request_.load()`
   (solo lectura, sin limpiar).
2. El flag SINGLE solo se limpia cuando:
   - El trigger dispara: `single_request_.store(false)`
   - O llega un comando RUN: `single_request_.store(false)`
3. `broadcast_loop` completa SINGLE solo si `!trigger_armed_ || trigger_triggered_`.

### BUG-4: Señal incompleta en timebases largos

**Síntoma**: A 500ms/div solo se ve ~20% inicial de la forma de onda.
A 100ms/div ~40%, a 10ms/div casi completa. El resto del viewport se ve
plano.

**Causa raíz**: `MAX_BURST_NORMAL = 4096` truncaba cada batch del broadcast
loop a solo 4096 muestras ≈ 8ms (a 500kHz). El buffer del cliente tenía
65536 muestras como máximo, pero el broadcast loop solo entregaba 4096
por ciclo, descartando el 93.75% de las muestras disponibles en el ring
buffer. El cliente nunca acumulaba suficiente data para llenar timebases
largos.

**Fix** (`server/core/server.cpp:310-318`): Aumentar ambos límites:
- `MAX_BURST_RESET = 4096` → `65536`
- `MAX_BURST_NORMAL = 4096` → `65536`

Con 65536 muestras por batch (el tamaño completo del ring buffer), el
cliente recibe toda la data disponible y acumula hasta cubrir timebases
largos (131ms a 500kHz).

---

## Bugs MEDIUM/LOW (fijos)

### BUG-6: Reconexión WebSocket duplica estado inicial

**Síntoma**: Al reconectar, los datos de la sesión anterior se mezclaban con
los nuevos, mostrando trazas superpuestas o saltos en el tiempo.

**Fix**: Control `wsConnectionId` que invalida handlers de conexiones
obsoletas + clear de `state.timestamps`/`state.states` al conectar.

### BUG-7: Contador simulación se resetea al reconectar

**Síntoma**: Tras reconectar en modo simulación, el contador de muestras
mostraba el valor de la sesión anterior hasta que llegaba el primer batch.

**Fix**: `document.getElementById('info-samples').textContent = '0'` en
`connect()`.

### BUG-8: Debug logging síncrono en hot path

**Descripción**: El logger usa `fputs` + `std::cerr` con buffer desactivado
(`_IONBF`). Cada `LOG_INFO` en `handle_client_write` cuesta ~30μs. A 30
logs/segundo el overhead es despreciable, pero en redes lentas con
backpressure puede sumar.

**Fix**: Flag `--quiet` en `server_manager.sh` que setea `LOG_WARN` como
nivel mínimo de log, eliminando los `LOG_INFO` en producción.

---

## Bugs OPEN

### BUG-9: SINGLE timeout 1s — forzar captura si el trigger no dispara

**Síntoma**: SINGLE + trigger configurado + señal que nunca cumple la
condición de trigger → SINGLE nunca se completa.

**Causa**: El pre-trigger buffer bloquea el buffer principal hasta que
el trigger dispare mediante `if (single_request_) { continue; }`. Si
la condición nunca se cumple, el bloqueo es permanente.

**Fix** (`server/core/server.cpp:252-281`): Agregar timeout de 1 segundo:
1. Se registra `single_start_time` cuando la espera comienza.
2. Cada iteración compara `elapsed >= 1s`.
3. Al cumplirse: `trigger_triggered_ = true`, se vacía el pre-trigger
   buffer al buffer principal, y se deja fluir al `buffer_.push()` general.
4. `single_request_` NO se limpia aquí — el `broadcast_loop` detecta
   `single_mode=true` + `trigger_triggered_=true` y pausa el sistema.
5. Además, se corrigió un bug existente: el pre-trigger flush en el
   `if (fire)` normal usaba `single_request_.load()` DESPUÉS de limpiarlo
   (siempre falso). Se guarda `was_single` antes de limpiar.

```diff
 // Antes: espera infinita
-if (single_request_) {
-    while (now < next) { yield; }
-    continue;
-}
+// Después: timeout 1s
+if (single_request_) {
+    if (single_start_time == time_point{}) {
+        single_start_time = now();
+    }
+    if (elapsed >= 1s) {
+        trigger_triggered_ = true;
+        // single_request_ queda true → broadcast loop pausa
+        flush pre-trigger al buffer principal;
+    } else {
+        while (now < next) { yield; }
+        continue;
+    }
+}
```

### BUG-10: Zoom/pan no se restaura al reconectar

**Síntoma**: El servidor persiste zoom/pan en `config.json` y los envía
al reconectar (`set_viewport`), pero el cliente no siempre aplica estos
valores al restaurar el viewport.

**Causa**: El mensaje `config` con zoom_level/pan_x se procesa en
`handleMessage`, pero la función de restauración puede no ejecutarse
si el estado `_applyViewportOnConnect` no está correctamente seteado.

---

## Historial de fixes

| Fecha | Bug | Archivos | Commit |
|------|-----|----------|--------|
| 2026-07-29 | BUG-1: RUN tras STOP | `server/core/server.cpp` | `e24fca5` |
| 2026-07-29 | BUG-5: Diagonales | `web/index.html` | `e24fca5` |
| 2026-07-29 | BUG-2: DEADLOCK | `server/core/server.cpp` | `ae57731` |
| 2026-07-29 | BUG-3: SINGLE exchange | `server/core/server.cpp` | `ae57731` |
| 2026-07-29 | BUG-4: MAX_BURST | `server/core/server.cpp` | `ae57731` |
| 2026-07-29 | BUG-6: WS reconnect | `web/index.html` | — |
| 2026-07-29 | BUG-7: Contador sim | `web/index.html` | — |
| 2026-07-29 | BUG-8: Debug flag | `server_manager.sh` | — |
| 2026-07-30 | BUG-9: SINGLE timeout | `server/core/server.cpp` | `0d0b344` |
