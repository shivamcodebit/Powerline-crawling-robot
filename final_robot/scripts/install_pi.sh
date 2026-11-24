#!/usr/bin/env bash
# ==============================================================================
# IIT Mandi Power Line Crawler Robot - Automated Raspberry Pi 4B Setup Script
# ==============================================================================

set -e

echo "=============================================================================="
echo "  IIT Mandi Power Line Inspection Crawler Robot - Raspberry Pi Installer"
echo "=============================================================================="

# 1. System Package Updates & Camera Stack
echo "[1/5] Installing core system libraries & camera stack..."
sudo apt-get update
sudo apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-pil \
    python3-picamera2 \
    libcamera-apps \
    v4l-utils \
    git

# 2. Configure Hardware Permissions
echo "[2/5] Adding user $USER to dialout and video hardware groups..."
sudo usermod -a -G dialout,video "$USER"

# 3. Setup Virtual Environment
echo "[3/5] Setting up Python virtual environment with system site packages..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

if [ ! -d "venv" ]; then
    python3 -m venv --system-site-packages venv
fi

source venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt

# 4. Install Udev Rules for STM32 ST-LINK
echo "[4/5] Installing STM32 Nucleo USB udev rules..."
if [ -f "scripts/99-stm32-nucleo.rules" ]; then
    sudo cp scripts/99-stm32-nucleo.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger
fi

# 5. Enable Camera in raspi-config
echo "[5/5] Checking camera interfaces..."
echo "To ensure Camera Module v2/v3 is enabled, ensure 'camera_auto_detect=1' is in /boot/firmware/config.txt."

echo ""
echo "=============================================================================="
echo "  INSTALLATION COMPLETE!"
echo "  Please log out and log back in (or reboot) for group permissions to apply."
echo "  To start the robot system: bash scripts/start_robot.sh"
echo "=============================================================================="
