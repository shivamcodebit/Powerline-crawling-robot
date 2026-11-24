# Raspberry Pi 4B Setup Guide

**Project:** IIT Mandi Power Line Inspection Crawler Robot  
**Host Controller:** Raspberry Pi 4B (4GB or 8GB recommended)  
**Operating System:** Raspberry Pi OS 64-bit (Debian Bookworm or Bullseye)

---

## 1. Physical Hardware Connections

1. **Power Supply:** Provide 5V 3A to the Raspberry Pi 4B via USB-C (use a reliable 5V step-down buck converter from the main battery).
2. **STM32 Nucleo Connection:** Connect the STM32 Nucleo-F446RE to any USB 3.0 or USB 2.0 port on the Raspberry Pi using a micro-USB cable. This supplies power to the Nucleo and provides the ST-LINK Virtual COM Port (`/dev/ttyACM0`).
3. **Camera Connection:** Connect the Raspberry Pi Camera Module v2 or v3 into the 15-pin CSI camera ribbon connector with the blue tape facing the Ethernet/USB ports.

---

## 2. Operating System & Interface Configuration

### Step 1: Flash OS
Using **Raspberry Pi Imager**:
- OS: `Raspberry Pi OS (64-bit)`
- Set hostname to `crawler` (enables access via `http://crawler.local:8002`)
- Configure Wi-Fi credentials and enable SSH.

### Step 2: Enable Camera Interface
On Debian Bookworm, cameras using the modern libcamera stack are auto-detected.
Verify in `/boot/firmware/config.txt` (or `/boot/config.txt` on older OS):
```ini
camera_auto_detect=1
```
Test the camera using:
```bash
rpicam-hello -t 3000
```

---

## 3. Automated Software Installation

Clone or copy the `final_robot` repository to your Raspberry Pi home directory:
```bash
cd ~
git clone <repo_url> final_robot
cd final_robot
```

Run the automated installer script:
```bash
chmod +x scripts/*.sh
bash scripts/install_pi.sh
```

This script will:
- Install Python 3, `libcamera-apps`, and `python3-picamera2`.
- Add user `pi` to the `dialout` and `video` groups.
- Set up a virtual environment with system site-packages.
- Install dependencies from `requirements.txt`.
- Install udev rules so the STM32 Nucleo is always available at `/dev/robot_stm32`.

Log out and back in (or reboot) for user group changes to take effect:
```bash
sudo reboot
```

---

## 4. Running the System

### Manual Foreground Launch:
```bash
cd ~/final_robot
bash scripts/start_robot.sh
```

### Automatic Headless Launch on Boot (Systemd):
```bash
cd ~/final_robot
bash scripts/setup_systemd.sh
```

To manage the background service:
```bash
# Check status
sudo systemctl status crawler-robot

# Restart service
sudo systemctl restart crawler-robot

# View live system logs
journalctl -u crawler-robot -f
```

---

## 5. Accessing Dashboards in Your Browser

Connect any laptop, tablet, or smartphone to the same Wi-Fi network as the Raspberry Pi:

- **Dashboard 1 (Robot Control):**  
  `http://<RASPBERRY_PI_IP>:8002`  or  `http://crawler.local:8002`

- **Dashboard 2 (Live Monitoring & Diagnostics):**  
  `http://<RASPBERRY_PI_IP>:8003`  or  `http://crawler.local:8003`
