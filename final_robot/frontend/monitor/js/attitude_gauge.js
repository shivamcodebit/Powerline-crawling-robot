/**
 * IIT Mandi Power Line Crawler Robot - Attitude Indicator (Artificial Horizon)
 * Renders pitch ladder and roll horizon on circular HTML5 canvas.
 */

class AttitudeIndicator {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.width = this.canvas.width;
        this.height = this.canvas.height;
        this.radius = this.width / 2;

        this.pitch = 0.0; // degrees (-90 to +90)
        this.roll = 0.0;  // degrees (-180 to +180)

        this.render();
    }

    update(pitch, roll) {
        this.pitch = pitch;
        this.roll = roll;
        this.render();
    }

    render() {
        const ctx = this.ctx;
        const w = this.width;
        const h = this.height;
        const r = this.radius;

        ctx.save();
        ctx.clearRect(0, 0, w, h);

        // Circular clipping region
        ctx.beginPath();
        ctx.arc(r, r, r - 3, 0, Math.PI * 2);
        ctx.clip();

        // Translate and rotate based on roll
        ctx.translate(r, r);
        ctx.rotate(-this.roll * Math.PI / 180);

        // Pitch displacement: 2 pixels per degree
        const pitchOffset = this.pitch * 2.0;

        // 1. Sky (Blue)
        ctx.fillStyle = '#1e4b7a';
        ctx.fillRect(-w, -h - pitchOffset, w * 2, h + pitchOffset);

        // 2. Ground (Brown / Dark Ground)
        ctx.fillStyle = '#4a2f1b';
        ctx.fillRect(-w, -pitchOffset, w * 2, h + pitchOffset);

        // 3. Horizon line
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(-w, -pitchOffset);
        ctx.lineTo(w, -pitchOffset);
        ctx.stroke();

        // 4. Pitch ladder (+/- 10, 20, 30 deg)
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.7)';
        ctx.fillStyle = '#ffffff';
        ctx.font = '9px monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';

        for (let deg = -40; deg <= 40; deg += 10) {
            if (deg === 0) continue;
            const y = -(deg * 2.0) - pitchOffset;
            const barW = (deg % 20 === 0) ? 36 : 20;

            ctx.lineWidth = (deg % 20 === 0) ? 2 : 1;
            ctx.beginPath();
            ctx.moveTo(-barW / 2, y);
            ctx.lineTo(barW / 2, y);
            ctx.stroke();

            if (deg % 20 === 0) {
                ctx.fillText(`${deg}`, (barW / 2) + 12, y);
                ctx.fillText(`${deg}`, -(barW / 2) - 12, y);
            }
        }

        ctx.restore();

        // Fixed central aircraft / robot reticle symbol
        ctx.strokeStyle = '#00ff9d';
        ctx.lineWidth = 3;
        ctx.beginPath();
        // Left wing
        ctx.moveTo(r - 40, r);
        ctx.lineTo(r - 12, r);
        ctx.lineTo(r - 12, r + 8);
        // Center pip
        ctx.moveTo(r - 3, r);
        ctx.lineTo(r + 3, r);
        // Right wing
        ctx.moveTo(r + 12, r + 8);
        ctx.lineTo(r + 12, r);
        ctx.lineTo(r + 40, r);
        ctx.stroke();

        // Roll index triangle at top
        ctx.fillStyle = '#ff9d00';
        ctx.beginPath();
        ctx.moveTo(r, 6);
        ctx.lineTo(r - 6, 16);
        ctx.lineTo(r + 6, 16);
        ctx.closePath();
        ctx.fill();
    }
}
