/**
 * IIT Mandi Power Line Crawler Robot - Self-Contained Canvas Sparkline Charts
 * Lightweight rolling telemetry trend graph renderer.
 */

class TelemetrySparkline {
    constructor(canvasId, maxPoints = 50, strokeColor = '#00d2ff', fillColor = 'rgba(0,210,255,0.1)') {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');
        this.maxPoints = maxPoints;
        this.strokeColor = strokeColor;
        this.fillColor = fillColor;
        this.data = [];
    }

    push(val) {
        if (!this.canvas) return;
        this.data.push(val);
        if (this.data.length > this.maxPoints) {
            this.data.shift();
        }
        this.render();
    }

    render() {
        if (!this.ctx || this.data.length < 2) return;
        const ctx = this.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;

        ctx.clearRect(0, 0, w, h);

        const min = Math.min(...this.data);
        const max = Math.max(...this.data);
        const range = (max - min) === 0 ? 1.0 : (max - min);

        const stepX = w / (this.maxPoints - 1);
        const startIdx = this.maxPoints - this.data.length;

        ctx.beginPath();
        for (let i = 0; i < this.data.length; i++) {
            const x = (startIdx + i) * stepX;
            const normY = (this.data[i] - min) / range;
            const y = h - (normY * (h - 10)) - 5;
            if (i === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        }

        ctx.strokeStyle = this.strokeColor;
        ctx.lineWidth = 2;
        ctx.stroke();

        // Area fill
        ctx.lineTo(w, h);
        ctx.lineTo(startIdx * stepX, h);
        ctx.closePath();
        ctx.fillStyle = this.fillColor;
        ctx.fill();
    }
}
