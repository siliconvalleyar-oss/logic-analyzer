#!/bin/bash
#===============================================================================
# gpio_test.sh — Generador de senales de prueba GPIO para el Analizador Logico
#
# Genera ondas digitales en los GPIOs 2-7 para verificar el funcionamiento
# del analizador logico sin necesidad de conectar señales externas.
#
# Senales generadas:
#   GPIO2: 1 kHz     (periodo 1ms)  — onda cuadrada rapida
#   GPIO3: 500 Hz    (periodo 2ms)  — onda cuadrada media, invertida vs GPIO2
#   GPIO4: 100 Hz    (periodo 10ms) — onda cuadrada lenta
#   GPIO5: 10101010                 — patron binario (200ms por bit)
#   GPIO6: 11001100                 — patron binario (250ms por bit)
#   GPIO7: Heartbeat                — 1s HIGH, 1s LOW
#
# Usage:
#   ./scripts/gpio_test.sh                  # Iniciar generacion (foreground)
#   ./scripts/gpio_test.sh --daemon         # Iniciar en background (daemon)
#   ./scripts/gpio_test.sh --stop           # Detener generacion
#   ./scripts/gpio_test.sh --status         # Ver estado
#   ./scripts/gpio_test.sh --help           # Mostrar ayuda
#
# Requiere: pinctrl (Raspberry Pi), o simula en x86
#===============================================================================

set -euo pipefail

# === CONFIGURACION ===
SCRIPT_NAME="gpio_test"
PID_FILE="/tmp/${SCRIPT_NAME}.pid"
LOG_FILE="/var/log/${SCRIPT_NAME}.log"
PINS_TEST=(2 3 4 5 6 7)

# Patrones predefinidos (fuera del loop para eficiencia)
P5_PATTERN=(1 0 1 0 1 0 1 0)   # GPIO5: 10101010
P6_PATTERN=(1 1 0 0 1 1 0 0)   # GPIO6: 11001100
# GPIO7: heartbeat con contador

