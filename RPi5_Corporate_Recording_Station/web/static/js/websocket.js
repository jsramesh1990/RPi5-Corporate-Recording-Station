/**
 * RPi5 Corporate Recording Station - WebSocket Handler
 * 
 * Manages WebSocket connections for real-time updates including:
 * - Connection management with auto-reconnect
 * - Message queuing and delivery
 * - Event handling and routing
 * - Heartbeat/ping support
 */

// ============================================================
// WEBSOCKET MANAGER
// ============================================================

class WebSocketManager {
    constructor(options = {}) {
        this.url = options.url || this.getDefaultUrl();
        this.protocols = options.protocols || [];
        this.reconnectInterval = options.reconnectInterval || 5000;
        this.maxReconnectAttempts = options.maxReconnectAttempts || 10;
        this.pingInterval = options.pingInterval || 30000;
        this.messageQueue = [];
        this.connectionId = null;
        this.eventHandlers = new Map();
        this.reconnectAttempts = 0;
        this.isConnecting = false;
        this.shouldReconnect = true;
        
        // State
        this.ws = null;
        this.isConnected = false;
        this.isAuthenticated = false;
        this.pingTimer = null;
        this.reconnectTimer = null;
        
        // Event callbacks
        this.onOpenCallbacks = [];
        this.onCloseCallbacks = [];
        this.onErrorCallbacks = [];
        this.onReconnectCallbacks = [];
        this.onAuthenticatedCallbacks = [];
    }
    
