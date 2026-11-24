#!/usr/bin/env bash
# ==============================================================================
# IIT Mandi Power Line Crawler Robot - Systemd Service Installer
# ==============================================================================

set -e

SERVICE_FILE="/etc/systemd/system/crawler-robot.service"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROBOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CURRENT_USER="$USER"

echo "[+] Creating systemd service file at $SERVICE_FILE..."

sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=IIT Mandi Power Line Inspection Crawler Robot Backend Server
After=network.target network-online.target

[Service]
Type=simple
User=$CURRENT_USER
WorkingDirectory=$ROBOT_DIR
ExecStart=$ROBOT_DIR/venv/bin/python3 $ROBOT_DIR/run_robot.py
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
EOF

echo "[+] Reloading systemd daemon and enabling crawler-robot service..."
sudo systemctl daemon-reload
sudo systemctl enable crawler-robot.service

echo ""
echo "=============================================================================="
echo "  SERVICE INSTALLED & ENABLED ON BOOT!"
echo "  Commands to manage service:"
echo "    Start:   sudo systemctl start crawler-robot"
echo "    Stop:    sudo systemctl stop crawler-robot"
echo "    Restart: sudo systemctl restart crawler-robot"
echo "    Status:  sudo systemctl status crawler-robot"
echo "    Logs:    journalctl -u crawler-robot -f"
echo "=============================================================================="
