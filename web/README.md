# Logic Analyzer — Web Frontend

Interfaz web para el analizador lógico de 26 canales vía GPIO en Raspberry Pi.

## Arranque rápido

```bash
# 1. Compilar el servidor C++ en la Raspberry Pi
cd server && make

# 2. Iniciar (directo o via systemd)
./logic_server 8080

# 3. Abrir en cualquier navegador de la red:
#    http://raspberry.local:8080
```

## Instalación como servicio systemd (auto-inicio en boot)

```bash
cd /ruta/del/proyecto
sudo ./script_tools/install-service.sh
```

## Cómo usar la UI

1. **Conectar** — Se abre un diálogo automático. Ingresar la URL WebSocket:
   - `ws://raspberry.local:8080` (desde la red local)
   - `ws://localhost:8080` (desde la misma máquina)
   - O hacer clic en "Connect" con el valor por defecto

2. **Controles principales**:
   - **RUN** — Captura continua en tiempo real
   - **STOP** — Congela la pantalla para analizar
   - **SINGLE** — Captura un solo disparo (útil con trigger)

3. **Timebase** — Selecciona la resolución horizontal (1 µs/div a 10 ms/div)

4. **Trigger** — Elige un pin y tipo de disparo:
   - `Rising` — Flanco de subida ╱
   - `Falling` — Flanco de bajada ╲
   - `Both` — Ambos flancos ╱╲
   - `High` / `Low` — Nivel alto/bajo

5. **Zoom y navegación**:
   - Rueda del ratón → zoom horizontal
   - Ctrl + rueda → zoom vertical (altura de canales)
   - Arrastrar con el ratón → desplazar (pan)
   - Doble clic → reset zoom

6. **Cursores A/B** — Hacer clic en "☰ Cursors" y luego en la forma de onda:
   - Muestra Δt y frecuencia entre cursores
   - Se arrastran haciendo clic cerca de ellos

7. **Exportar** — "📥 CSV" descarga los datos actuales como archivo CSV

## Conexión desde otros dispositivos

Desde un celular o tablet en la misma red:

```
http://raspberry.local:8080
```

O usando la IP directa:

```
http://192.168.1.XXX:8080
```

## Parámetros URL

| Parámetro | Ejemplo | Descripción |
|-----------|---------|-------------|
| `?connect=ws://IP:8080` | `?connect=ws://192.168.1.44:8080` | Auto-conectar al cargar la página |

## Arquitectura del frontend

`index.html` es un archivo **único auto-contenido** (sin dependencias externas):

- Canvas API para renderizado de formas de onda
- WebSocket nativo para comunicación en tiempo real
- Sin frameworks, sin librerías externas, sin CDN
- Funciona offline una vez cargado
- Responsive: 320px (celular) a 4K (monitor)

El servidor C++ sirve el HTML vía HTTP y transmite datos vía WebSocket con mensajes JSON con el formato `{ type: "waveform", timestamps: [...], states: [...], trigger_index: N }`.

## Solución de problemas

| Problema | Causa probable | Solución |
|----------|---------------|----------|
| "Failed to connect" | Servidor no corriendo | `ssh pi@raspberry.local 'systemctl status logic-analyzer'` |
| "Connection refused" | Puerto incorrecto | Verificar `ss -tlnp \| grep 8080` en la Pi |
| Pantalla negra sin ondas | GPIO sin señal o en simulación | Verificar conexiones, o iniciar con `GPIO_MMAP=0` para simular |
| No se ve el diálogo de conexión | Ya conectado automáticamente | Recargar la página (F5) |
| Lento / muchos samples perdidos | Red WiFi lenta | Usar cable Ethernet, o reducir sample rate |
| `/dev/gpiomem: Permission denied` | Usuario no está en grupo `gpio` | `sudo usermod -a -G gpio $USER && sudo chmod 660 /dev/gpiomem` |
