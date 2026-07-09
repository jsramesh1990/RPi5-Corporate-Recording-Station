/**
 * RPi5 Corporate Recording Station - Main Script
 * 
 * Provides core functionality for the web interface including:
 * - Navigation and sidebar management
 * - API communication
 * - Notification system
 * - Utility functions
 * - Application state management
 */

// ============================================================
// APPLICATION STATE
// ============================================================

const AppState = {
    isRecording: false,
    isPaused: false,
    isConnected: true,
    recordingFilename: '',
    recordingDuration: 0,
    recordingSize: 0,
    currentPage: 'dashboard',
    apiToken: localStorage.getItem('apiToken') || '',
    refreshInterval: null,
    wsConnection: null,
    wsConnected: false
};

// ============================================================
// DOM REFERENCES
// ============================================================

const DOM = {
    sidebar: document.getElementById('sidebar'),
    sidebarOverlay: document.getElementById('sidebarOverlay'),
    menuToggle: document.getElementById('menuToggle'),
    pageTitle: document.getElementById('pageTitle'),
    contentArea: document.getElementById('contentArea'),
    statusDot: document.getElementById('statusDot'),
    statusText: document.getElementById('statusText'),
    timeDisplay: document.getElementById('timeDisplay'),
    notificationContainer: document.getElementById('notificationContainer')
};

// ============================================================
// API CLIENT
// ============================================================

class ApiClient {
    constructor(baseUrl = '/api/v1') {
        this.baseUrl = baseUrl;
        this.token = AppState.apiToken;
    }

    setToken(token) {
        this.token = token;
        localStorage.setItem('apiToken', token);
        AppState.apiToken = token;
    }

    getHeaders() {
        const headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        };
        if (this.token) {
            headers['Authorization'] = `Bearer ${this.token}`;
        }
        return headers;
    }

    async request(endpoint, method = 'GET', data = null) {
        const url = `${this.baseUrl}${endpoint}`;
        const options = {
            method,
            headers: this.getHeaders()
        };
        if (data) {
            options.body = JSON.stringify(data);
        }

        try {
            const response = await fetch(url, options);
            const result = await response.json();
            
            if (!response.ok) {
                throw new Error(result.error || result.message || `HTTP ${response.status}`);
            }
            return result;
        } catch (error) {
            console.error('API Request failed:', error);
            throw error;
        }
    }

    // System endpoints
    getStatus() {
        return this.request('/system/status');
    }

    getVersion() {
        return this.request('/system/version');
    }

    rebootSystem() {
        return this.request('/system/reboot', 'POST');
    }

    shutdownSystem() {
        return this.request('/system/shutdown', 'POST');
    }

    // Recording endpoints
    getRecordingStatus() {
        return this.request('/recording/status');
    }

    startRecording(config = {}) {
        return this.request('/recording/start', 'POST', config);
    }

    stopRecording() {
        return this.request('/recording/stop', 'POST');
    }

    pauseRecording() {
        return this.request('/recording/pause', 'POST');
    }

    resumeRecording() {
        return this.request('/recording/resume', 'POST');
    }

    listRecordings(limit = 100, offset = 0, sort = 'date', order = 'desc') {
        return this.request(`/recording/list?limit=${limit}&offset=${offset}&sort=${sort}&order=${order}`);
    }

    getRecording(id) {
        return this.request(`/recording/${id}`);
    }

    deleteRecording(id) {
        return this.request(`/recording/${id}`, 'DELETE');
    }

    downloadRecording(id) {
        return `${this.baseUrl}/recording/${id}/download`;
    }

    // Storage endpoints
    getStorageStatus() {
        return this.request('/storage/status');
    }

    listFiles(path = '/', limit = 100, offset = 0, type = '', pattern = '') {
        let url = `/storage/files?limit=${limit}&offset=${offset}`;
        if (path) url += `&path=${encodeURIComponent(path)}`;
        if (type) url += `&type=${encodeURIComponent(type)}`;
        if (pattern) url += `&pattern=${encodeURIComponent(pattern)}`;
        return this.request(url);
    }

    cleanupStorage(ageDays = 30, dryRun = false) {
        return this.request('/storage/cleanup', 'POST', { age_days: ageDays, dry_run: dryRun });
    }

    // Cloud endpoints
    getCloudStatus() {
        return this.request('/cloud/status');
    }

    connectCloud(provider, url, username, password, apiKey = '') {
        return this.request('/cloud/connect', 'POST', { provider, url, username, password, api_key: apiKey });
    }

    disconnectCloud() {
        return this.request('/cloud/disconnect', 'POST');
    }

    uploadToCloud(file, remotePath = '', provider = '', overwrite = false) {
        return this.request('/cloud/upload', 'POST', { file, remote_path: remotePath, provider, overwrite });
    }

    syncCloud() {
        return this.request('/cloud/sync', 'POST');
    }

    // Network endpoints
    getNetworkStatus() {
        return this.request('/network/status');
    }

    scanWiFi() {
        return this.request('/network/wifi/scan');
    }

    connectWiFi(ssid, password, security = 'wpa2', hidden = false) {
        return this.request('/network/wifi/connect', 'POST', { ssid, password, security, hidden });
    }

    disconnectWiFi() {
        return this.request('/network/wifi/disconnect', 'POST');
    }

    // Hardware endpoints
    getHardwareStatus() {
        return this.request('/hardware/status');
    }

    getGPIOStatus() {
        return this.request('/hardware/gpio');
    }

    setGPIOPin(pin, value, direction = 'out') {
        return this.request(`/hardware/gpio/${pin}`, 'POST', { value, direction });
    }

    // Health endpoints
    healthCheck() {
        return this.request('/health');
    }

    livenessCheck() {
        return this.request('/health/live');
    }

    readinessCheck() {
        return this.request('/health/ready');
    }

    // Metrics
    getMetrics() {
        return this.request('/metrics');
    }
}

// ============================================================
// NOTIFICATION SYSTEM
// ============================================================

class NotificationManager {
    constructor(container) {
        this.container = container || DOM.notificationContainer;
        this.id = 0;
    }

    show(message, title = '', type = 'info', duration = 5000) {
        const id = ++this.id;
        const icon = this.getIcon(type);
        const color = this.getColor(type);

        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.id = `toast-${id}`;
        toast.innerHTML = `
            <div class="toast-icon">${icon}</div>
            <div class="toast-content">
                ${title ? `<div class="toast-title" style="color:${color}">${title}</div>` : ''}
                <div class="toast-message">${message}</div>
            </div>
            <button class="toast-close" onclick="NotificationManager.close(${id})">&times;</button>
        `;
        toast.style.animation = 'slideIn 0.3s ease';

        this.container.appendChild(toast);

        if (duration > 0) {
            setTimeout(() => {
                this.close(id);
            }, duration);
        }

        return id;
    }

