/**
 * IIT Mandi Power Line Inspection Crawler Robot - Live Monitoring Dashboard App
 * Subscribes to 20Hz WebSocket telemetry and updates gauges, charts, and sensor tables.
 */

document.addEventListener('DOMContentLoaded', () => {
    // Top Bar Badges
    const badgeLink = document.getElementById('mon-badge-link');
    const dotLink = document.getElementById('mon-dot-link');
    const badgeState = document.getElementById('mon-badge-state');
    const badgeWdog = document.getElementById('mon-badge-wdog');
    const badgeRate = document.getElementById('mon-badge-rate');

    // Camera
    const cameraFeed = document.getElementById('camera-feed');
    const cameraStatusText = document.getElementById('camera-status-text');
    const btnCamStart = document.getElementById('btn-cam-start');
    const btnCamStop = document.getElementById('btn-cam-stop');
    const btnCamReconnect = document.getElementById('btn-cam-reconnect');

    // Motor Elements
    const txtCrawlA = document.getElementById('mot-crawl-a');
    const txtCrawlB = document.getElementById('mot-crawl-b');
    const txtGripA = document.getElementById('mot-grip-a');
    const txtGripB = document.getElementById('mot-grip-b');

    // Encoder Elements
    const txtEnc1Count = document.getElementById('enc1-count');
    const txtEnc1Rpm = document.getElementById('enc1-rpm');
    const badgeEnc1Status = document.getElementById('enc1-status');

    const txtEnc2Count = document.getElementById('enc2-count');
    const txtEnc2Rpm = document.getElementById('enc2-rpm');
    const badgeEnc2Status = document.getElementById('enc2-status');

    const txtWEnc1Count = document.getElementById('wenc1-count');
    const txtWEnc1Rpm = document.getElementById('wenc1-rpm');
    const badgeWEnc1Status = document.getElementById('wenc1-status');

    const txtWEnc2Count = document.getElementById('wenc2-count');
    const txtWEnc2Rpm = document.getElementById('wenc2-rpm');
    const badgeWEnc2Status = document.getElementById('wenc2-status');

    // IMU Elements
    const txtAccelX = document.getElementById('imu-ax');
    const txtAccelY = document.getElementById('imu-ay');
    const txtAccelZ = document.getElementById('imu-az');
    const txtGyroX = document.getElementById('imu-gx');
    const txtGyroY = document.getElementById('imu-gy');
    const txtGyroZ = document.getElementById('imu-gz');
    const txtImuTemp = document.getElementById('imu-temp');
    const txtImuRoll = document.getElementById('imu-roll');
    const txtImuPitch = document.getElementById('imu-pitch');
    const txtImuAddr = document.getElementById('imu-addr');

    // INA219 Elements
    const txtIna1Volt = document.getElementById('ina1-volt');
    const txtIna1Curr = document.getElementById('ina1-curr');
    const txtIna1Pwr = document.getElementById('ina1-pwr');
    const txtIna1Addr = document.getElementById('ina1-addr');

    const txtIna2Volt = document.getElementById('ina2-volt');
    const txtIna2Curr = document.getElementById('ina2-curr');
    const txtIna2Pwr = document.getElementById('ina2-pwr');
    const txtIna2Addr = document.getElementById('ina2-addr');

    // Diagnostics
    const diagMcuTime = document.getElementById('diag-mcu-time');
    const diagPktRate = document.getElementById('diag-pkt-rate');
    const diagTotalPkts = document.getElementById('diag-total-pkts');
    const diagDropped = document.getElementById('diag-dropped');
    const diagErrors = document.getElementById('diag-errors');
    const diagI2cScan = document.getElementById('diag-i2c-scan');

    const txtFooterTime = document.getElementById('footer-mon-time');

    // Initialize Gauges & Sparklines
    const attitudeGauge = new AttitudeIndicator('attitude-canvas');
    const voltSparkline = new TelemetrySparkline('spark-volt', 40, '#00d2ff', 'rgba(0,210,255,0.1)');
    const pwrSparkline = new TelemetrySparkline('spark-pwr', 40, '#00ff9d', 'rgba(0,255,157,0.1)');

    // -------------------------------------------------------------------------
    // WebSocket Connection
    // -------------------------------------------------------------------------
    let ws = null;

    function connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws/telemetry`;

        badgeLink.textContent = 'CONNECTING...';
        dotLink.className = 'dot dot-orange';

        ws = new WebSocket(wsUrl);

        ws.onopen = () => {
            badgeLink.textContent = 'TELEMETRY LIVE';
            dotLink.className = 'dot dot-green';
        };

        ws.onmessage = (event) => {
            try {
                const telemetry = JSON.parse(event.data);
                updateDashboard(telemetry);
            } catch (e) {
                console.error('Telemetry parse error:', e);
            }
        };

        ws.onclose = () => {
            badgeLink.textContent = 'TELEMETRY OFFLINE';
            dotLink.className = 'dot dot-red';
            setTimeout(connectWebSocket, 1500);
        };

        ws.onerror = () => {
            ws.close();
        };
    }

    // -------------------------------------------------------------------------
    // Telemetry DOM Update
    // -------------------------------------------------------------------------
    function updateDashboard(t) {
        if (!t) return;

        // Health & Link
        if (t.health) {
            const h = t.health;
            if (h.stm32_connected) {
                badgeLink.textContent = h.is_stale ? 'STM32 STALE' : 'STM32 ONLINE';
                dotLink.className = h.is_stale ? 'dot dot-orange' : 'dot dot-green';
            } else {
                badgeLink.textContent = 'STM32 DISCONNECTED';
                dotLink.className = 'dot dot-red';
            }

            badgeRate.textContent = `${(h.packet_rate_hz || 0).toFixed(1)} Hz`;
            diagPktRate.textContent = `${(h.packet_rate_hz || 0).toFixed(1)} Hz`;
            diagTotalPkts.textContent = `${h.total_packets || 0}`;
            diagDropped.textContent = `${h.dropped_packets || 0}`;
            diagErrors.textContent = `${h.error_count || 0}`;
        }

        // Arming / State
        const stateStr = t.state || 'DISARMED';
        badgeState.textContent = stateStr;
        badgeState.className = 'state-val';
        if (stateStr === 'ARMED') {
            badgeState.classList.add('state-armed');
        } else if (stateStr === 'ESTOP_LATCHED') {
            badgeState.classList.add('state-estop');
        } else {
            badgeState.classList.add('state-disarmed');
        }

        // Watchdog
        badgeWdog.textContent = t.watchdog_tripped ? 'WDOG: TRIPPED' : 'WDOG: OK';
        badgeWdog.style.color = t.watchdog_tripped ? '#ef4444' : '#10b981';

        // Motors Commanded
        if (t.motors) {
            txtCrawlA.textContent = `${t.motors.crawler_a > 0 ? '+' : ''}${t.motors.crawler_a}%`;
            txtCrawlB.textContent = `${t.motors.crawler_b > 0 ? '+' : ''}${t.motors.crawler_b}%`;
            txtGripA.textContent = `${t.motors.gripper_a > 0 ? '+' : ''}${t.motors.gripper_a}%`;
            txtGripB.textContent = `${t.motors.gripper_b > 0 ? '+' : ''}${t.motors.gripper_b}%`;
        }

        // Encoders (Counts & RPM)
        if (t.encoders) {
            const e = t.encoders;

            // ENC 1
            if (e.enc1) {
                txtEnc1Count.textContent = e.enc1.count;
                txtEnc1Rpm.textContent = e.enc1.calibrated ? `${e.enc1.rpm.toFixed(1)} RPM` : '0.0 RPM (Uncalibrated)';
                badgeEnc1Status.className = e.enc1.calibrated ? 'cal-badge' : 'uncal-badge';
                badgeEnc1Status.textContent = e.enc1.calibrated ? 'CALIBRATED' : 'UNCALIBRATED (CPR=0)';
            }

            // ENC 2
            if (e.enc2) {
                txtEnc2Count.textContent = e.enc2.count;
                txtEnc2Rpm.textContent = e.enc2.calibrated ? `${e.enc2.rpm.toFixed(1)} RPM` : '0.0 RPM (Uncalibrated)';
                badgeEnc2Status.className = e.enc2.calibrated ? 'cal-badge' : 'uncal-badge';
                badgeEnc2Status.textContent = e.enc2.calibrated ? 'CALIBRATED' : 'UNCALIBRATED (CPR=0)';
            }

            // WENC 1
            if (e.wenc1) {
                txtWEnc1Count.textContent = e.wenc1.count;
                txtWEnc1Rpm.textContent = e.wenc1.calibrated ? `${e.wenc1.rpm.toFixed(1)} RPM` : '0.0 RPM (Uncalibrated)';
                badgeWEnc1Status.className = e.wenc1.calibrated ? 'cal-badge' : 'uncal-badge';
                badgeWEnc1Status.textContent = e.wenc1.calibrated ? 'CALIBRATED' : 'UNCALIBRATED (CPR=0)';
            }

            // WENC 2
            if (e.wenc2) {
                txtWEnc2Count.textContent = e.wenc2.count;
                txtWEnc2Rpm.textContent = e.wenc2.calibrated ? `${e.wenc2.rpm.toFixed(1)} RPM` : '0.0 RPM (Uncalibrated)';
                badgeWEnc2Status.className = e.wenc2.calibrated ? 'cal-badge' : 'uncal-badge';
                badgeWEnc2Status.textContent = e.wenc2.calibrated ? 'CALIBRATED' : 'UNCALIBRATED (CPR=0)';
            }
        }

        // MPU9250 IMU
        if (t.imu) {
            const imu = t.imu;
            txtAccelX.textContent = `${imu.ax.toFixed(2)} m/s²`;
            txtAccelY.textContent = `${imu.ay.toFixed(2)} m/s²`;
            txtAccelZ.textContent = `${imu.az.toFixed(2)} m/s²`;

            txtGyroX.textContent = `${imu.gx.toFixed(2)} °/s`;
            txtGyroY.textContent = `${imu.gy.toFixed(2)} °/s`;
            txtGyroZ.textContent = `${imu.gz.toFixed(2)} °/s`;

            txtImuTemp.textContent = `${imu.temp.toFixed(1)} °C`;
            txtImuRoll.textContent = `${imu.roll.toFixed(1)}°`;
            txtImuPitch.textContent = `${imu.pitch.toFixed(1)}°`;
            txtImuAddr.textContent = imu.connected ? `${imu.address} (ONLINE)` : 'OFFLINE';

            // Update 2D Artificial Horizon
            attitudeGauge.update(imu.pitch, imu.roll);
        }

        // INA219 #1
        if (t.ina219_1) {
            const ina1 = t.ina219_1;
            txtIna1Volt.textContent = ina1.connected ? `${ina1.voltage_v.toFixed(2)} V` : 'OFFLINE';
            txtIna1Curr.textContent = ina1.connected ? `${ina1.current_ma.toFixed(1)} mA` : '0 mA';
            txtIna1Pwr.textContent = ina1.connected ? `${(ina1.power_mw / 1000.0).toFixed(2)} W` : '0 W';
            txtIna1Addr.textContent = ina1.connected ? `ADDR: ${ina1.address}` : 'NOT DETECTED';

            if (ina1.connected) {
                voltSparkline.push(ina1.voltage_v);
            }
        }

        // INA219 #2
        if (t.ina219_2) {
            const ina2 = t.ina219_2;
            txtIna2Volt.textContent = ina2.connected ? `${ina2.voltage_v.toFixed(2)} V` : 'OFFLINE';
            txtIna2Curr.textContent = ina2.connected ? `${ina2.current_ma.toFixed(1)} mA` : '0 mA';
            txtIna2Pwr.textContent = ina2.connected ? `${(ina2.power_mw / 1000.0).toFixed(2)} W` : '0 W';
            txtIna2Addr.textContent = ina2.connected ? `ADDR: ${ina2.address}` : 'NOT DETECTED';

            if (ina2.connected) {
                pwrSparkline.push(ina2.power_mw / 1000.0);
            }
        }

        // I2C Devices Discovered
        if (t.i2c_scan) {
            diagI2cScan.textContent = t.i2c_scan.length > 0 ? t.i2c_scan.join(', ') : 'None detected';
        }

        // Timestamps
        diagMcuTime.textContent = `${t.timestamp || 0} ms`;
        if (txtFooterTime) {
            txtFooterTime.textContent = `MCU CLOCK: ${t.timestamp || 0} ms | PACKET SEQ: ${t.seq || 0}`;
        }
    }

    // -------------------------------------------------------------------------
    // Camera Stream Controls
    // -------------------------------------------------------------------------
    function refreshCameraStatus() {
        fetch('/api/camera/status')
            .then(res => res.json())
            .then(data => {
                cameraStatusText.textContent = `${data.status} | ${data.resolution} @ ${data.fps} FPS`;
            })
            .catch(() => {
                cameraStatusText.textContent = 'CAMERA SERVICE UNREACHABLE';
            });
    }

    btnCamStart.addEventListener('click', () => {
        fetch('/api/camera/start', { method: 'POST' })
            .then(() => {
                cameraFeed.src = `/video_feed?t=${Date.now()}`;
                refreshCameraStatus();
            });
    });

    btnCamStop.addEventListener('click', () => {
        fetch('/api/camera/stop', { method: 'POST' })
            .then(() => {
                cameraFeed.src = '';
                refreshCameraStatus();
            });
    });

    btnCamReconnect.addEventListener('click', () => {
        fetch('/api/camera/reconnect', { method: 'POST' })
            .then(() => {
                cameraFeed.src = `/video_feed?t=${Date.now()}`;
                refreshCameraStatus();
            });
    });

    const btnCamFlip = document.getElementById('btn-cam-flip');
    let camIsFlipped = false;
    if (btnCamFlip) {
        btnCamFlip.addEventListener('click', () => {
            camIsFlipped = !camIsFlipped;
            cameraFeed.style.transform = camIsFlipped ? 'rotate(180deg)' : 'none';
        });
    }

    // Initial setup
    refreshCameraStatus();
    connectWebSocket();
    setInterval(refreshCameraStatus, 3000);
});