    getDefaultUrl() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.host;
        return `${protocol}//${host}/ws`;
    }
    
    // ============================================================
    // CONNECTION MANAGEMENT
    // ============================================================
    
    connect() {
        if (this.isConnecting || this.ws?.readyState === WebSocket.OPEN) {
            return;
        }
        
        this.isConnecting = true;
        console.log('WebSocket: Connecting to', this.url);
        
        try {
            this.ws = new WebSocket(this.url, this.protocols);
            this.ws.onopen = this.handleOpen.bind(this);
            this.ws.onmessage = this.handleMessage.bind(this);
            this.ws.onclose = this.handleClose.bind(this);
            this.ws.onerror = this.handleError.bind(this);
        } catch (error) {
            console.error('WebSocket: Connection error', error);
            this.isConnecting = false;
            this.scheduleReconnect();
        }
    }
    
    disconnect() {
        this.shouldReconnect = false;
        this.clearTimers();
        
        if (this.ws) {
            if (this.ws.readyState === WebSocket.OPEN) {
                this.ws.close(1000, 'Client disconnect');
            }
            this.ws = null;
        }
        
        this.isConnected = false;
        this.isAuthenticated = false;
        console.log('WebSocket: Disconnected');
    }
    
    reconnect() {
        if (!this.shouldReconnect) return;
        
        this.reconnectAttempts++;
        if (this.reconnectAttempts > this.maxReconnectAttempts) {
            console.warn('WebSocket: Max reconnect attempts reached');
            this.notifyError(new Error('Max reconnect attempts reached'));
            return;
        }
        
        console.log(`WebSocket: Reconnecting (attempt ${this.reconnectAttempts})`);
        this.notifyReconnect(this.reconnectAttempts);
        this.connect();
    }
    
    scheduleReconnect() {
        this.clearTimers();
        this.reconnectTimer = setTimeout(() => {
            this.reconnect();
        }, this.reconnectInterval);
    }
    
    clearTimers() {
        if (this.pingTimer) {
            clearInterval(this.pingTimer);
            this.pingTimer = null;
        }
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
    }
    
    // ============================================================
    // EVENT HANDLING
    // ============================================================
    
    on(event, handler) {
        if (!this.eventHandlers.has(event)) {
            this.eventHandlers.set(event, []);
        }
        this.eventHandlers.get(event).push(handler);
    }
    
    off(event, handler) {
        if (!this.eventHandlers.has(event)) return;
        const handlers = this.eventHandlers.get(event);
        const index = handlers.indexOf(handler);
        if (index !== -1) {
            handlers.splice(index, 1);
        }
    }
    
    once(event, handler) {
        const onceHandler = (data) => {
            this.off(event, onceHandler);
            handler(data);
        };
        this.on(event, onceHandler);
    }
    
    emit(event, data) {
        if (!this.eventHandlers.has(event)) return;
        const handlers = this.eventHandlers.get(event);
        handlers.forEach(handler => {
            try {
                handler(data);
            } catch (error) {
                console.error('WebSocket event handler error:', error);
            }
        });
    }
    
    // ============================================================
    // MESSAGE HANDLING
    // ============================================================
    
    send(data) {
        if (this.isConnected && this.ws?.readyState === WebSocket.OPEN) {
            this.ws.send(typeof data === 'string' ? data : JSON.stringify(data));
            return true;
        } else {
            // Queue message for later
            this.messageQueue.push(data);
            return false;
        }
    }
    
    sendJSON(data) {
        return this.send(JSON.stringify(data));
    }
    
    sendAuthenticated(data) {
        if (!this.isAuthenticated) {
            console.warn('WebSocket: Not authenticated, queuing message');
            this.messageQueue.push(data);
            return false;
        }
        return this.send(data);
    }
    
    authenticate(token) {
        this.sendJSON({
            type: 'auth',
            token: token
        });
    }
    
    flushQueue() {
        if (this.messageQueue.length === 0) return;
        
        console.log(`WebSocket: Flushing ${this.messageQueue.length} queued messages`);
        const queue = [...this.messageQueue];
        this.messageQueue = [];
        
        queue.forEach(data => {
            this.send(data);
        });
    }
    
    // ============================================================
    // WEBSOCKET EVENT HANDLERS
    // ============================================================
    
    handleOpen(event) {
        console.log('WebSocket: Connected');
        this.isConnecting = false;
        this.isConnected = true;
        this.reconnectAttempts = 0;
        
        // Start ping timer
        this.pingTimer = setInterval(() => {
            if (this.isConnected) {
                this.sendJSON({ type: 'ping', timestamp: Date.now() });
            }
        }, this.pingInterval);
        
        // Flush message queue
        this.flushQueue();
        
        // Notify open callbacks
        this.onOpenCallbacks.forEach(cb => cb(event));
        this.emit('open', event);
    }
    
    handleMessage(event) {
        try {
            const data = JSON.parse(event.data);
            
            // Handle ping/pong
            if (data.type === 'pong') {
                this.emit('pong', data);
                return;
            }
            
            if (data.type === 'auth_response') {
                this.isAuthenticated = data.success;
                if (this.isAuthenticated) {
                    console.log('WebSocket: Authenticated');
                    this.onAuthenticatedCallbacks.forEach(cb => cb());
                    this.emit('authenticated', data);
                } else {
                    console.warn('WebSocket: Authentication failed', data.message);
                }
                return;
            }
            
            // Handle event
            if (data.event) {
                this.emit(data.event, data.data);
            }
            
            // Generic message handler
            this.emit('message', data);
            
        } catch (error) {
            console.warn('WebSocket: Invalid message', event.data);
        }
    }
    
    handleClose(event) {
        console.log('WebSocket: Closed', event.code, event.reason);
        this.isConnected = false;
        this.isConnecting = false;
        this.clearTimers();
        
        // Notify close callbacks
        this.onCloseCallbacks.forEach(cb => cb(event));
        this.emit('close', event);
        
        // Schedule reconnect
        if (this.shouldReconnect && event.code !== 1000) {
            this.scheduleReconnect();
        }
    }
    
    handleError(event) {
        console.error('WebSocket: Error', event);
        this.notifyError(event);
        this.emit('error', event);
    }
    
    // ============================================================
    // NOTIFICATION HELPERS
    // ============================================================
    
    notifyOpen(callback) {
        this.onOpenCallbacks.push(callback);
    }
    
    notifyClose(callback) {
        this.onCloseCallbacks.push(callback);
    }
    
    notifyError(callback) {
        if (typeof callback === 'function') {
            this.onErrorCallbacks.push(callback);
        } else {
            this.onErrorCallbacks.forEach(cb => cb(callback));
        }
        this.emit('error', callback);
    }
    
    notifyReconnect(callback) {
        if (typeof callback === 'function') {
            this.onReconnectCallbacks.push(callback);
        } else {
            this.onReconnectCallbacks.forEach(cb => cb(callback));
        }
        this.emit('reconnect', callback);
    }
    
    notifyAuthenticated(callback) {
        this.onAuthenticatedCallbacks.push(callback);
    }
    
    // ============================================================
    // UTILITY
    // ============================================================
    
    getState() {
        if (!this.ws) return 'disconnected';
        switch (this.ws.readyState) {
            case WebSocket.CONNECTING: return 'connecting';
            case WebSocket.OPEN: return 'connected';
            case WebSocket.CLOSING: return 'closing';
            case WebSocket.CLOSED: return 'disconnected';
            default: return 'unknown';
        }
    }
    
    isReady() {
        return this.isConnected && this.isAuthenticated;
    }
    
    setUrl(url) {
        if (this.isConnected) {
            console.warn('WebSocket: Cannot change URL while connected');
            return;
        }
        this.url = url;
    }
}