    success(message, title = 'Success', duration = 4000) {
        return this.show(message, title, 'success', duration);
    }

    error(message, title = 'Error', duration = 6000) {
        return this.show(message, title, 'error', duration);
    }

    warning(message, title = 'Warning', duration = 5000) {
        return this.show(message, title, 'warning', duration);
    }

    info(message, title = 'Info', duration = 4000) {
        return this.show(message, title, 'info', duration);
    }

    close(id) {
        const toast = document.getElementById(`toast-${id}`);
        if (toast) {
            toast.style.opacity = '0';
            toast.style.transform = 'translateX(100px)';
            toast.style.transition = 'all 0.3s ease';
            setTimeout(() => {
                if (toast.parentNode) {
                    toast.parentNode.removeChild(toast);
                }
            }, 300);
        }
    }

    clear() {
        this.container.innerHTML = '';
    }

    getIcon(type) {
        switch (type) {
            case 'success': return '✅';
            case 'error': return '❌';
            case 'warning': return '⚠️';
            case 'info': return 'ℹ️';
            default: return '📋';
        }
    }

    getColor(type) {
        switch (type) {
            case 'success': return '#66bb6a';
            case 'error': return '#ef5350';
            case 'warning': return '#fdd835';
            case 'info': return '#42a5f5';
            default: return '#e0e7ee';
        }
    }
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function formatDuration(seconds) {
    if (seconds < 0) seconds = 0;
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = Math.floor(seconds % 60);
    
    if (h > 0) {
        return `${h}h ${m}m ${s}s`;
    } else if (m > 0) {
        return `${m}m ${s}s`;
    } else {
        return `${s}s`;
    }
}

function formatTimestamp(timestamp) {
    if (!timestamp) return '';
    const date = new Date(timestamp);
    return date.toLocaleString();
}

function formatDate(date) {
    if (!date) return '';
    const d = new Date(date);
    return d.toLocaleDateString() + ' ' + d.toLocaleTimeString();
}

function getPercentage(value, total) {
    if (total === 0) return 0;
    return Math.min(100, (value / total) * 100);
}

function getColorForPercentage(value) {
    if (value < 60) return 'green';
    if (value < 80) return 'yellow';
    return 'red';
}

function getStatusColor(status) {
    switch (status) {
        case 'online': return 'green';
        case 'recording': return 'red';
        case 'warning': return 'yellow';
        case 'offline': return 'gray';
        default: return 'gray';
    }
}

function debounce(func, wait) {
    let timeout;
    return function(...args) {
        clearTimeout(timeout);
        timeout = setTimeout(() => func.apply(this, args), wait);
    };
}

function throttle(func, limit) {
    let inThrottle;
    return function(...args) {
        if (!inThrottle) {
            func.apply(this, args);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    };
}

function getQueryParam(param) {
    const urlParams = new URLSearchParams(window.location.search);
    return urlParams.get(param);
}

function setQueryParam(param, value) {
    const url = new URL(window.location);
    url.searchParams.set(param, value);
    window.history.pushState({}, '', url);
}

function generateId() {
    return Date.now().toString(36) + Math.random().toString(36).substr(2, 5);
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function safeJsonParse(str, fallback = null) {
    try {
        return JSON.parse(str);
    } catch {
        return fallback;
    }
}

// ============================================================
// NAVIGATION
// ============================================================

function navigateTo(page, data = null) {
    const pageTitle = {
        dashboard: 'Dashboard',
        recordings: 'Recordings',
        storage: 'Storage',
        cloud: 'Cloud',
        network: 'Network',
        hardware: 'Hardware',
        settings: 'Settings',
        logs: 'Logs'
    };

    // Update sidebar active state
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
        if (item.dataset.page === page) {
            item.classList.add('active');
        }
    });

    // Update page title
    DOM.pageTitle.textContent = pageTitle[page] || page.charAt(0).toUpperCase() + page.slice(1);

    // Load page content
    loadPage(page, data);

    // Close sidebar on mobile
    if (window.innerWidth <= 768) {
        toggleSidebar(false);
    }

    AppState.currentPage = page;
}

function loadPage(page, data) {
    const contentArea = DOM.contentArea;
    
    switch (page) {
        case 'dashboard':
            loadDashboard(contentArea);
            break;
        case 'recordings':
            loadRecordings(contentArea);
            break;
        case 'storage':
            loadStorage(contentArea);
            break;
        case 'cloud':
            loadCloud(contentArea);
            break;
        case 'network':
            loadNetwork(contentArea);
            break;
        case 'hardware':
            loadHardware(contentArea);
            break;
        case 'settings':
            loadSettings(contentArea);
            break;
        case 'logs':
            loadLogs(contentArea);
            break;
        default:
            contentArea.innerHTML = '<h2>Page not found</h2>';
    }
}

function toggleSidebar(open) {
    const sidebar = DOM.sidebar;
    const overlay = DOM.sidebarOverlay;
    
    if (open === undefined) {
        open = !sidebar.classList.contains('open');
    }
    
    if (open) {
        sidebar.classList.add('open');
        overlay.classList.add('active');
        document.body.style.overflow = 'hidden';
    } else {
        sidebar.classList.remove('open');
        overlay.classList.remove('active');
        document.body.style.overflow = '';
    }
}

// ============================================================
// PAGE LOADERS
// ============================================================

function loadDashboard(container) {
    container.innerHTML = `
        <div class="dashboard">
            <!-- Metrics Row -->
            <div class="metrics-row" id="metricsRow">
                <div class="metric-card">
                    <div class="metric-label">CPU Usage</div>
                    <div class="metric-value" id="cpuValue">--%</div>
                    <div class="progress-bar"><div class="progress-fill green" id="cpuBar" style="width:0%"></div></div>
                </div>
                <div class="metric-card">
                    <div class="metric-label">Memory</div>
                    <div class="metric-value" id="memoryValue">--</div>
                    <div class="progress-bar"><div class="progress-fill blue" id="memoryBar" style="width:0%"></div></div>
                </div>
                <div class="metric-card">
                    <div class="metric-label">Storage</div>
                    <div class="metric-value" id="storageValue">--</div>
                    <div class="progress-bar"><div class="progress-fill green" id="storageBar" style="width:0%"></div></div>
                </div>
                <div class="metric-card">
                    <div class="metric-label">Temperature</div>
                    <div class="metric-value" id="tempValue">--°C</div>
                    <div class="metric-sub" id="tempStatus">Normal</div>
                </div>
                <div class="metric-card">
                    <div class="metric-label">Uptime</div>
                    <div class="metric-value" id="uptimeValue">--</div>
                    <div class="metric-sub" id="uptimeSub">--</div>
                </div>
            </div>

            <!-- Recording Controls -->
            <div class="recording-controls">
                <div class="status-display">
                    <div class="recording-indicator idle" id="recordingIndicator"></div>
                    <div class="recording-info">
                        <div id="recordingStatusText">Idle</div>
                        <div class="filename" id="recordingFilename"></div>
                    </div>
                </div>
                <div class="btn-group">
                    <button class="btn btn-success" id="recordBtn" onclick="startRecording()">▶ Start Recording</button>
                    <button class="btn btn-warning" id="pauseBtn" onclick="pauseRecording()" style="display:none">⏸ Pause</button>
                    <button class="btn btn-danger" id="stopBtn" onclick="stopRecording()" style="display:none">⏹ Stop</button>
                </div>
            </div>

            <!-- Recent Recordings -->
            <div class="card">
                <div class="card-header">
                    <span class="card-title">Recent Recordings</span>
                    <button class="btn btn-sm btn-outline" onclick="navigateTo('recordings')">View All</button>
                </div>
                <div id="recentRecordings">
                    <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
                </div>
            </div>

            <!-- System Status -->
            <div class="card">
                <div class="card-header">
                    <span class="card-title">System Status</span>
                </div>
                <div class="system-status-grid" id="systemStatusGrid">
                    <div class="system-status-item">
                        <span class="status-label">Status</span>
                        <span class="status-value" id="sysStatusValue">Online</span>
                    </div>
                    <div class="system-status-item">
                        <span class="status-label">Version</span>
                        <span class="status-value" id="sysVersionValue">--</span>
                    </div>
                    <div class="system-status-item">
                        <span class="status-label">Processes</span>
                        <span class="status-value" id="sysProcessesValue">--</span>
                    </div>
                    <div class="system-status-item">
                        <span class="status-label">Load Average</span>
                        <span class="status-value" id="sysLoadValue">--</span>
                    </div>
                </div>
            </div>
        </div>
    `;

    // Load initial data
    refreshDashboard();
}

function loadRecordings(container) {
    container.innerHTML = `
        <div class="card">
            <div class="card-header">
                <span class="card-title">All Recordings</span>
                <div style="display:flex;gap:0.5rem;">
                    <button class="btn btn-sm btn-outline" onclick="refreshRecordings()">🔄 Refresh</button>
                </div>
            </div>
            <div id="recordingsList">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
            </div>
        </div>
    `;
    refreshRecordings();
}

function loadStorage(container) {
    container.innerHTML = `
        <div class="card">
            <div class="card-header">
                <span class="card-title">Storage Status</span>
                <button class="btn btn-sm btn-outline" onclick="refreshStorage()">🔄 Refresh</button>
            </div>
            <div id="storageDetails">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
            </div>
        </div>
        <div class="card" style="margin-top:1.25rem;">
            <div class="card-header">
                <span class="card-title">File Browser</span>
            </div>
            <div id="fileBrowser">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
            </div>
        </div>
    `;
    refreshStorage();
}

function loadCloud(container) {
    container.innerHTML = `
        <div class="card">
            <div class="card-header">
                <span class="card-title">Cloud Status</span>
                <button class="btn btn-sm btn-outline" onclick="refreshCloud()">🔄 Refresh</button>
            </div>
            <div id="cloudDetails">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
            </div>
        </div>
        <div class="card" style="margin-top:1.25rem;">
            <div class="card-header">
                <span class="card-title">Upload to Cloud</span>
            </div>
            <div id="cloudUpload">
                <div style="padding:0.5rem 0;">
                    <div class="form-group">
                        <label>File Path</label>
                        <input type="text" class="form-control" id="uploadFilePath" placeholder="/var/lib/recording-station/recordings/file.mp4">
                    </div>
                    <div class="form-group">
                        <label>Remote Path (optional)</label>
                        <input type="text" class="form-control" id="uploadRemotePath" placeholder="/Recordings/">
                    </div>
                    <button class="btn btn-primary" onclick="uploadToCloud()">☁️ Upload</button>
                </div>
            </div>
        </div>
    `;
    refreshCloud();
}

function loadNetwork(container) {
    container.innerHTML = `
        <div class="card">
            <div class="card-header">
                <span class="card-title">Network Status</span>
                <button class="btn btn-sm btn-outline" onclick="refreshNetwork()">🔄 Refresh</button>
            </div>
            <div id="networkDetails">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
            </div>
        </div>
        <div class="card" style="margin-top:1.25rem;">
            <div class="card-header">
                <span class="card-title">WiFi Management</span>
            </div>
            <div id="wifiManagement">
                <button class="btn btn-primary" onclick="scanWiFi()">📡 Scan WiFi</button>
                <div id="wifiNetworks" style="margin-top:1rem;"></div>
            </div>
        </div>
    `;
    refreshNetwork();
}

function loadHardware(container) {
    container.innerHTML = `
        <div class="card">
            <div class="card-header">
                <span class="card-title">Hardware Status</span>
                <button class="btn btn-sm btn-outline" onclick="refreshHardware()">🔄 Refresh</button>
            </div>
            <div id="hardwareDetails">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
            </div>
        </div>
        <div class="card" style="margin-top:1.25rem;">
            <div class="card-header">
                <span class="card-title">GPIO Control</span>
            </div>
            <div id="gpioControl">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading...</div>
            </div>
        </div>
    `;
    refreshHardware();
}

function loadSettings(container) {
    container.innerHTML = `
        <div class="card">
            <div class="card-header">
                <span class="card-title">Settings</span>
            </div>
            <div style="padding:1rem 0;">
                <div class="form-group">
                    <label>API Token</label>
                    <input type="text" class="form-control" id="apiTokenInput" placeholder="Enter API token">
                    <button class="btn btn-primary" onclick="saveApiToken()" style="margin-top:0.5rem;">Save Token</button>
                </div>
                <hr style="border-color:var(--border-color);margin:1rem 0;">
                <div style="display:flex;gap:0.5rem;">
                    <button class="btn btn-warning" onclick="confirmReboot()">🔄 Reboot System</button>
                    <button class="btn btn-danger" onclick="confirmShutdown()">⏻ Shutdown System</button>
                </div>
            </div>
        </div>
    `;
    document.getElementById('apiTokenInput').value = AppState.apiToken;
}

function loadLogs(container) {
    container.innerHTML = `
        <div class="card">
            <div class="card-header">
                <span class="card-title">System Logs</span>
                <div style="display:flex;gap:0.5rem;">
                    <button class="btn btn-sm btn-outline" onclick="refreshLogs()">🔄 Refresh</button>
                    <button class="btn btn-sm btn-outline" onclick="clearLogs()">🗑 Clear</button>
                </div>
            </div>
            <div id="logsContainer">
                <div style="text-align:center;padding:2rem;color:var(--text-muted);">Loading logs...</div>
            </div>
        </div>
    `;
    refreshLogs();
}

// ============================================================
// REFRESH FUNCTIONS
// ============================================================

async function refreshDashboard() {
    try {
        const api = new ApiClient();
        const status = await api.getStatus();
        const recordings = await api.listRecordings(5);

        // Update metrics
        document.getElementById('cpuValue').textContent = status.cpu_usage.toFixed(1) + '%';
        document.getElementById('cpuBar').style.width = status.cpu_usage + '%';
        document.getElementById('cpuBar').className = 'progress-fill ' + getColorForPercentage(status.cpu_usage);

        const memPercent = getPercentage(status.memory.used, status.memory.total);
        document.getElementById('memoryValue').textContent = formatBytes(status.memory.used) + ' / ' + formatBytes(status.memory.total);
        document.getElementById('memoryBar').style.width = memPercent + '%';

        const storagePercent = getPercentage(status.storage.used, status.storage.total);
        document.getElementById('storageValue').textContent = formatBytes(status.storage.used) + ' / ' + formatBytes(status.storage.total);
        document.getElementById('storageBar').style.width = storagePercent + '%';
        document.getElementById('storageBar').className = 'progress-fill ' + getColorForPercentage(storagePercent);

        document.getElementById('tempValue').textContent = status.cpu_temp.toFixed(1) + '°C';
        const tempStatus = status.cpu_temp < 60 ? 'Normal' : status.cpu_temp < 75 ? 'Warm' : 'Hot';
        document.getElementById('tempStatus').textContent = tempStatus;

        const uptime = formatDuration(status.uptime);
        document.getElementById('uptimeValue').textContent = uptime;
        document.getElementById('uptimeSub').textContent = 'Since ' + formatDate(status.timestamp);

        // Update system status
        document.getElementById('sysStatusValue').textContent = status.status.charAt(0).toUpperCase() + status.status.slice(1);
        document.getElementById('sysStatusValue').style.color = status.status === 'online' ? 'var(--success-light)' : 'var(--warning-light)';
        document.getElementById('sysVersionValue').textContent = status.version || '2.0.0';
        document.getElementById('sysProcessesValue').textContent = status.processes || '--';
        document.getElementById('sysLoadValue').textContent = `${status.load_avg.one_min.toFixed(2)}, ${status.load_avg.five_min.toFixed(2)}, ${status.load_avg.fifteen_min.toFixed(2)}`;

        // Update recording status
        const recStatus = await api.getRecordingStatus();
        updateRecordingUI(recStatus);

        // Update recent recordings
        const recList = document.getElementById('recentRecordings');
        if (recordings.recordings && recordings.recordings.length > 0) {
            let html = `<div class="file-list">`;
            recordings.recordings.forEach(rec => {
                html += `
                    <div class="file-item">
                        <div class="file-info">
                            <span class="file-icon">🎬</span>
                            <span class="file-name">${escapeHtml(rec.filename)}</span>
                            <span class="file-meta">${formatBytes(rec.size)}</span>
                            <span class="file-meta">${formatDuration(rec.duration)}</span>
                            <span class="file-meta">${formatDate(rec.created)}</span>
                        </div>
                        <div class="file-actions">
                            <button class="action-btn" onclick="downloadRecording('${rec.id}')" title="Download">⬇️</button>
                            <button class="action-btn delete" onclick="deleteRecording('${rec.id}')" title="Delete">🗑</button>
                        </div>
                    </div>
                `;
            });
            html += `</div>`;
            recList.innerHTML = html;
        } else {
            recList.innerHTML = '<div style="text-align:center;padding:1rem;color:var(--text-muted);">No recordings found</div>';
        }

    } catch (error) {
        console.error('Failed to refresh dashboard:', error);
    }
}

async function refreshRecordings() {
    try {
        const api = new ApiClient();
        const result = await api.listRecordings(100);
        const container = document.getElementById('recordingsList');

        if (result.recordings && result.recordings.length > 0) {
            let html = `
                <div class="file-list">
                <div style="display:grid;grid-template-columns:2fr 1fr 1fr 1fr 1fr;padding:0.75rem 1.25rem;background:var(--bg-secondary);color:var(--text-secondary);font-size:0.75rem;font-weight:600;">
                    <span>Filename</span>
                    <span>Size</span>
                    <span>Duration</span>
                    <span>Date</span>
                    <span>Actions</span>
                </div>
            `;
            result.recordings.forEach(rec => {
                html += `
                    <div class="file-item">
                        <div class="file-info">
                            <span class="file-icon">🎬</span>
                            <span class="file-name">${escapeHtml(rec.filename)}</span>
                            <span class="file-meta">${formatBytes(rec.size)}</span>
                            <span class="file-meta">${formatDuration(rec.duration)}</span>
                            <span class="file-meta">${formatDate(rec.created)}</span>
                        </div>
                        <div class="file-actions">
                            <button class="action-btn" onclick="downloadRecording('${rec.id}')" title="Download">⬇️</button>
                            <button class="action-btn" onclick="copyPath('${rec.path}')" title="Copy path">📋</button>
                            <button class="action-btn delete" onclick="deleteRecording('${rec.id}')" title="Delete">🗑</button>
                        </div>
                    </div>
                `;
            });
            html += `</div>`;
            container.innerHTML = html;
        } else {
            container.innerHTML = '<div style="text-align:center;padding:2rem;color:var(--text-muted);">No recordings found</div>';
        }
    } catch (error) {
        console.error('Failed to refresh recordings:', error);
        document.getElementById('recordingsList').innerHTML = '<div style="text-align:center;padding:2rem;color:var(--danger-light);">Failed to load recordings</div>';
    }
}

async function refreshStorage() {
    try {
        const api = new ApiClient();
        const storage = await api.getStorageStatus();
        const files = await api.listFiles('/', 20);

        const container = document.getElementById('storageDetails');
        const percent = getPercentage(storage.used, storage.total);
        const color = getColorForPercentage(percent);

        container.innerHTML = `
            <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:1rem;margin-bottom:1rem;">
                <div><strong>Total:</strong> ${formatBytes(storage.total)}</div>
                <div><strong>Used:</strong> ${formatBytes(storage.used)}</div>
                <div><strong>Free:</strong> ${formatBytes(storage.free)}</div>
                <div><strong>Files:</strong> ${storage.file_count}</div>
            </div>
            <div style="margin:1rem 0;">
                <div style="display:flex;justify-content:space-between;font-size:0.8rem;color:var(--text-secondary);">
                    <span>Used ${percent.toFixed(1)}%</span>
                    <span>${formatBytes(storage.used)} / ${formatBytes(storage.total)}</span>
                </div>
                <div class="progress-bar" style="height:20px;">
                    <div class="progress-fill ${color}" style="width:${percent}%;height:100%;"></div>
                </div>
            </div>
            <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:0.5rem;margin-top:0.5rem;">
                ${Object.entries(storage.file_types || {}).map(([type, count]) => `
                    <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                        <div style="font-weight:600;font-size:1.1rem;">${count}</div>
                        <div style="font-size:0.7rem;color:var(--text-muted);">${type || 'Other'}</div>
                    </div>
                `).join('')}
            </div>
        `;

        // File browser
        const browser = document.getElementById('fileBrowser');
        if (files.files && files.files.length > 0) {
            let html = `<div class="file-list">`;
            files.files.forEach(file => {
                const icon = file.type === 'directory' ? '📁' : '📄';
                html += `
                    <div class="file-item">
                        <div class="file-info">
                            <span class="file-icon">${icon}</span>
                            <span class="file-name">${escapeHtml(file.name)}</span>
                            <span class="file-meta">${file.type === 'directory' ? 'DIR' : formatBytes(file.size)}</span>
                            <span class="file-meta">${formatDate(file.modified)}</span>
                        </div>
                    </div>
                `;
            });
            html += `</div>`;
            browser.innerHTML = html;
        } else {
            browser.innerHTML = '<div style="text-align:center;padding:1rem;color:var(--text-muted);">No files found</div>';
        }

    } catch (error) {
        console.error('Failed to refresh storage:', error);
    }
}

async function refreshCloud() {
    try {
        const api = new ApiClient();
        const status = await api.getCloudStatus();

        const container = document.getElementById('cloudDetails');
        const connected = status.connected;

        container.innerHTML = `
            <div style="display:flex;align-items:center;gap:1.5rem;flex-wrap:wrap;">
                <div style="font-size:3rem;">${connected ? '☁️' : '🌧'}</div>
                <div style="flex:1;">
                    <div style="font-weight:600;font-size:1.1rem;">${connected ? 'Connected' : 'Disconnected'}</div>
                    <div style="color:var(--text-secondary);font-size:0.85rem;">
                        ${connected ? `Provider: ${status.provider || 'Unknown'}` : 'Not connected to any cloud provider'}
                    </div>
                    ${connected ? `
                        <div style="color:var(--text-muted);font-size:0.8rem;margin-top:0.25rem;">
                            ${status.username} | ${formatBytes(status.used_space)} / ${formatBytes(status.total_space)}
                        </div>
                        ${status.syncing ? `<div style="color:var(--warning-light);font-size:0.8rem;">🔄 Syncing... ${status.last_sync || ''}</div>` : ''}
                    ` : ''}
                </div>
                <div style="text-align:right;">
                    ${connected ? `
                        <button class="btn btn-sm btn-danger" onclick="disconnectCloud()">Disconnect</button>
                    ` : `
                        <button class="btn btn-sm btn-primary" onclick="showConnectCloud()">Connect</button>
                    `}
                </div>
            </div>
            ${connected ? `
                <div style="margin-top:1rem;display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:0.5rem;">
                    <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                        <div style="font-weight:600;">${formatBytes(status.total_space)}</div>
                        <div style="font-size:0.7rem;color:var(--text-muted);">Total</div>
                    </div>
                    <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                        <div style="font-weight:600;">${formatBytes(status.used_space)}</div>
                        <div style="font-size:0.7rem;color:var(--text-muted);">Used</div>
                    </div>
                    <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                        <div style="font-weight:600;">${formatBytes(status.free_space)}</div>
                        <div style="font-size:0.7rem;color:var(--text-muted);">Free</div>
                    </div>
                    <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                        <div style="font-weight:600;">${status.queue_size || 0}</div>
                        <div style="font-size:0.7rem;color:var(--text-muted);">Queue</div>
                    </div>
                </div>
                <div style="margin-top:1rem;">
                    <button class="btn btn-sm btn-primary" onclick="syncCloud()">🔄 Sync Now</button>
                </div>
            ` : ''}
        `;

    } catch (error) {
        console.error('Failed to refresh cloud:', error);
        document.getElementById('cloudDetails').innerHTML = '<div style="text-align:center;padding:2rem;color:var(--danger-light);">Failed to load cloud status</div>';
    }
}

async function refreshNetwork() {
    try {
        const api = new ApiClient();
        const status = await api.getNetworkStatus();

        const container = document.getElementById('networkDetails');
        const connected = status.state === 'up';

        container.innerHTML = `
            <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;">
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">Interface</div>
                    <div style="font-weight:600;">${status.interface}</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">Status</div>
                    <div style="font-weight:600;color:${connected ? 'var(--success-light)' : 'var(--danger-light)'};">${connected ? '🟢 Up' : '🔴 Down'}</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">IP Address</div>
                    <div style="font-weight:600;">${status.ip_address || '--'}</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">MAC Address</div>
                    <div style="font-weight:600;font-size:0.8rem;">${status.mac_address || '--'}</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">Speed</div>
                    <div style="font-weight:600;">${status.speed || '--'} Mbps</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">Gateway</div>
                    <div style="font-weight:600;font-size:0.85rem;">${status.gateway || '--'}</div>
                </div>
            </div>
            <div style="margin-top:1rem;display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:0.5rem;">
                <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                    <div style="font-weight:600;">${formatBytes(status.bytes_sent || 0)}</div>
                    <div style="font-size:0.7rem;color:var(--text-muted);">Sent</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                    <div style="font-weight:600;">${formatBytes(status.bytes_received || 0)}</div>
                    <div style="font-size:0.7rem;color:var(--text-muted);">Received</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                    <div style="font-weight:600;">${(status.send_rate_mbps || 0).toFixed(1)} Mbps</div>
                    <div style="font-size:0.7rem;color:var(--text-muted);">Send Rate</div>
                </div>
                <div style="background:var(--bg-secondary);padding:0.5rem;border-radius:6px;text-align:center;">
                    <div style="font-weight:600;">${(status.receive_rate_mbps || 0).toFixed(1)} Mbps</div>
                    <div style="font-size:0.7rem;color:var(--text-muted);">Receive Rate</div>
                </div>
            </div>
            ${status.wifi_ssid ? `
                <div style="margin-top:1rem;padding:0.75rem;background:var(--bg-secondary);border-radius:8px;">
                    <div style="display:flex;justify-content:space-between;align-items:center;">
                        <div>
                            <span style="font-weight:600;">📶 ${status.wifi_ssid}</span>
                            <span style="color:var(--text-muted);font-size:0.8rem;margin-left:0.5rem;">${status.wifi_signal || 0} dBm</span>
                        </div>
                        <span class="badge badge-info">WiFi</span>
                    </div>
                </div>
            ` : ''}
        `;

    } catch (error) {
        console.error('Failed to refresh network:', error);
        document.getElementById('networkDetails').innerHTML = '<div style="text-align:center;padding:2rem;color:var(--danger-light);">Failed to load network status</div>';
    }
}

async function refreshHardware() {
    try {
        const api = new ApiClient();
        const status = await api.getHardwareStatus();

        const container = document.getElementById('hardwareDetails');

        container.innerHTML = `
            <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;">
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">Camera</div>
                    <div style="font-weight:600;color:${status.camera.detected ? 'var(--success-light)' : 'var(--text-muted)'};">${status.camera.detected ? '✅ Detected' : '❌ Not Detected'}</div>
                    ${status.camera.detected ? `<div style="font-size:0.8rem;color:var(--text-muted);">${status.camera.resolution || '--'} @ ${status.camera.framerate || '--'}fps</div>` : ''}
                </div>
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">Audio</div>
                    <div style="font-weight:600;color:${status.audio.detected ? 'var(--success-light)' : 'var(--text-muted)'};">${status.audio.detected ? '✅ Detected' : '❌ Not Detected'}</div>
                    ${status.audio.detected ? `<div style="font-size:0.8rem;color:var(--text-muted);">${status.audio.sample_rate || '--'}Hz, ${status.audio.channels || '--'}ch</div>` : ''}
                </div>
                <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;">
                    <div style="font-size:0.7rem;color:var(--text-muted);">UART</div>
                    <div style="font-weight:600;color:${status.uart.state === 'open' ? 'var(--success-light)' : 'var(--text-muted)'};">${status.uart.state === 'open' ? '✅ Open' : '❌ Closed'}</div>
                    <div style="font-size:0.8rem;color:var(--text-muted);">${status.uart.device || '--'} @ ${status.uart.baud || '--'} baud</div>
                </div>
            </div>
        `;

        // GPIO Control
        const gpioContainer = document.getElementById('gpioControl');
        const gpioStatus = await api.getGPIOStatus();

        if (gpioStatus.pins && gpioStatus.pins.length > 0) {
            let html = `
                <div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(100px,1fr));gap:0.5rem;">
            `;
            gpioStatus.pins.slice(0, 20).forEach(pin => {
                const state = pin.value ? 'high' : 'low';
                html += `
                    <div class="gpio-pin" onclick="toggleGPIOPin(${pin.pin}, ${!pin.value})" style="cursor:pointer;">
                        <span class="pin-number">${pin.pin}</span>
                        <span class="pin-name">${pin.name || 'GPIO' + pin.pin}</span>
                        <span class="pin-state ${state}"></span>
                    </div>
                `;
            });
            html += `</div>`;
            if (gpioStatus.pins.length > 20) {
                html += `<div style="text-align:center;font-size:0.8rem;color:var(--text-muted);margin-top:0.5rem;">Showing 20 of ${gpioStatus.pins.length} pins</div>`;
            }
            gpioContainer.innerHTML = html;
        } else {
            gpioContainer.innerHTML = '<div style="text-align:center;padding:1rem;color:var(--text-muted);">No GPIO pins available</div>';
        }

    } catch (error) {
        console.error('Failed to refresh hardware:', error);
        document.getElementById('hardwareDetails').innerHTML = '<div style="text-align:center;padding:2rem;color:var(--danger-light);">Failed to load hardware status</div>';
    }
}

function refreshLogs() {
    // Logs would be fetched from API or WebSocket
    // For now, show placeholder
    const container = document.getElementById('logsContainer');
    container.innerHTML = `
        <div class="activity-log">
            ${Array.from({length: 20}, (_, i) => `
                <div class="log-entry">
                    <span class="log-time">${new Date().toLocaleTimeString()}</span>
                    <span class="log-level ${['INFO', 'SUCCESS', 'WARNING', 'ERROR', 'DEBUG'][i % 5]}">${['INFO', 'SUCCESS', 'WARNING', 'ERROR', 'DEBUG'][i % 5]}</span>
                    <span class="log-message">${['System started', 'Recording started: meeting.mp4', 'Storage usage at 60%', 'Network connection established', 'Cloud sync completed'][i % 5]}</span>
                </div>
            `).join('')}
        </div>
    `;
}

function clearLogs() {
    document.getElementById('logsContainer').innerHTML = '<div style="text-align:center;padding:2rem;color:var(--text-muted);">Logs cleared</div>';
}

// ============================================================
// RECORDING CONTROLS
// ============================================================

async function startRecording() {
    try {
        const api = new ApiClient();
        const result = await api.startRecording({
            width: 1920,
            height: 1080,
            framerate: 30,
            quality: 'high'
        });
        
        if (result.success) {
            AppState.isRecording = true;
            updateRecordingUI({ is_recording: true, filename: result.data.filename });
            showNotification('Recording started', 'Success', 'success');
        }
    } catch (error) {
        showNotification('Failed to start recording: ' + error.message, 'Error', 'error');
    }
}

async function stopRecording() {
    try {
        const api = new ApiClient();
        const result = await api.stopRecording();
        
        if (result.success) {
            AppState.isRecording = false;
            AppState.isPaused = false;
            updateRecordingUI({ is_recording: false });
            showNotification('Recording stopped', 'Success', 'success');
            refreshRecordings();
        }
    } catch (error) {
        showNotification('Failed to stop recording: ' + error.message, 'Error', 'error');
    }
}

async function pauseRecording() {
    try {
        const api = new ApiClient();
        await api.pauseRecording();
        AppState.isPaused = true;
        updateRecordingUI({ is_paused: true });
        showNotification('Recording paused', 'Info', 'info');
    } catch (error) {
        showNotification('Failed to pause recording: ' + error.message, 'Error', 'error');
    }
}

async function resumeRecording() {
    try {
        const api = new ApiClient();
        await api.resumeRecording();
        AppState.isPaused = false;
        updateRecordingUI({ is_paused: false });
        showNotification('Recording resumed', 'Info', 'info');
    } catch (error) {
        showNotification('Failed to resume recording: ' + error.message, 'Error', 'error');
    }
}

function updateRecordingUI(status) {
    const indicator = document.getElementById('recordingIndicator');
    const statusText = document.getElementById('recordingStatusText');
    const filename = document.getElementById('recordingFilename');
    const recordBtn = document.getElementById('recordBtn');
    const pauseBtn = document.getElementById('pauseBtn');
    const stopBtn = document.getElementById('stopBtn');

    if (status.is_recording) {
        indicator.className = 'recording-indicator recording';
        statusText.textContent = status.is_paused ? '⏸ Paused' : '🔴 Recording';
        recordBtn.style.display = 'none';
        pauseBtn.style.display = status.is_paused ? 'inline-flex' : 'inline-flex';
        pauseBtn.textContent = status.is_paused ? '▶ Resume' : '⏸ Pause';
        pauseBtn.onclick = status.is_paused ? resumeRecording : pauseRecording;
        stopBtn.style.display = 'inline-flex';
        filename.textContent = status.filename || 'Recording...';
    } else {
        indicator.className = 'recording-indicator idle';
        statusText.textContent = '⏹ Idle';
        recordBtn.style.display = 'inline-flex';
        pauseBtn.style.display = 'none';
        stopBtn.style.display = 'none';
        filename.textContent = '';
    }
}

// ============================================================
// RECORDING MANAGEMENT
// ============================================================

async function downloadRecording(id) {
    const api = new ApiClient();
    const url = api.downloadRecording(id);
    window.open(url, '_blank');
}

async function deleteRecording(id) {
    if (!confirm('Are you sure you want to delete this recording?')) return;
    
    try {
        const api = new ApiClient();
        await api.deleteRecording(id);
        showNotification('Recording deleted', 'Success', 'success');
        refreshRecordings();
        refreshDashboard();
    } catch (error) {
        showNotification('Failed to delete recording: ' + error.message, 'Error', 'error');
    }
}

function copyPath(path) {
    navigator.clipboard.writeText(path).then(() => {
        showNotification('Path copied to clipboard', 'Success', 'success');
    }).catch(() => {
        // Fallback
        const input = document.createElement('input');
        input.value = path;
        document.body.appendChild(input);
        input.select();
        document.execCommand('copy');
        document.body.removeChild(input);
        showNotification('Path copied to clipboard', 'Success', 'success');
    });
}

// ============================================================
// CLOUD MANAGEMENT
// ============================================================

async function uploadToCloud() {
    const file = document.getElementById('uploadFilePath').value;
    const remote = document.getElementById('uploadRemotePath').value;
    
    if (!file) {
        showNotification('Please enter a file path', 'Warning', 'warning');
        return;
    }
    
    try {
        const api = new ApiClient();
        const result = await api.uploadToCloud(file, remote);
        showNotification('Upload started: ' + result.file, 'Success', 'success');
        refreshCloud();
    } catch (error) {
        showNotification('Upload failed: ' + error.message, 'Error', 'error');
    }
}

async function syncCloud() {
    try {
        const api = new ApiClient();
        await api.syncCloud();
        showNotification('Sync started', 'Info', 'info');
        refreshCloud();
    } catch (error) {
        showNotification('Sync failed: ' + error.message, 'Error', 'error');
    }
}

async function disconnectCloud() {
    if (!confirm('Disconnect from cloud?')) return;
    
    try {
        const api = new ApiClient();
        await api.disconnectCloud();
        showNotification('Cloud disconnected', 'Info', 'info');
        refreshCloud();
    } catch (error) {
        showNotification('Disconnect failed: ' + error.message, 'Error', 'error');
    }
}

function showConnectCloud() {
    const container = document.getElementById('cloudDetails');
    container.innerHTML = `
        <div style="padding:1rem 0;">
            <div class="form-group">
                <label>Provider</label>
                <select class="form-control" id="cloudProvider">
                    <option value="nextcloud">Nextcloud</option>
                    <option value="onedrive">OneDrive</option>
                    <option value="s3">AWS S3</option>
                    <option value="webdav">WebDAV</option>
                </select>
            </div>
            <div class="form-group">
                <label>URL</label>
                <input type="text" class="form-control" id="cloudUrl" placeholder="https://cloud.example.com">
            </div>
            <div class="form-group">
                <label>Username</label>
                <input type="text" class="form-control" id="cloudUsername">
            </div>
            <div class="form-group">
                <label>Password / API Key</label>
                <input type="password" class="form-control" id="cloudPassword">
            </div>
            <button class="btn btn-primary" onclick="connectCloud()">Connect</button>
            <button class="btn btn-outline" onclick="refreshCloud()">Cancel</button>
        </div>
    `;
}

async function connectCloud() {
    const provider = document.getElementById('cloudProvider').value;
    const url = document.getElementById('cloudUrl').value;
    const username = document.getElementById('cloudUsername').value;
    const password = document.getElementById('cloudPassword').value;
    
    if (!url || !username) {
        showNotification('Please fill in required fields', 'Warning', 'warning');
        return;
    }
    
    try {
        const api = new ApiClient();
        await api.connectCloud(provider, url, username, password);
        showNotification('Cloud connected successfully', 'Success', 'success');
        refreshCloud();
    } catch (error) {
        showNotification('Connection failed: ' + error.message, 'Error', 'error');
    }
}

// ============================================================
// WIFI MANAGEMENT
// ============================================================

async function scanWiFi() {
    try {
        const api = new ApiClient();
        const result = await api.scanWiFi();
        
        const container = document.getElementById('wifiNetworks');
        if (result.networks && result.networks.length > 0) {
            let html = `
                <div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:0.5rem;margin-top:0.5rem;">
            `;
            result.networks.forEach(net => {
                const strength = net.signal > -50 ? '🟢' : net.signal > -70 ? '🟡' : '🔴';
                html += `
                    <div style="background:var(--bg-secondary);padding:0.75rem;border-radius:8px;cursor:pointer;" onclick="connectToWiFi('${net.ssid}')">
                        <div style="font-weight:600;">${net.ssid}</div>
                        <div style="font-size:0.75rem;color:var(--text-muted);">
                            ${strength} ${net.signal} dBm | ${net.security || 'Open'}
                        </div>
                    </div>
                `;
            });
            html += `</div>`;
            container.innerHTML = html;
        } else {
            container.innerHTML = '<div style="text-align:center;padding:1rem;color:var(--text-muted);">No WiFi networks found</div>';
        }
    } catch (error) {
        showNotification('WiFi scan failed: ' + error.message, 'Error', 'error');
    }
}

function connectToWiFi(ssid) {
    const password = prompt(`Enter password for ${ssid}:`);
    if (password === null) return;
    
    connectWiFiNetwork(ssid, password);
}

async function connectWiFiNetwork(ssid, password) {
    try {
        const api = new ApiClient();
        await api.connectWiFi(ssid, password);
        showNotification(`Connected to ${ssid}`, 'Success', 'success');
        refreshNetwork();
    } catch (error) {
        showNotification('WiFi connection failed: ' + error.message, 'Error', 'error');
    }
}

// ============================================================
// GPIO CONTROL
// ============================================================

async function toggleGPIOPin(pin, value) {
    try {
        const api = new ApiClient();
        await api.setGPIOPin(pin, value);
        refreshHardware();
    } catch (error) {
        showNotification('GPIO control failed: ' + error.message, 'Error', 'error');
    }
}

// ============================================================
// SETTINGS
// ============================================================

function saveApiToken() {
    const token = document.getElementById('apiTokenInput').value;
    const api = new ApiClient();
    api.setToken(token);
    showNotification('API token saved', 'Success', 'success');
}

function confirmReboot() {
    if (confirm('Are you sure you want to reboot the system?')) {
        rebootSystem();
    }
}

async function rebootSystem() {
    try {
        const api = new ApiClient();
        await api.rebootSystem();
        showNotification('System rebooting...', 'Info', 'info');
    } catch (error) {
        showNotification('Reboot failed: ' + error.message, 'Error', 'error');
    }
}

function confirmShutdown() {
    if (confirm('Are you sure you want to shutdown the system?')) {
        shutdownSystem();
    }
}

async function shutdownSystem() {
    try {
        const api = new ApiClient();
        await api.shutdownSystem();
        showNotification('System shutting down...', 'Info', 'info');
    } catch (error) {
        showNotification('Shutdown failed: ' + error.message, 'Error', 'error');
    }
}

// ============================================================
// NOTIFICATION HELPER
// ============================================================

let notificationManager = null;

function getNotificationManager() {
    if (!notificationManager) {
        notificationManager = new NotificationManager();
    }
    return notificationManager;
}

function showNotification(message, title = '', type = 'info', duration = 4000) {
    return getNotificationManager().show(message, title, type, duration);
}

// ============================================================
// WEBSOCKET CONNECTION
// ============================================================

function connectWebSocket() {
    if (AppState.wsConnected) return;
    
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    
    try {
        const ws = new WebSocket(wsUrl);
        AppState.wsConnection = ws;
        
        ws.onopen = () => {
            console.log('WebSocket connected');
            AppState.wsConnected = true;
        };
        
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleWebSocketMessage(data);
            } catch (e) {
                console.warn('Invalid WebSocket message:', event.data);
            }
        };
        
