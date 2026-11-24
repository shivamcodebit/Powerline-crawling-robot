/**
 * IIT Mandi Power Line Inspection Crawler Robot - Control App Coordinator.
 * Manages WebSocket comms, telemetry decoding, arming state machine, and safety interlocks.
 */

document.addEventListener('DOMContentLoaded', () => {
    // -------------------------------------------------------------------------
    // UI Elements
    // -------------------------------------------------------------------------
    const linkBadge = document.getElementById('badge-link');
    const linkDot = document.getElementById('dot-link');
    const stateBadge = document.getElementById('badge-state');
    const wdogBadge = document.getElementById('badge-wdog');

    const crawlerSpeedEl = document.getElementById('crawler-speed-val');
    const crawlerDirEl = document.getElementById('crawler-dir-val');

    const invertCrawlerABtn = document.getElementById('btn-invert-ca');
    const invertCrawlerBBtn = document.getElementById('btn-invert-cb');

    const btnArm = document.getElementById('btn-arm');
    const btnDisarm = document.getElementById('btn-disarm');
    const btnEstop = document.getElementById('btn-estop');
    const btnResetEstop = document.getElementById('btn-reset-estop');

    const btnStopGripA = document.getElementById('btn-stop-grip-a');
    const btnStopGripB = document.getElementById('btn-stop-grip-b');

    const btnPresetS1Center = document.getElementById('btn-s1-center');
    const btnPresetS2Center = document.getElementById('btn-s2-center');

    const txtFooterTime = document.getElementById('footer-time');
    const txtFooterPackets = document.getElementById('footer-packets');

    // -------------------------------------------------------------------------
    // Local State
    // -------------------------------------------------------------------------
    let ws = null;
    let isConnected = false;
    let isArmed = false;
    let isEstop = false;
    let lastCrawlerCmdTime = 0;
    let lastGripACmdTime = 0;
    let lastGripBCmdTime = 0;

    let invertCrawlerA = false;
    let invertCrawlerB = true; // Opposite end mounting default
    let invertGripperA = false;
    let invertGripperB = false;

    // -------------------------------------------------------------------------
    // WebSocket Connection
    // -------------------------------------------------------------------------
    function connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws/control`;

        linkBadge.textContent = 'CONNECTING...';
        linkDot.className = 'dot dot-orange';

        ws = new WebSocket(wsUrl);

        ws.onopen = () => {
            isConnected = true;
            linkBadge.textContent = 'LINK ONLINE';
            linkDot.className = 'dot dot-green';
            sendHeartbeat();
        };

        ws.onmessage = (event) => {
            try {
                const msg = JSON.parse(event.data);
                handleIncomingTelemetry(msg);
            } catch (e) {
                console.error('WS Parse Error:', e);
            }
        };

        ws.onclose = () => {
            isConnected = false;
            linkBadge.textContent = 'LINK OFFLINE';
            linkDot.className = 'dot dot-red';
            stateBadge.textContent = 'DISCONNECTED';
            setTimeout(connectWebSocket, 1500);
        };

        ws.onerror = () => {
            ws.close();
        };
    }

    function sendCommand(obj) {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify(obj));
        }
    }

    // -------------------------------------------------------------------------
    // Telemetry Handler
    // -------------------------------------------------------------------------
    function handleIncomingTelemetry(t) {
        if (!t) return;

        // Health & Link Status
        if (t.health) {
            if (t.health.stm32_connected) {
                linkBadge.textContent = t.health.is_stale ? 'STM32 STALE' : 'STM32 ONLINE';
                linkDot.className = t.health.is_stale ? 'dot dot-orange' : 'dot dot-green';
            } else {
                linkBadge.textContent = 'STM32 DISCONNECTED';
                linkDot.className = 'dot dot-red';
            }
            if (txtFooterPackets) {
                txtFooterPackets.textContent = `PKTS: ${t.health.total_packets || 0} (${(t.health.packet_rate_hz || 0).toFixed(1)} Hz)`;
            }
        }

        // State & Arming
        isArmed = Boolean(t.armed);
        isEstop = Boolean(t.estop);
        const stateStr = t.state || 'DISARMED';
        stateBadge.textContent = stateStr;

        stateBadge.className = 'state-val';
        if (stateStr === 'ARMED') {
            stateBadge.classList.add('state-armed');
        } else if (stateStr === 'ESTOP_LATCHED') {
            stateBadge.classList.add('state-estop');
        } else {
            stateBadge.classList.add('state-disarmed');
        }

        // Watchdog state
        if (t.watchdog_tripped) {
            wdogBadge.textContent = 'WDOG: TRIPPED';
            wdogBadge.style.color = '#ef4444';
        } else {
            wdogBadge.textContent = 'WDOG: OK';
            wdogBadge.style.color = '#10b981';
        }

        if (txtFooterTime) {
            txtFooterTime.textContent = `MCU: ${t.timestamp || 0} ms | Seq: ${t.seq || 0}`;
        }
    }

    // -------------------------------------------------------------------------
    // Subsystem Controllers Initialization
    // -------------------------------------------------------------------------

    // 1. Crawler Joystick
    const crawlerJoystick = new VerticalJoystick('crawler-joystick', (speed) => {
        const now = Date.now();
        // Update local text metric
        crawlerSpeedEl.textContent = `${speed > 0 ? '+' : ''}${speed}%`;
        if (speed > 0) {
            crawlerDirEl.textContent = '▲ FORWARD';
            crawlerDirEl.style.color = 'var(--accent-cyan)';
        } else if (speed < 0) {
            crawlerDirEl.textContent = '▼ REVERSE';
            crawlerDirEl.style.color = 'var(--accent-orange)';
        } else {
            crawlerDirEl.textContent = '◼ STOPPED';
            crawlerDirEl.style.color = 'var(--text-muted)';
        }

        // Throttle commands to 20Hz or immediate 0
        if (speed === 0 || (now - lastCrawlerCmdTime) > 45) {
            lastCrawlerCmdTime = now;
            sendCommand({ type: 'crawler', speed: speed });
        }
    });

    // 2. Gripper Motor A Bar
    const gripABar = new GripperSpeedBar('gripper-a-canvas', 'grip-a-val', (speed) => {
        const now = Date.now();
        if (speed === 0 || (now - lastGripACmdTime) > 45) {
            lastGripACmdTime = now;
            sendCommand({ type: 'gripper', motor: 'A', speed: speed });
        }
    });

    // 3. Gripper Motor B Bar
    const gripBBar = new GripperSpeedBar('gripper-b-canvas', 'grip-b-val', (speed) => {
        const now = Date.now();
        if (speed === 0 || (now - lastGripBCmdTime) > 45) {
            lastGripBCmdTime = now;
            sendCommand({ type: 'gripper', motor: 'B', speed: speed });
        }
    });

    // Dedicated Gripper Stop Buttons
    btnStopGripA.addEventListener('click', () => {
        gripABar.resetToZero();
        sendCommand({ type: 'gripper', motor: 'A', speed: 0 });
    });

    btnStopGripB.addEventListener('click', () => {
        gripBBar.resetToZero();
        sendCommand({ type: 'gripper', motor: 'B', speed: 0 });
    });

    // 4. Servos
    const servo1 = new ServoController(
        1, 'servo-1-slider', 'servo-1-deg', 'btn-servo-1-toggle',
        (id, angle) => sendCommand({ type: 'servo', id: id, angle: angle }),
        (id, enabled) => sendCommand({ type: 'servo_enable', id: id, enabled: enabled })
    );

    const servo2 = new ServoController(
        2, 'servo-2-slider', 'servo-2-deg', 'btn-servo-2-toggle',
        (id, angle) => sendCommand({ type: 'servo', id: id, angle: angle }),
        (id, enabled) => sendCommand({ type: 'servo_enable', id: id, enabled: enabled })
    );

    btnPresetS1Center.addEventListener('click', () => {
        servo1.setAngle(90);
        sendCommand({ type: 'servo', id: 1, angle: 90 });
    });

    btnPresetS2Center.addEventListener('click', () => {
        servo2.setAngle(90);
        sendCommand({ type: 'servo', id: 2, angle: 90 });
    });

    // 5. Direction Inversion Toggles
    function syncInversionUI() {
        if (invertCrawlerABtn) {
            invertCrawlerABtn.classList.toggle('active', invertCrawlerA);
            invertCrawlerABtn.textContent = `CRAWLER A: ${invertCrawlerA ? 'INVERTED' : 'NORMAL'}`;
        }
        if (invertCrawlerBBtn) {
            invertCrawlerBBtn.classList.toggle('active', invertCrawlerB);
            invertCrawlerBBtn.textContent = `CRAWLER B: ${invertCrawlerB ? 'INVERTED' : 'NORMAL'}`;
        }
    }

    invertCrawlerABtn.addEventListener('click', () => {
        invertCrawlerA = !invertCrawlerA;
        syncInversionUI();
        sendCommand({
            type: 'inversion',
            crawler_a: invertCrawlerA,
            crawler_b: invertCrawlerB,
            gripper_a: invertGripperA,
            gripper_b: invertGripperB
        });
    });

    invertCrawlerBBtn.addEventListener('click', () => {
        invertCrawlerB = !invertCrawlerB;
        syncInversionUI();
        sendCommand({
            type: 'inversion',
            crawler_a: invertCrawlerA,
            crawler_b: invertCrawlerB,
            gripper_a: invertGripperA,
            gripper_b: invertGripperB
        });
    });

    syncInversionUI();

    // 6. Safety & Arming Controls
    btnArm.addEventListener('click', () => {
        if (crawlerJoystick.currentSpeed !== 0 || gripABar.currentSpeed !== 0 || gripBBar.currentSpeed !== 0) {
            alert('SAFETY INTERLOCK:\nCannot ARM robot while motor controls are not at neutral (0%).');
            return;
        }
        sendCommand({ type: 'arm', active: true });
    });

    btnDisarm.addEventListener('click', () => {
        sendCommand({ type: 'arm', active: false });
    });

    btnEstop.addEventListener('click', () => {
        // Immediate UI neutral reset
        crawlerJoystick.handleY = crawlerJoystick.centerY;
        crawlerJoystick.currentSpeed = 0;
        crawlerJoystick._draw();
        crawlerSpeedEl.textContent = '0%';
        crawlerDirEl.textContent = '◼ STOPPED';

        gripABar.resetToZero();
        gripBBar.resetToZero();

        sendCommand({ type: 'estop', active: true });
    });

    btnResetEstop.addEventListener('click', () => {
        sendCommand({ type: 'reset_estop' });
    });

    // 7. Client Heartbeat Loop (keeps backend SafetyManager happy at 10Hz)
    function sendHeartbeat() {
        if (isConnected) {
            sendCommand({ type: 'ping' });
        }
        setTimeout(sendHeartbeat, 100);
    }

    // Start WebSocket
    connectWebSocket();
});
