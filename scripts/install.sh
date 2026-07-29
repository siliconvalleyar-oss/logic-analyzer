#!/bin/bash
#==============================================================================
# install.sh — Logic Analyzer RPi Installation Script
#
# Usage:
#   ./scripts/install.sh              # Install to /opt/logic-analyzer
#   ./scripts/install.sh --prefix ~/local  # Custom prefix
#   ./scripts/install.sh --help
#==============================================================================

set -euo pipefail

VERSION="1.0.0"
PREFIX="${PREFIX:-/opt/logic-analyzer}"
CONFIG_DIR="/etc/logic-analyzer"

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

print_ok()   { echo -e "${GREEN}[OK]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_err()  { echo -e "${RED}[ERROR]${NC} $1"; }

usage() {
    cat <<EOF
Logic Analyzer RPi v$VERSION — Installation

Usage: $0 [options]

Options:
  --prefix <dir>   Installation directory (default: /opt/logic-analyzer)
  --config <dir>   Config directory (default: /etc/logic-analyzer)
  --help           Show this help

Examples:
  $0                          # Default install
  $0 --prefix ~/logic         # User install (no root)
  $0 --prefix /usr/local      # System-wide install
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --config) CONFIG_DIR="$2"; shift 2 ;;
        --help) usage ;;
        *) print_err "Unknown option: $1"; usage ;;
    esac
done

echo "=============================================="
echo "  Logic Analyzer RPi v$VERSION"
echo "  Installation"
echo "=============================================="

# Check architecture
ARCH=$(uname -m)
case "$ARCH" in
    armv7l|armhf)   echo "  Arch: 32-bit ARM" ;;
    aarch64|arm64)  echo "  Arch: 64-bit ARM" ;;
    x86_64)         echo "  Arch: x86_64 (simulation mode)" ;;
    *)              print_warn "Unsupported arch: $ARCH (simulation only)" ;;
esac

# Dependencies
echo ""
echo "Step 1: Installing dependencies..."
if command -v apt &>/dev/null; then
    sudo apt update
    sudo apt install -y build-essential cmake g++ libssl-dev
    print_ok "Dependencies installed"
elif command -v brew &>/dev/null; then
    brew install cmake
    print_ok "Dependencies installed (brew)"
else
    print_warn "Package manager not detected. Install build-essential, cmake, g++ manually."
fi

# Build
echo ""
echo "Step 2: Building..."
cd "$(dirname "$0")/../server"
make clean 2>/dev/null || true
make -j$(nproc)
print_ok "Build complete"

# Install
echo ""
echo "Step 3: Installing to $PREFIX..."
sudo mkdir -p "$PREFIX/bin" "$PREFIX/web" "$CONFIG_DIR"
sudo cp logic_server "$PREFIX/bin/"
sudo cp ../web/index.html "$PREFIX/web/"
sudo cp ../config.json "$CONFIG_DIR/" 2>/dev/null || print_warn "No config.json found, skipping"

# Systemd service
if [[ -d /etc/systemd/system ]]; then
    echo ""
    echo "Step 4: Installing systemd service..."
    sudo tee /etc/systemd/system/logic-analyzer.service > /dev/null <<'SVC'
[Unit]
Description=Logic Analyzer Server
Documentation=https://github.com/siliconvalleyar-oss/logic_analizer_rpi
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=PREFIX/bin/logic_server --config CONFIG_DIR/config.json
Restart=on-failure
RestartSec=5
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
SVC
    sudo sed -i "s|PREFIX|$PREFIX|g; s|CONFIG_DIR|$CONFIG_DIR|g" /etc/systemd/system/logic-analyzer.service
    sudo systemctl daemon-reload
    print_ok "Service installed. Start with: sudo systemctl start logic-analyzer"
fi

echo ""
echo "=============================================="
echo "  Installation complete!"
echo "=============================================="
echo "  Binary: $PREFIX/bin/logic_server"
echo "  Config: $CONFIG_DIR/config.json"
echo "  Web:    $PREFIX/web/index.html"
echo "  URL:    http://$(hostname):8080"
echo "=============================================="