        ws.onclose = () => {
            console.log('WebSocket disconnected');
            AppState.wsConnected = false;
            // Attempt to reconnect after 5 seconds
            setTimeout(connectWebSocket, 5000);
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
    } catch (error) {
        console.error('WebSocket connection failed:', error);
    }
}

function handleWebSocketMessage(data) {
    const event = data.event;
    const payload = data.data;
    
    switch (event) {
        case 'system.status':
            updateSystemStatus(payload);
            break;
        case 'recording.started':
            showNotification(`Recording started: ${payload.filename}`, 'Recording', 'success');
            AppState.isRecording = true;
            updateRecordingUI({ is_recording: true, filename: payload.filename });
            break;
        case 'recording.stopped':
            showNotification(`Recording stopped: ${payload.filename}`, 'Recording', 'info');
            AppState.isRecording = false;
            AppState.isPaused = false;
            updateRecordingUI({ is_recording: false });
            refreshRecordings();
            break;
        case 'recording.progress':
            updateRecordingProgress(payload);
            break;
        case 'storage.updated':
            updateStorageUI(payload);
            break;
        case 'cloud.synced':
            showNotification(`Cloud sync complete: ${payload.synced} files`, 'Cloud', 'success');
            refreshCloud();
            break;
        case 'hardware.gpio_changed':
            // Update GPIO status in real-time
            break;
        default:
            console.log('Unhandled WebSocket event:', event);
    }
}

