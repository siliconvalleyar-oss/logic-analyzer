#!/bin/bash
#===============================================================================
# install-service.sh — Instala el servicio systemd del Analizador Logico
#
# Este script copia logic-analyzer.service a /etc/systemd/system/,
# recarga systemd, habilita el inicio automatico y (opcionalmente) lo arranca.
#
# Usage:
#   sudo ./script_tools/install-service.sh              # Instalar + habilitar + iniciar
#   sudo ./script_tools/install-service.sh --no-start   # Instalar + habilitar, NO iniciar
#   sudo ./script_tools/install-service.sh --uninstall   # Detener + deshabilitar + borrar
#   ./script_tools/install-service.sh --status           # Ver estado (no necesita sudo)
#   ./script_tools/install-service.sh --help             # Mostrar ayuda
#
# Requiere: systemd, sudo
#===============================================================================

set -euo pipefail

SERVICE_NAME="logic-analyzer"
SERVICE_FILE="${SERVICE_NAME}.service"
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_PATH="${SOURCE_DIR}/${SERVICE_FILE}"
TARGET_PATH="/etc/systemd/system/${SERVICE_FILE}"

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
ok()   { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()  { echo -e "${RED}[ERROR]${NC} $1"; }
info() { echo -e "${CYAN}[INFO]${NC} $1"; }

usage() {
    cat <<EOF
Logic Analyzer — systemd Service Installer

Usage: sudo $0 [options]

Options:
  --no-start    Instala y habilita, pero no inicia el servicio
  --uninstall   Detiene, deshabilita y elimina el servicio
  --status      Muestra el estado del servicio (no requiere sudo)
  --help        Muestra esta ayuda

Examples:
  sudo $0                              # Instalar, habilitar e iniciar
  sudo $0 --no-start                   # Solo instalar y habilitar
  sudo $0 --uninstall                  # Eliminar completamente
  $0 --status                          # Ver estado
EOF
    exit 0
}

do_status() {
    if systemctl is-active --quiet "${SERVICE_NAME}" 2>/dev/null; then
        ok "Servicio ${SERVICE_NAME} esta ACTIVO"
    elif systemctl is-enabled --quiet "${SERVICE_NAME}" 2>/dev/null; then
        warn "Servicio ${SERVICE_NAME} esta HABILITADO pero INACTIVO"
    else
        err "Servicio ${SERVICE_NAME} NO INSTALADO"
    fi
    echo ""
    systemctl status "${SERVICE_NAME}" 2>&1 || true
}

do_uninstall() {
    echo "=== Desinstalando servicio ${SERVICE_NAME} ==="

    if systemctl is-active --quiet "${SERVICE_NAME}" 2>/dev/null; then
        echo "Deteniendo servicio..."
        sudo systemctl stop "${SERVICE_NAME}"
        ok "Servicio detenido"
    fi

    if systemctl is-enabled --quiet "${SERVICE_NAME}" 2>/dev/null; then
        echo "Deshabilitando inicio automatico..."
        sudo systemctl disable "${SERVICE_NAME}"
        ok "Inicio automatico deshabilitado"
    fi

    if [[ -f "${TARGET_PATH}" ]]; then
        echo "Eliminando archivo de unidad..."
        sudo rm -f "${TARGET_PATH}"
        ok "Archivo ${TARGET_PATH} eliminado"
    fi

    sudo systemctl daemon-reload
    echo ""
    ok "Servicio ${SERVICE_NAME} desinstalado correctamente"
}

do_install() {
    local start_service="${1:-true}"

    echo "=============================================="
    echo "  Logic Analyzer — systemd Service Installer"
    echo "=============================================="
    echo ""

    # Verificar que el archivo fuente existe
    if [[ ! -f "${SOURCE_PATH}" ]]; then
        err "No se encuentra ${SOURCE_PATH}"
        err "Ejecuta este script desde el directorio raiz del proyecto:"
        err "  sudo ./script_tools/install-service.sh"
        exit 1
    fi

    # Verificar que el binario existe (opcional)
    local binary_path
    binary_path="$(cd "${SOURCE_DIR}/../server" && pwd)/logic_server"
    if [[ ! -f "${binary_path}" ]]; then
        warn "Binario no encontrado en ${binary_path}"
        warn "Compila primero con: cd server && make"
        echo -n "Continuar de todas formas? [s/N] "
        read -r resp
        if [[ "${resp}" != "s" && "${resp}" != "S" ]]; then
            err "Instalacion cancelada"
            exit 1
        fi
    fi

    # Verificar que no estamos ejecutando como root (aunque sudo es necesario)
    if [[ "${EUID}" -eq 0 ]]; then
        warn "Ejecutando como root directamente. Usa 'sudo' como usuario normal."
    fi

    echo "Step 1: Instalando archivo de unidad..."
    sudo cp "${SOURCE_PATH}" "${TARGET_PATH}"
    sudo chmod 644 "${TARGET_PATH}"
    ok "Archivo copiado a ${TARGET_PATH}"

    echo "Step 2: Recargando systemd..."
    sudo systemctl daemon-reload
    ok "systemd recargado"

    echo "Step 3: Habilitando inicio automatico..."
    sudo systemctl enable "${SERVICE_NAME}"
    ok "Servicio habilitado (arranque automatico en boot)"

    if [[ "${start_service}" == "true" ]]; then
        echo "Step 4: Matando procesos anteriores..."
        sudo killall -9 logic_server 2>/dev/null || true
        sleep 1
        echo "Step 5: Iniciando servicio..."
        if sudo systemctl start "${SERVICE_NAME}"; then
            ok "Servicio iniciado correctamente"
        else
            warn "El servicio no arranco. Ver logs: sudo journalctl -u ${SERVICE_NAME} -f"
        fi
    else
        echo "Step 4: Omitiendo inicio (--no-start)"
        info "Inicia manualmente: sudo systemctl start ${SERVICE_NAME}"
    fi

    echo ""
    echo "=============================================="
    echo "  Instalacion completada"
    echo "=============================================="
    echo "  Servicio:  ${SERVICE_NAME}"
    echo "  Unidad:    ${TARGET_PATH}"
    echo "  Comandos:"
    echo "    sudo systemctl start ${SERVICE_NAME}"
    echo "    sudo systemctl stop ${SERVICE_NAME}"
    echo "    sudo systemctl restart ${SERVICE_NAME}"
    echo "    sudo systemctl status ${SERVICE_NAME}"
    echo "    sudo journalctl -u ${SERVICE_NAME} -f"
    echo "  URL:       http://$(hostname):8080"
    echo "=============================================="
}

# === Main ===

if [[ "${EUID}" -ne 0 ]] && [[ "$*" != *"--help"* ]] && [[ "$*" != *"--status"* ]]; then
    err "Este script requiere sudo para la mayoria de las operaciones."
    err "Reintenta: sudo $0 $*"
    echo ""
    echo "  Uso sin sudo: $0 --status   (solo ver estado)"
    echo "  Uso con sudo: sudo $0        (instalar)"
    exit 1
fi

case "${1:-}" in
    --help)     usage ;;
    --status)   do_status ;;
    --uninstall) do_uninstall ;;
    --no-start) do_install false ;;
    "")         do_install true ;;
    *)
        err "Opcion desconocida: $1"
        usage
        ;;
esac
