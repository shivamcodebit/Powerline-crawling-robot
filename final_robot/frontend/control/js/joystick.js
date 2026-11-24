/**
 * IIT Mandi Power Line Crawler Robot - Vertical Crawler Joystick
 * Pure vertical forward/reverse proportional control.
 * Smooth touch & mouse drag action with automatic return-to-zero spring.
 * Immune to click-jumping: tracks relative drag displacement so tapping never drives the robot.
 */

class VerticalJoystick {
    constructor(canvasId, onSpeedChange) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.onSpeedChange = onSpeedChange;

        this.width = this.canvas.width;
        this.height = this.canvas.height;
        this.centerY = this.height / 2;
        this.travelY = (this.height / 2) - 32;

        this.handleY = this.centerY;
        this.isDragging = false;
        this.dragStartY = 0;
        this.startHandleY = this.centerY;
        this.currentSpeed = 0; // -100 to +100
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
            this.dragStartY = e.clientY;
            this.startHandleY = this.handleY;

            if (this.canvas.setPointerCapture) {
                try { this.canvas.setPointerCapture(e.pointerId); } catch (_) {}
            }
            this._draw();
        };

        const onPointerMove = (e) => {
            if (!this.isDragging) return;
            e.preventDefault();

            const rect = this.canvas.getBoundingClientRect();
            if (!rect.height) return;
            const scaleY = this.canvas.height / rect.height;
            const deltaY = (e.clientY - this.dragStartY) * scaleY;

            let y = this.startHandleY + deltaY;
            const minY = this.centerY - this.travelY;
            const maxY = this.centerY + this.travelY;
            y = Math.max(minY, Math.min(maxY, y));

            this.handleY = y;

            // Push UP (y < centerY) is Forward (+100)
            // Push DOWN (y > centerY) is Reverse (-100)
            const normalized = (this.centerY - y) / this.travelY;
            const speed = Math.round(normalized * 100);

            if (speed !== this.currentSpeed) {
                this.currentSpeed = speed;
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
            this._springToZero();
        };

        this.canvas.addEventListener('pointerdown', onPointerDown);
        window.addEventListener('pointermove', onPointerMove, { passive: false });
        window.addEventListener('pointerup', onPointerUp, { passive: false });
        window.addEventListener('pointercancel', onPointerUp, { passive: false });
    }

    _springToZero() {
        if (this.animFrameId) {
            cancelAnimationFrame(this.animFrameId);
        }

        const step = () => {
            if (this.isDragging) return;
            const dy = this.centerY - this.handleY;
            if (Math.abs(dy) < 1.0) {
                this.handleY = this.centerY;
                this.currentSpeed = 0;
                if (this.onSpeedChange) {
                    this.onSpeedChange(0);
                }
                this._draw();
                this.animFrameId = null;
            } else {
                this.handleY += dy * 0.35;
                const normalized = (this.centerY - this.handleY) / this.travelY;
                this.currentSpeed = Math.round(normalized * 100);
                if (this.onSpeedChange) {
                    this.onSpeedChange(this.currentSpeed);
                }
                this._draw();
                this.animFrameId = requestAnimationFrame(step);
            }
        };
        this.animFrameId = requestAnimationFrame(step);
    }

    _draw() {
        const ctx = this.ctx;
        const w = this.width;
        const h = this.height;
        const cx = w / 2;

        ctx.clearRect(0, 0, w, h);

        // 1. Guide track slot
        ctx.strokeStyle = '#1e293b';
        ctx.lineWidth = 14;
        ctx.lineCap = 'round';
        ctx.beginPath();
        ctx.moveTo(cx, this.centerY - this.travelY);
        ctx.lineTo(cx, this.centerY + this.travelY);
        ctx.stroke();

        // 2. Center neutral marker
        ctx.strokeStyle = '#475569';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(cx - 24, this.centerY);
        ctx.lineTo(cx + 24, this.centerY);
        ctx.stroke();

        // 3. Proportional active colored track
        if (this.handleY !== this.centerY) {
            const isFwd = this.handleY < this.centerY;
            ctx.strokeStyle = isFwd ? '#0ea5e9' : '#f59e0b';
            ctx.lineWidth = 8;
            ctx.lineCap = 'round';
            ctx.beginPath();
            ctx.moveTo(cx, this.centerY);
            ctx.lineTo(cx, this.handleY);
            ctx.stroke();
        }

        // 4. Direction arrow labels
        ctx.fillStyle = '#64748b';
        ctx.font = '600 11px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('▲ FWD', cx, this.centerY - this.travelY - 10);
        ctx.fillText('▼ REV', cx, this.centerY + this.travelY + 20);

        // 5. Handle knob
        const isFwd = this.handleY < this.centerY;
        const isRev = this.handleY > this.centerY;
        const knobColor = isFwd ? '#0ea5e9' : (isRev ? '#f59e0b' : '#38bdf8');

        // Outer ring
        ctx.fillStyle = '#0f172a';
        ctx.beginPath();
        ctx.arc(cx, this.handleY, 24, 0, Math.PI * 2);
        ctx.fill();

        ctx.lineWidth = 3;
        ctx.strokeStyle = this.isDragging ? '#ffffff' : knobColor;
        ctx.stroke();

        // Inner solid pip
        ctx.fillStyle = this.isDragging ? '#ffffff' : knobColor;
        ctx.beginPath();
        ctx.arc(cx, this.handleY, 10, 0, Math.PI * 2);
        ctx.fill();
    }
}