function updateSystemStatus(data) {
    // Update status indicators
    if (data.status) {
        const statusDot = document.getElementById('statusDot');
        const statusText = document.getElementById('statusText');
        
        statusDot.className = 'dot ' + data.status;
        statusText.textContent = data.status.charAt(0).toUpperCase() + data.status.slice(1);
    }
}

function updateRecordingProgress(data) {
    // Update recording progress display
    const statusText = document.getElementById('recordingStatusText');
    if (data.duration) {
        statusText.textContent = `🔴 Recording: ${formatDuration(data.duration)}`;
    }
}

function updateStorageUI(data) {
    // Update storage metrics if visible
    const storageBar = document.getElementById('storageBar');
    const storageValue = document.getElementById('storageValue');
    if (storageBar && data.used) {
        const percent = getPercentage(data.used, data.total);
        storageBar.style.width = percent + '%';
        storageBar.className = 'progress-fill ' + getColorForPercentage(percent);
        storageValue.textContent = formatBytes(data.used) + ' / ' + formatBytes(data.total);
    }
}

// ============================================================
// TIME UPDATES
// ============================================================

function updateClock() {
    const now = new Date();
    const timeStr = now.toLocaleTimeString('en-US', { hour12: false });
    const dateStr = now.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: 'numeric' });
    DOM.timeDisplay.textContent = `${dateStr} ${timeStr}`;
}

