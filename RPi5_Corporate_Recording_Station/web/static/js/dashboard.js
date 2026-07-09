/**
 * RPi5 Corporate Recording Station - Dashboard Script
 * 
 * Advanced dashboard functionality including:
 * - Real-time data visualization
 * - Chart rendering
 * - System monitoring
 * - Recording controls
 */

// Dashboard state
const DashboardState = {
    charts: {},
    metrics: {},
    updateInterval: null,
    isLive: true
};

// ============================================================
// CHART RENDERER (Simplified - uses canvas)
// ============================================================

class ChartRenderer {
    constructor(canvasId, options = {}) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        
        this.ctx = this.canvas.getContext('2d');
        this.options = options;
        this.data = [];
        this.labels = [];
        this.maxDataPoints = options.maxDataPoints || 60;
        this.animationFrame = null;
        
        // Setup canvas size
        this.resize();
        window.addEventListener('resize', () => this.resize());
    }
    
    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        this.canvas.width = rect.width || 400;
        this.canvas.height = this.options.height || 200;
        this.render();
    }
    
    addDataPoint(value, label = null) {
        this.data.push(value);
        if (label) this.labels.push(label);
        else this.labels.push(new Date().toLocaleTimeString());
        
        if (this.data.length > this.maxDataPoints) {
            this.data.shift();
            this.labels.shift();
        }
        
        this.render();
    }
    
    render() {
        const ctx = this.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;
        
        // Clear
        ctx.clearRect(0, 0, w, h);
        
        if (this.data.length === 0) {
            ctx.fillStyle = '#556677';
            ctx.font = '14px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('No data', w/2, h/2);
            return;
        }
        
        const padding = { top: 10, bottom: 20, left: 10, right: 10 };
        const chartW = w - padding.left - padding.right;
        const chartH = h - padding.top - padding.bottom;
        
        const maxVal = Math.max(1, ...this.data) * 1.2;
        const minVal = Math.min(0, ...this.data);
        const range = maxVal - minVal || 1;
        
        // Draw grid lines
        ctx.strokeStyle = 'rgba(255,255,255,0.05)';
        ctx.lineWidth = 0.5;
        for (let i = 0; i <= 5; i++) {
            const y = padding.top + (i / 5) * chartH;
            ctx.beginPath();
            ctx.moveTo(padding.left, y);
            ctx.lineTo(w - padding.right, y);
            ctx.stroke();
        }
        
        // Draw data line
        const gradient = ctx.createLinearGradient(0, padding.top, 0, h - padding.bottom);
        gradient.addColorStop(0, this.options.color || '#64b5f6');
        gradient.addColorStop(1, (this.options.color || '#64b5f6') + '33');
        
        ctx.beginPath();
        for (let i = 0; i < this.data.length; i++) {
            const x = padding.left + (i / (this.data.length - 1 || 1)) * chartW;
            const y = padding.top + chartH - ((this.data[i] - minVal) / range) * chartH;
            
            if (i === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        }
        ctx.strokeStyle = this.options.color || '#64b5f6';
        ctx.lineWidth = 2;
        ctx.stroke();
        
        // Fill area
        const lastX = padding.left + chartW;
        const lastY = padding.top + chartH - ((this.data[this.data.length-1] - minVal) / range) * chartH;
        ctx.lineTo(lastX, padding.top + chartH);
        ctx.lineTo(padding.left, padding.top + chartH);
        ctx.closePath();
        ctx.fillStyle = gradient;
        ctx.fill();
        
        // Draw current value
        if (this.data.length > 0) {
            const currentVal = this.data[this.data.length - 1];
            const valStr = this.options.format ? this.options.format(currentVal) : currentVal.toFixed(1);
            
            ctx.fillStyle = '#e0e7ee';
            ctx.font = '12px monospace';
            ctx.textAlign = 'right';
            ctx.textBaseline = 'bottom';
            ctx.fillText(valStr, w - padding.right, padding.top - 2);
        }
    }
    
    clear() {
        this.data = [];
        this.labels = [];
        this.render();
    }
}

// ============================================================
// DASHBOARD INITIALIZATION
// ============================================================

function initDashboard() {
    // Start live updates
    startLiveUpdates();
    
    // Setup recording controls
    setupRecordingControls();
    
    // Initialize charts
    initCharts();
}

function startLiveUpdates() {
    if (DashboardState.updateInterval) {
        clearInterval(DashboardState.updateInterval);
    }
    
    DashboardState.updateInterval = setInterval(() => {
        if (DashboardState.isLive) {
            updateMetrics();
        }
    }, 2000);
}

function stopLiveUpdates() {
    if (DashboardState.updateInterval) {
        clearInterval(DashboardState.updateInterval);
        DashboardState.updateInterval = null;
    }
}

function setupRecordingControls() {
    // Additional recording control setup
    // (Main controls are in script.js)
}

