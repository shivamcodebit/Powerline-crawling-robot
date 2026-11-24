/**
 * IIT Mandi Power Line Crawler Robot - Servo Subsystem Controller
 * Safe angle limits, real-time degree feedback, and enable/disable states.
 */

class ServoController {
    constructor(servoId, sliderId, readoutId, enableBtnId, onAngleChange, onEnableChange) {
        this.servoId = servoId;
        this.slider = document.getElementById(sliderId);
        this.readout = document.getElementById(readoutId);
        this.enableBtn = document.getElementById(enableBtnId);
        this.onAngleChange = onAngleChange;
        this.onEnableChange = onEnableChange;

        this.minAngle = 10;
        this.maxAngle = 170;
        this.currentAngle = 90;
        this.isEnabled = true;

        this._setupEvents();
        this._updateUI();
    }

    _setupEvents() {
        if (this.slider) {
            this.slider.addEventListener('input', (e) => {
                let val = parseInt(e.target.value, 10);
                val = Math.max(this.minAngle, Math.min(this.maxAngle, val));
                this.currentAngle = val;
                this._updateUI();
                if (this.onAngleChange) {
                    this.onAngleChange(this.servoId, this.currentAngle);
                }
            });
        }

        if (this.enableBtn) {
            this.enableBtn.addEventListener('click', () => {
                this.isEnabled = !this.isEnabled;
                this._updateUI();
                if (this.onEnableChange) {
                    this.onEnableChange(this.servoId, this.isEnabled);
                }
            });
        }
    }

    setAngle(angle) {
        this.currentAngle = Math.max(this.minAngle, Math.min(this.maxAngle, angle));
        if (this.slider) this.slider.value = this.currentAngle;
        this._updateUI();
    }

    setEnabled(enabled) {
        this.isEnabled = enabled;
        this._updateUI();
    }

    _updateUI() {
        if (this.readout) {
            this.readout.textContent = `${this.currentAngle}°`;
        }
        if (this.enableBtn) {
            if (this.isEnabled) {
                this.enableBtn.textContent = 'ENABLED';
                this.enableBtn.style.color = 'var(--accent-green)';
                this.enableBtn.style.borderColor = 'var(--accent-green)';
            } else {
                this.enableBtn.textContent = 'DISABLED';
                this.enableBtn.style.color = 'var(--text-muted)';
                this.enableBtn.style.borderColor = 'var(--border-color)';
            }
        }
    }
}