// ============================================================
// KEYBOARD SHORTCUTS
// ============================================================

document.addEventListener('keydown', (e) => {
    // Alt + 1-9: Navigate to pages
    if (e.altKey && e.key >= '1' && e.key <= '9') {
        const pages = ['dashboard', 'recordings', 'storage', 'cloud', 'network', 'hardware', 'settings', 'logs'];
        const idx = parseInt(e.key) - 1;
        if (idx < pages.length) {
            e.preventDefault();
            navigateTo(pages[idx]);
        }
    }
    
    // Space: Toggle recording
    if (e.target.tagName !== 'INPUT' && e.target.tagName !== 'TEXTAREA' && e.key === ' ') {
        e.preventDefault();
        if (AppState.isRecording) {
            stopRecording();
        } else {
            startRecording();
        }
    }
    
    // Escape: Close sidebar
    if (e.key === 'Escape' && DOM.sidebar.classList.contains('open')) {
        toggleSidebar(false);
    }
});

// ============================================================
// INITIALIZATION
// ============================================================

document.addEventListener('DOMContentLoaded', () => {
    // Initialize sidebar
    DOM.menuToggle.addEventListener('click', () => toggleSidebar());
    DOM.sidebarOverlay.addEventListener('click', () => toggleSidebar(false));
    
    // Initialize navigation
    document.querySelectorAll('.nav-item').forEach(item => {
        item.addEventListener('click', () => {
            navigateTo(item.dataset.page);
        });
    });
    
    // Initialize page from URL or default
    const page = getQueryParam('page') || 'dashboard';
    navigateTo(page);
    
    // Start clock
    updateClock();
    setInterval(updateClock, 1000);
    
    // Connect WebSocket
    connectWebSocket();
    
    // Set up periodic refresh
    setInterval(() => {
        if (AppState.currentPage === 'dashboard') {
            refreshDashboard();
        }
    }, 10000);
    
    console.log('RPi5 Corporate Recording Station initialized');
});