// ============================================================
// WEBSOCKET EVENTS
// ============================================================

const WebSocketEvents = {
    // System events
    SYSTEM_STATUS: 'system.status',
    SYSTEM_CONNECTED: 'system.connected',
    
    // Recording events
    RECORDING_STARTED: 'recording.started',
    RECORDING_STOPPED: 'recording.stopped',
    RECORDING_PROGRESS: 'recording.progress',
    RECORDING_PAUSED: 'recording.paused',
    RECORDING_RESUMED: 'recording.resumed',
    
    // Storage events
    STORAGE_UPDATED: 'storage.updated',
    STORAGE_FILE_ADDED: 'storage.file_added',
    STORAGE_FILE_MODIFIED: 'storage.file_modified',
    STORAGE_FILE_DELETED: 'storage.file_deleted',
    
    // Cloud events
    CLOUD_CONNECTED: 'cloud.connected',
    CLOUD_DISCONNECTED: 'cloud.disconnected',
    CLOUD_UPLOAD_STARTED: 'cloud.upload_started',
    CLOUD_UPLOAD_PROGRESS: 'cloud.upload_progress',
    CLOUD_UPLOAD_COMPLETE: 'cloud.upload_complete',
    CLOUD_UPLOAD_FAILED: 'cloud.upload_failed',
    CLOUD_SYNCED: 'cloud.synced',
    
    // Hardware events
    HARDWARE_GPIO_CHANGED: 'hardware.gpio_changed',
    HARDWARE_BUTTON_PRESSED: 'hardware.button_pressed',
    HARDWARE_TEMPERATURE_WARNING: 'hardware.temperature_warning',
    
    // Security events
    SECURITY_AUTHENTICATION: 'security.authentication',
    SECURITY_AUTHORIZATION: 'security.authorization',
    
    // Custom events
    CUSTOM: 'custom'
};

// ============================================================
// SINGLETON INSTANCE
// ============================================================

let wsManagerInstance = null;

function getWebSocketManager(options = {}) {
    if (!wsManagerInstance) {
        wsManagerInstance = new WebSocketManager(options);
    }
    return wsManagerInstance;
}

// ============================================================
// REACT HOOKS (For React/React-like usage)
// ============================================================

function useWebSocket(options = {}) {
    const manager = getWebSocketManager(options);
    
    return {
        connect: () => manager.connect(),
        disconnect: () => manager.disconnect(),
        send: (data) => manager.send(data),
        sendJSON: (data) => manager.sendJSON(data),
        on: (event, handler) => manager.on(event, handler),
        off: (event, handler) => manager.off(event, handler),
        once: (event, handler) => manager.once(event, handler),
        isConnected: manager.isConnected,
        isAuthenticated: manager.isAuthenticated,
        getState: () => manager.getState(),
        isReady: () => manager.isReady()
    };
}

// ============================================================
// EXPORT
// ============================================================

// For browser usage
if (typeof window !== 'undefined') {
    window.WebSocketManager = WebSocketManager;
    window.WebSocketEvents = WebSocketEvents;
    window.getWebSocketManager = getWebSocketManager;
    window.useWebSocket = useWebSocket;
}