# === COLORS ===
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
ok()   { echo -e "${GREEN}[OK]${NC}   $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()  { echo -e "${RED}[ERROR]${NC} $1"; }
info() { echo -e "${CYAN}[INFO]${NC} $1"; }
log()  { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "${LOG_FILE}"; }

# === DETECCION DE PLATAFORMA ===
is_rpi() {
    grep -iq "Raspberry Pi\|BCM\|bcm2835\|BCM2711\|BCM2712" /proc/cpuinfo 2>/dev/null && return 0
    [[ -c /dev/gpiomem ]] && return 0
    return 1
}

has_pinctrl() {
    command -v pinctrl &>/dev/null
}

# === LIMPIEZA ===
cleanup() {
    local exit_code=${1:-0}
    info "Deteniendo generacion de senales de prueba..."

    # Restaurar todos los pines a input (seguro)
    for pin in "${PINS_TEST[@]}"; do
        if is_rpi && has_pinctrl; then
            pinctrl set "${pin}" ip pn 2>/dev/null || true
        fi
    done

    rm -f "${PID_FILE}" 2>/dev/null || true
    log "Test signals stopped (exit=${exit_code})"
    ok "GPIOs restaurados a input"
    exit "${exit_code}"
}

# === RESTAURAR GPIOs (sin salir) ===
restore_gpios() {
    for pin in "${PINS_TEST[@]}"; do
        if is_rpi && has_pinctrl; then
            pinctrl set "${pin}" ip pn 2>/dev/null || true
        fi
    done
    log "GPIOs restored to input"
}

# === INICIALIZACION GPIO ===
init_gpio() {
    if ! is_rpi; then
        warn "No se detecto Raspberry Pi — modo simulacion"
        log "WARNING: Not running on Raspberry Pi, simulation mode"
        return 0
    fi

    if ! has_pinctrl; then
        err "pinctrl no encontrado. Instalar: sudo apt install raspi-gpio"
        exit 1
    fi

    info "Configurando GPIOs como salidas..."
    for pin in "${PINS_TEST[@]}"; do
        pinctrl set "${pin}" op pn dl
        log "GPIO${pin} = output LOW"
    done
    ok "GPIOs ${PINS_TEST[*]} configurados como salidas"
}

# === BUCLE PRINCIPAL DE GENERACION ===
generate_signals() {
    # El trap SOLO se activa en este modo (no en --help, --status, etc.)
    trap 'cleanup 0' SIGTERM SIGINT EXIT

    if ! is_rpi; then
        # Modo simulacion: solo log
        while true; do
            log "SIMULATION: GPIO toggling (no hardware)"
            sleep 1
        done
    fi

    info "Generando senales de prueba (Ctrl+C para detener)..."
    log "Test signal generation started"

    # Contador principal para frecuencias relativas
    # Cada iteracion dura ~2ms (sleep 0.002 + overhead pinctrl)
    local counter=0

    # Timestamp para patrones lentos (evita acumular error)
    local t_start
    t_start=$(date +%s%3N)

    while true; do
        # === PINES RAPIDOS: usan contador (frecuencias relativas estables) ===
        # GPIO2: 1kHz — toggle cada ~500us (1 tick del contador)
        if (( counter % 2 == 0 )); then
            pinctrl set 2 dh
        else
            pinctrl set 2 dl
        fi

        # GPIO3: 500Hz — toggle cada ~1000us (2 ticks), invertido vs GPIO2
        if (( (counter / 2) % 2 == 0 )); then
            pinctrl set 3 dl  # invertido
        else
            pinctrl set 3 dh
        fi

        # GPIO4: 100Hz — toggle cada ~5000us (5 ticks)
        if (( (counter / 5) % 2 == 0 )); then
            pinctrl set 4 dh
        else
            pinctrl set 4 dl
        fi

        # === PINES LENTOS: usan timestamp absoluto para precision ===
        local elapsed_ms=$(( $(date +%s%3N) - t_start ))
        if (( elapsed_ms < 0 )); then
            # wraparound
            t_start=$(date +%s%3N)
            elapsed_ms=0
        fi

        # GPIO5: Pattern 10101010 (cada 200ms)
        local pat5=$(( (elapsed_ms / 200) % 8 ))
        if (( P5_PATTERN[pat5] == 1 )); then
            pinctrl set 5 dh
        else
            pinctrl set 5 dl
        fi

        # GPIO6: Pattern 11001100 (cada 250ms)
        local pat6=$(( (elapsed_ms / 250) % 8 ))
        if (( P6_PATTERN[pat6] == 1 )); then
            pinctrl set 6 dh
        else
            pinctrl set 6 dl
        fi

        # GPIO7: Heartbeat (1s HIGH, 1s LOW)
        if (( (elapsed_ms / 1000) % 2 == 0 )); then
            pinctrl set 7 dh
        else
            pinctrl set 7 dl
        fi

        counter=$(( counter + 1 ))
        sleep 0.002  # 2ms — baseline del loop
    done
}

# === COMANDOS ===
usage() {
    cat <<EOF
Generador de senales de prueba GPIO para Logic Analyzer

Usage: ${0##*/} [options]

Options:
  --daemon    Iniciar en background (daemon)
  --stop      Detener el daemon
  --status    Ver estado del daemon
  --help      Mostrar esta ayuda

Senales:
  GPIO2:  1 kHz    — onda cuadrada rapida (periodo 1ms)
  GPIO3:  500 Hz   — onda cuadrada media, invertida respecto a GPIO2
  GPIO4:  100 Hz   — onda cuadrada lenta (periodo 10ms)
  GPIO5:  10101010 — patron binario (200ms por bit)
  GPIO6:  11001100 — patron binario (250ms por bit)
  GPIO7:  Heartbeat — 1s HIGH, 1s LOW (visual)

Ejemplos:
  ${0##*/}              # Iniciar en primer plano
  ${0##*/} --daemon     # Iniciar como daemon
  ${0##*/} --stop       # Detener
  ${0##*/} --status     # Ver estado
EOF
    exit 0
}

do_daemon() {
    if [[ -f "${PID_FILE}" ]]; then
        local old_pid
        old_pid=$(cat "${PID_FILE}" 2>/dev/null || echo "")
        if [[ -n "${old_pid}" ]] && kill -0 "${old_pid}" 2>/dev/null; then
            err "Ya hay una instancia corriendo (PID ${old_pid})"
            err "Detener con: ${0##*/} --stop"
            exit 1
        fi
        rm -f "${PID_FILE}"
    fi

    info "Iniciando generacion de senales en background..."
    nohup bash "$0" --foreground </dev/null >> "${LOG_FILE}" 2>&1 &
    local pid=$!
    echo "${pid}" > "${PID_FILE}"
    ok "Generacion iniciada (PID ${pid})"
    info "Log: ${LOG_FILE}"
}

do_stop() {
    if [[ ! -f "${PID_FILE}" ]]; then
        warn "No se encontro archivo PID en ${PID_FILE}"
        restore_gpios
        exit 1
    fi

    local pid
    pid=$(cat "${PID_FILE}" 2>/dev/null || echo "")

    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
        info "Deteniendo generacion (PID ${pid})..."
        kill "${pid}" 2>/dev/null || true
        sleep 1
        if kill -0 "${pid}" 2>/dev/null; then
            warn "No respondio, forzando..."
            kill -9 "${pid}" 2>/dev/null || true
        fi
        ok "Generacion detenida"
    else
        warn "No se encontro proceso corriendo"
    fi

    # Restaurar GPIOs localmente por si el cleanup del proceso hijo no alcanzo
    restore_gpios
    rm -f "${PID_FILE}"
    ok "GPIOs restaurados a input"
}

do_status() {
    if [[ -f "${PID_FILE}" ]]; then
        local pid
        pid=$(cat "${PID_FILE}" 2>/dev/null || echo "")
        if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
            ok "Generacion ACTIVA (PID ${pid})"
            echo ""
            echo "GPIO states:"
            for pin in "${PINS_TEST[@]}"; do
                if is_rpi && has_pinctrl; then
                    pinctrl get "${pin}" 2>/dev/null | head -1
                fi
            done
            echo ""
            echo "Ultimas lineas del log:"
            tail -5 "${LOG_FILE}" 2>/dev/null || echo "(log vacio)"
        else
            warn "PID file existe pero el proceso no corre (PID ${pid})"
            rm -f "${PID_FILE}"
            restore_gpios
        fi
    else
        info "Generacion de prueba NO activa"
    fi
}

# === MAIN ===
case "${1:-}" in
    --help|-h)
        usage
        ;;
    --daemon)
        do_daemon
        ;;
    --stop)
        do_stop
        ;;
    --status)
        do_status
        ;;
    --foreground)
        # Modo interno para el daemon
        init_gpio
        generate_signals
        ;;
    "")
        # Modo foreground (default)
        init_gpio
        generate_signals
        ;;
    *)
        err "Opcion desconocida: $1"
        usage
        ;;
esac
