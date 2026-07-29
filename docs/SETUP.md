# Analizador Logico — Configuracion por modelo

## Conexion general

```
[DUT (Device Under Test)]  ──  [Raspberry Pi GPIO]
                                 ─ GND comun
                                 ─ 3.3V si el DUT es 3.3V
                                 ─ Divisor de voltaje si DUT es 5V
```

**ATENCION**: GPIO de Raspberry Pi son **3.3V tolerant**. NO conectar 5V directamente. Usar divisor resistivo 2:1 (2kΩ + 1kΩ) para senales 5V.

## Pin mappings por modelo

### Pi 2W (40-pin header, modelo mas limitado)

| Pin Fisico | GPIO | Funcion recomendada | Nota |
|-----------|------|--------------------|------|
| 3 | GPIO2 (SDA1) | D0 — Canal 0 | I2C si no se usa |
| 5 | GPIO3 (SCL1) | D1 — Canal 1 | |
| 7 | GPIO4 | D2 — Canal 2 | |
| 8 | GPIO14 (TXD0) | D3 — Canal 3 | UART si no se usa |
| 10 | GPIO15 (RXD0) | D4 — Canal 4 | |
| 11 | GPIO17 | D5 — Canal 5 | |
| 12 | GPIO18 | D6 — Canal 6 | |
| 13 | GPIO27 | D7 — Canal 7 | |
| 15 | GPIO22 | D8 — Canal 8 | |
| 16 | GPIO23 | D9 — Canal 9 | |

En Pi 2W, hay **26 pines GPIO utilizables** (GPIO0-27, excluyendo los reservados).

### Pi 4 (40-pin header, maximo rendimiento)

| Pin Fisico | GPIO | Funcion recomendada | Nota |
|-----------|------|--------------------|------|
| 3 | GPIO2 (SDA1) | D0 | |
| 5 | GPIO3 (SCL1) | D1 | |
| 7 | GPIO4 | D2 | |
| 8 | GPIO14 (TXD0) | D3 | |
| 10 | GPIO15 (RXD0) | D4 | |
| 11 | GPIO17 | D5 | |
| 12 | GPIO18 | D6 | |
| 13 | GPIO27 | D7 | |
| 15 | GPIO22 | D8 | |
| 16 | GPIO23 | D9 | |
| 18 | GPIO24 | D10 | |
| 22 | GPIO25 | D11 | |
| 29 | GPIO5 | D12 | |
| 31 | GPIO6 | D13 | |
| 32 | GPIO12 | D14 | |
| 33 | GPIO13 | D15 | |

Pi 4: **26 pines GPIO** utilizables (todos GPIO0-27, evitar GPIO0/1 por HAT ID).

### Pi 5 (40-pin header, nuevo chip RP1)

| Pin Fisico | GPIO | Funcion recomendada |
|-----------|------|--------------------|
| Similar a Pi 4 | GPIO0-27 | Todos disponibles |

Pi 5 usa el chip **RP1** para GPIO, con latencia ligeramente diferente. `libgpiod` funciona igual.

### Pi Zero 2W (40-pin header, misma limitacion que Pi 2W)

Mismo pinout que Pi 2W. CPU mas lento (Cortex-A53 a 1GHz), esperar ~70% del rendimiento de Pi 2W.

## Diagrama de conexion (ejemplo con 8 canales)

```
DUT (microcontrolador)         Raspberry Pi
┌──────────────┐              ┌──────────────┐
│ CLK  ────────┼──────────────┤ GPIO17 (D0)  │
│ DATA ────────┼──────────────┤ GPIO22 (D1)  │
│ CS   ────────┼──────────────┤ GPIO23 (D2)  │
│ MOSI ────────┼──────────────┤ GPIO24 (D3)  │
│ MISO ────────┼──────────────┤ GPIO27 (D4)  │
│ TX   ────────┼──────────────┤ GPIO4  (D5)  │
│ RX   ────────┼──────────────┤ GPIO5  (D6)  │
│ INT  ────────┼──────────────┤ GPIO6  (D7)  │
│ GND  ────────┼──────────────┤ GND          │
└──────────────┘              └──────────────┘
```

## Division de voltaje (senales 5V → 3.3V)

```
DUT 5V ────[2.2kΩ]────┬──── GPIO Pi 3.3V
                       │
                      [1kΩ]
                       │
                      GND
```

## Indicador LED opcional
- GPIO26 + LED + 330Ω → indicador de muestreo activo (parpadea al capturar)
- GPIO19 + LED + 330Ω → indicador de trigger disparado

## Instalacion de software (todas las Pi)

```bash
# Sistema base (32 o 64 bits)
sudo apt update && sudo apt install -y python3 python3-pip python3-venv gpiod

# Dependencias Python
python3 -m venv ~/logic-env
source ~/logic-env/bin/activate
pip install websockets gpiod

# Verificar gpiod
gpioinfo | head -30
```

## Verificacion rapida
```bash
# Probar lectura de GPIO17 con comando
gpioget gpiochip0 17

# Esperar evento en GPIO17
gpio-mon gpiochip0 17
```