function initCharts() {
    // CPU Chart
    const cpuChart = new ChartRenderer('cpuChart', {
        color: '#66bb6a',
        height: 120,
        maxDataPoints: 60,
        format: (v) => v.toFixed(1) + '%'
    });
    DashboardState.charts.cpu = cpuChart;
    
    // Memory Chart
    const memChart = new ChartRenderer('memChart', {
        color: '#42a5f5',
        height: 120,
        maxDataPoints: 60,
        format: (v) => v.toFixed(1) + '%'
    });
    DashboardState.charts.memory = memChart;
    
    // Network Chart
    const netChart = new ChartRenderer('netChart', {
        color: '#ab47bc',
        height: 120,
        maxDataPoints: 60,
        format: (v) => v.toFixed(1) + ' Mbps'
    });
    DashboardState.charts.network = netChart;
}

function updateMetrics() {
    // Fetch latest metrics and update charts
    const api = new ApiClient();
    api.getStatus().then(status => {
        // Update CPU chart
        if (DashboardState.charts.cpu) {
            DashboardState.charts.cpu.addDataPoint(status.cpu_usage);
        }
        
        // Update Memory chart
        if (DashboardState.charts.memory) {
            const memPercent = getPercentage(status.memory.used, status.memory.total);
            DashboardState.charts.memory.addDataPoint(memPercent);
        }
        
        // Update network chart (from network status)
        api.getNetworkStatus().then(net => {
            if (DashboardState.charts.network) {
                const rate = net.receive_rate_mbps || 0;
                DashboardState.charts.network.addDataPoint(rate);
            }
        }).catch(() => {});
        
    }).catch(() => {});
}

// ============================================================
// DASHBOARD EXPORT
// ============================================================

function exportDashboard() {
    const api = new ApiClient();
    api.getStatus().then(status => {
        const data = {
            timestamp: new Date().toISOString(),
            system: status,
            recordings: null,
            storage: null
        };
        
        // Create download
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `dashboard_export_${Date.now()}.json`;
        a.click();
        URL.revokeObjectURL(url);
    }).catch(err => {
        showNotification('Export failed: ' + err.message, 'Error', 'error');
    });
}

// ============================================================
// DASHBOARD THEMES
// ============================================================

function setDashboardTheme(theme) {
    const root = document.documentElement;
    
    switch (theme) {
        case 'dark':
            root.style.setProperty('--bg-primary', '#0a0e17');
            root.style.setProperty('--bg-secondary', '#141d2b');
            root.style.setProperty('--bg-card', '#1a2537');
            root.style.setProperty('--text-primary', '#e0e7ee');
            root.style.setProperty('--text-secondary', '#8899aa');
            break;
        case 'light':
            root.style.setProperty('--bg-primary', '#f0f2f5');
            root.style.setProperty('--bg-secondary', '#e8eaed');
            root.style.setProperty('--bg-card', '#ffffff');
            root.style.setProperty('--text-primary', '#1a1a2e');
            root.style.setProperty('--text-secondary', '#4a4a6a');
            break;
        case 'oled':
            root.style.setProperty('--bg-primary', '#000000');
            root.style.setProperty('--bg-secondary', '#0a0a0a');
            root.style.setProperty('--bg-card', '#111111');
            root.style.setProperty('--text-primary', '#ffffff');
            root.style.setProperty('--text-secondary', '#888888');
            break;
        default:
            // Dark is default
            break;
    }
    
    localStorage.setItem('dashboardTheme', theme);
}

function getDashboardTheme() {
    return localStorage.getItem('dashboardTheme') || 'dark';
}

// ============================================================
// FULLSCREEN MODE
// ============================================================

function toggleFullscreen() {
    if (!document.fullscreenElement) {
        document.documentElement.requestFullscreen();
    } else {
        document.exitFullscreen();
    }
}

// ============================================================
// KEYBOARD SHORTCUTS (Dashboard specific)
// ============================================================

document.addEventListener('keydown', (e) => {
    // Ctrl + E: Export dashboard
    if (e.ctrlKey && e.key === 'e') {
        e.preventDefault();
        exportDashboard();
    }
    
    // Ctrl + F: Toggle fullscreen
    if (e.ctrlKey && e.key === 'f') {
        e.preventDefault();
        toggleFullscreen();
    }
    
    // Ctrl + L: Toggle live updates
    if (e.ctrlKey && e.key === 'l') {
        e.preventDefault();
        DashboardState.isLive = !DashboardState.isLive;
        showNotification(
            DashboardState.isLive ? 'Live updates enabled' : 'Live updates paused',
            'Dashboard',
            DashboardState.isLive ? 'success' : 'warning',
            2000
        );
    }
});

// ============================================================
// INACTIVITY DETECTION
// ============================================================

let inactivityTimer = null;
const INACTIVITY_TIMEOUT = 300000; // 5 minutes

function resetInactivityTimer() {
    if (inactivityTimer) {
        clearTimeout(inactivityTimer);
    }
    inactivityTimer = setTimeout(() => {
        // Pause live updates on inactivity
        if (DashboardState.isLive) {
            DashboardState.isLive = false;
            showNotification('Live updates paused due to inactivity', 'Dashboard', 'warning', 3000);
        }
    }, INACTIVITY_TIMEOUT);
}

document.addEventListener('mousemove', resetInactivityTimer);
document.addEventListener('keydown', resetInactivityTimer);
document.addEventListener('click', resetInactivityTimer);

// ============================================================
// INITIALIZATION
// ============================================================

// Auto-initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initDashboard);
} else {
    initDashboard();
}
