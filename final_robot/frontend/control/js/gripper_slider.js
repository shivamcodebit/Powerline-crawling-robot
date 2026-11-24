/**
 * IIT Mandi Power Line Crawler Robot - Independent Gripper Speed Bar
 * Horizontal spring-back controller (-100% to +100%).
 * Relative drag: clicking/tapping never causes speed jumps.
 * Releasing immediately snaps back to 0% and shuts off the motor.
 */

class GripperSpeedBar {
    constructor(canvasId, valueDisplayId, onSpeedChange) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.valEl = document.getElementById(valueDisplayId);
        this.onSpeedChange = onSpeedChange;

        this.width = this.canvas.width;
        this.height = this.canvas.height;
        this.centerX = this.width / 2;
        this.travelX = (this.width / 2) - 28;

        this.handleX = this.centerX;
        this.isDragging = false;
        this.dragStartX = 0;
        this.startHandleX = this.centerX;
        this.currentSpeed = 0;
        this.animFrameId = null;

        this._setupEvents();
        this._draw();
    }

    _setupEvents() {
        const onPointerDown = (e) => {
            e.preventDefault();
            if (this.animFrameId) {
                cancelAnimationFrame(this.animFrameId);
                this.animFrameId = null;
            }
            this.isDragging = true;
            this.dragStartX = e.clientX;
            this.startHandleX = this.handleX;

            if (this.canvas.setPointerCapture) {
                try { this.canvas.setPointerCapture(e.pointerId); } catch (_) {}
            }
            this._draw();
        };

        const onPointerMove = (e) => {
            if (!this.isDragging) return;
            e.preventDefault();

            const rect = this.canvas.getBoundingClientRect();
            if (!rect.width) return;
            const scaleX = this.canvas.width / rect.width;
            const deltaX = (e.clientX - this.dragStartX) * scaleX;

            let x = this.startHandleX + deltaX;
            const minX = this.centerX - this.travelX;
            const maxX = this.centerX + this.travelX;
            x = Math.max(minX, Math.min(maxX, x));

            this.handleX = x;

            // Left is Reverse (-100%), Right is Forward (+100%)
            const normalized = (x - this.centerX) / this.travelX;
            const speed = Math.round(normalized * 100);

            if (speed !== this.currentSpeed) {
                this.currentSpeed = speed;
                this._updateDisplay();
                if (this.onSpeedChange) {
                    this.onSpeedChange(this.currentSpeed);
                }
            }
            this._draw();
        };

        const onPointerUp = (e) => {
            if (!this.isDragging) return;
            e.preventDefault();
            this.isDragging = false;

            if (this.canvas.releasePointerCapture) {
                try { this.canvas.releasePointerCapture(e.pointerId); } catch (_) {}
            }
            this.resetToZero();
        };

        this.canvas.addEventListener('pointerdown', onPointerDown);
        window.addEventListener('pointermove', onPointerMove, { passive: false });
        window.addEventListener('pointerup', onPointerUp, { passive: false });
        window.addEventListener('pointercancel', onPointerUp, { passive: false });
    }

    resetToZero() {
        if (this.animFrameId) {
            cancelAnimationFrame(this.animFrameId);
        }

        const step = () => {
            if (this.isDragging) return;
            const dx = this.centerX - this.handleX;
            if (Math.abs(dx) < 1.0) {
                this.handleX = this.centerX;
                this.currentSpeed = 0;
                this._updateDisplay();
                if (this.onSpeedChange) {
                    this.onSpeedChange(0);
                }
                this._draw();
                this.animFrameId = null;
            } else {
                this.handleX += dx * 0.4;
                const normalized = (this.handleX - this.centerX) / this.travelX;
                this.currentSpeed = Math.round(normalized * 100);
                this._updateDisplay();
                if (this.onSpeedChange) {
                    this.onSpeedChange(this.currentSpeed);
                }
                this._draw();
                this.animFrameId = requestAnimationFrame(step);
            }
        };
        this.animFrameId = requestAnimationFrame(step);
    }

    _updateDisplay() {
        if (this.valEl) {
            const prefix = this.currentSpeed > 0 ? '+' : '';
            this.valEl.textContent = `${prefix}${this.currentSpeed}%`;
            if (this.currentSpeed > 0) {
                this.valEl.style.color = '#0ea5e9';
            } else if (this.currentSpeed < 0) {
                this.valEl.style.color = '#f59e0b';
            } else {
                this.valEl.style.color = '#94a3b8';
            }
        }
    }

    _draw() {
        const ctx = this.ctx;
        const w = this.width;
        const h = this.height;
        const cy = h / 2;

        ctx.clearRect(0, 0, w, h);

        // 1. Horizontal track slot
        ctx.strokeStyle = '#1e293b';
        ctx.lineWidth = 14;
        ctx.lineCap = 'round';
        ctx.beginPath();
        ctx.moveTo(this.centerX - this.travelX, cy);
        ctx.lineTo(this.centerX + this.travelX, cy);
        ctx.stroke();

        // 2. Center neutral marker
        ctx.strokeStyle = '#475569';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(this.centerX, cy - 14);
        ctx.lineTo(this.centerX, cy + 14);
        ctx.stroke();

        // 3. Proportional active colored bar
        if (this.handleX !== this.centerX) {
            const isFwd = this.handleX > this.centerX;
            ctx.strokeStyle = isFwd ? '#0ea5e9' : '#f59e0b';
            ctx.lineWidth = 8;
            ctx.lineCap = 'round';
            ctx.beginPath();
            ctx.moveTo(this.centerX, cy);
            ctx.lineTo(this.handleX, cy);
            ctx.stroke();
        }

        // 4. Direction guidance text
        ctx.fillStyle = '#64748b';
        ctx.font = '600 10px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';
        ctx.textAlign = 'left';
        ctx.fillText('◀ REV', this.centerX - this.travelX + 4, cy - 12);
        ctx.textAlign = 'right';
        ctx.fillText('FWD ▶', this.centerX + this.travelX - 4, cy - 12);

        // 5. Handle knob
        const isFwd = this.handleX > this.centerX;
        const isRev = this.handleX < this.centerX;
        const knobColor = isFwd ? '#0ea5e9' : (isRev ? '#f59e0b' : '#38bdf8');

        // Outer pill/circle
        ctx.fillStyle = '#0f172a';
        ctx.beginPath();
        ctx.arc(this.handleX, cy, 18, 0, Math.PI * 2);
        ctx.fill();

        ctx.lineWidth = 3;
        ctx.strokeStyle = this.isDragging ? '#ffffff' : knobColor;
        ctx.stroke();

        // Inner pip
        ctx.fillStyle = this.isDragging ? '#ffffff' : knobColor;
        ctx.beginPath();
        ctx.arc(this.handleX, cy, 7, 0, Math.PI * 2);
        ctx.fill();
    }
}
