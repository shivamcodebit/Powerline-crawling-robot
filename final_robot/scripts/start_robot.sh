#!/usr/bin/env bash
# ==============================================================================
# IIT Mandi Power Line Crawler Robot - Linux / Raspberry Pi Launcher
# ==============================================================================

set -e

# Navigate to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

echo "=============================================================================="
echo "  IIT Mandi Power Line Inspection Crawler Robot - Starting Services..."
echo "=============================================================================="

# Check Python environment
if [ -d "venv" ]; then
    echo "[+] Activating virtual environment (venv)..."
    source venv/bin/activate
fi

# Run backend
python3 run_robot.py
