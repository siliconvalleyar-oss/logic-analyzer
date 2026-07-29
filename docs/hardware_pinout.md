# Pinout — Asignacion de GPIOs

## Pines del ADC

| GPIO | Funcion | Pin Fisico (Pico) | Notas |
|------|---------|-------------------|-------|
| 26 | ADC0 — Canal 1 (CH1) | Pin 31 | Entrada analogica principal |
| 27 | ADC1 — Canal 2 (CH2) | Pin 32 | Entrada analogica secundaria |
| 28 | ADC2 | Pin 34 | No usado por defecto, disponible |

**Importante**: Los pines ADC solo aceptan 0 - 3.3V. Voltajes superiores danan la Pico.

## Pines I2C para ADS1115

| Plataforma | Bus I2C | SDA (GPIO) | SCL (GPIO) | Pines Fisicos |
|-----------|---------|------------|------------|---------------|
| Pico W | I2C0 | GP4 (pin 6) | GP5 (pin 7) | — |
| Pico W | I2C1 | GP6 (pin 9) | GP7 (pin 10) | — |
| Pi 2W | I2C1 | GPIO 2 (pin 3) | GPIO 3 (pin 5) | Header J8 |
| Pi 4 | I2C1 | GPIO 2 (pin 3) | GPIO 3 (pin 5) | Header GPIO 40-pin |

Recomendado: usar I2C0 en Pico W (GP4/GP5) e I2C1 en Pi (GPIO 2/3).

## Conexion ADS1115

```
ADS1115    →    Pico W
─────────────────────────
VDD       →    3.3V (pin 36)
GND       →    GND (pin 38)
SCL       →    GP5 (I2C0 SCL, pin 7)
SDA       →    GP4 (I2C0 SDA, pin 6)
ADDR      →    GND (0x48)

AIN0      →    Senal CH1
AIN1      →    Senal CH2
```

## Pines Digitales

| GPIO | Funcion | Pin Fisico | Notas |
|------|---------|-----------|-------|
| 0 | UART TX (debug) | Pin 1 | Salida serial de diagnostico |
| 1 | UART RX (debug) | Pin 2 | Entrada serial |
| 16 | (Libre) | Pin 21 | Se puede reasignar en firmware |
| 17 | (Libre) | Pin 22 | Se puede reasignar en firmware |
| 22 | Test Signal Out (PWM) | Pin 29 | Salida de onda cuadrada 1kHz |
| 25 | LED interno | Pin 37 | LED integrado en la placa |

## Pines de Alimentacion

| Pin | Nombre | Descripcion |
|-----|--------|-------------|
| 39 | VSYS | Entrada de alimentacion 5V DC |
| 40 | VBUS | 5V del USB (solo cuando USB conectado) |
| 36 | 3V3_OUT | Salida regulada 3.3V (max 300 mA) |
| 38 | GND | Tierra |
| 3, 8, 13, 18, 23, 28, 33 | GND | Pines de tierra adicionales |

## Pines para Front-End Analogico (cuando se usan)

| GPIO | Funcion | Descripcion |
|------|---------|-------------|
| 2, 3 | CH1 Voltage Range | Entradas para seleccion de rango CH1 |
| 4, 5 | CH2 Voltage Range | Entradas para seleccion de rango CH2 |
| 14 | Wi-Fi Status | LED indicador de estado Wi-Fi |
| 15 | Trigger Status | LED indicador de trigger |

Estos pines se utilizan cuando se conecta un front-end analogico con seleccion automatica de rango.

## Resumen Visual

```
                    Raspberry Pi Pico
                    ┌──────────────────┐
  GPIO 0  (UART TX) │ Pin 1      Pin 40│ VBUS (5V USB)
  GPIO 1  (UART RX) │ Pin 2      Pin 39│ VSYS (5V DC in)
               GND  │ Pin 3      Pin 38│ GND
               ...  │ ...        ...   │ ...
  GPIO 22  (TEST)   │ Pin 29     Pin 31│ GPIO 26 (ADC0 CH1)
                    │            Pin 32│ GPIO 27 (ADC1 CH2)
  GPIO 25  (LED)    │ Pin 37     Pin 34│ GPIO 28 (ADC2)
                    └──────────────────┘
```

Consulta la [Hoja de Datos Oficial del RP2040](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) para informacion detallada.
