#include "WebSocketHandler.hpp"

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

// This is a simplified implementation - production would use a real WebSocket library

WebSocketHandler::WebSocketHandler() 
    : ws_handle(nullptr), running(false) {
}

WebSocketHandler::~WebSocketHandler() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool WebSocketHandler::initialize(int ws_port, int max_clients) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    port = ws_port;
    this->max_clients = max_clients;
    
    initialized = true;
    std::cout << "WebSocket Handler initialized on port " << port << std::endl;
    return true;
}

void WebSocketHandler::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    if (running) {
        stop();
    }
    
    initialized = false;
    std::cout << "WebSocket Handler shutdown" << std::endl;
}

// ============================================
// SERVER CONTROL
// ============================================

bool WebSocketHandler::start() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        on_error("Handler not initialized");
        return false;
    }
    
    if (running) {
        return true;
    }
    
    if (!startServer()) {
        return false;
    }
    
    running = true;
    stats.start_time = std::chrono::system_clock::now();
    
    accept_thread = std::thread(&WebSocketHandler::acceptLoop, this);
    ping_thread = std::thread(&WebSocketHandler::pingLoop, this);
    
    std::cout << "WebSocket Server started on port " << port << std::endl;
    return true;
}

void WebSocketHandler::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!running) {
        return;
    }
    
    running = false;
    
    if (accept_thread.joinable()) {
        accept_thread.join();
    }
    if (ping_thread.joinable()) {
        ping_thread.join();
    }
    
    stopServer();
    
    std::cout << "WebSocket Server stopped" << std::endl;
}

// ============================================
// CLIENT MANAGEMENT
// ============================================

std::vector<WebSocketHandler::Client> WebSocketHandler::getClients() const {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    std::vector<Client> result;
    for (const auto& [id, client] : clients) {
        if (client.state == State::CONNECTED) {
            result.push_back(client);
        }
    }
    return result;
}

WebSocketHandler::Client WebSocketHandler::getClient(int client_id) const {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    if (it != clients.end()) {
        return it->second;
    }
    return Client{};
}

size_t WebSocketHandler::getClientCount() const {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    size_t count = 0;
    for (const auto& [id, client] : clients) {
        if (client.state == State::CONNECTED) {
            count++;
        }
    }
    return count;
}

bool WebSocketHandler::disconnectClient(int client_id, const std::string& reason) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    if (it == clients.end()) {
        return false;
    }
    
    it->second.state = State::CLOSING;
    if (on_disconnect) {
        on_disconnect(client_id);
    }
    removeClient(client_id);
    
    return true;
}

void WebSocketHandler::disconnectAll(const std::string& reason) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    std::vector<int> client_ids;
    for (const auto& [id, client] : clients) {
        if (client.state == State::CONNECTED) {
            client_ids.push_back(id);
        }
    }
    
    for (int id : client_ids) {
        disconnectClient(id, reason);
    }
}

bool WebSocketHandler::setClientMetadata(int client_id, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    if (it == clients.end()) {
        return false;
    }
    
    it->second.metadata[key] = value;
    return true;
}

std::string WebSocketHandler::getClientMetadata(int client_id, const std::string& key) const {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    if (it == clients.end()) {
        return "";
    }
    
    auto mit = it->second.metadata.find(key);
    if (mit != it->second.metadata.end()) {
        return mit->second;
    }
    return "";
}

// ============================================
// MESSAGE HANDLING
// ============================================

bool WebSocketHandler::sendMessage(int client_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    if (it == clients.end() || it->second.state != State::CONNECTED) {
        return false;
    }
    
    // In production, this would send the message over the WebSocket connection
    // This is a placeholder
    updateStatsMessageSent(message.size());
    it->second.messages_sent++;
    it->second.last_activity = std::chrono::system_clock::now();
    
    return true;
}

bool WebSocketHandler::sendBinary(int client_id, const std::vector<uint8_t>& data) {
    // Convert binary to string (placeholder)
    std::string message(data.begin(), data.end());
    return sendMessage(client_id, message);
}

size_t WebSocketHandler::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    size_t sent_count = 0;
    for (auto& [id, client] : clients) {
        if (client.state == State::CONNECTED) {
            if (sendMessage(id, message)) {
                sent_count++;
            }
        }
    }
    return sent_count;
}

size_t WebSocketHandler::broadcastTo(const std::string& message, std::function<bool(const Client&)> filter) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    size_t sent_count = 0;
    for (auto& [id, client] : clients) {
        if (client.state == State::CONNECTED && filter(client)) {
            if (sendMessage(id, message)) {
                sent_count++;
            }
        }
    }
    return sent_count;
}

bool WebSocketHandler::queueMessage(int client_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    auto it = clients.find(client_id);
    if (it == clients.end()) {
        return false;
    }
    
    Message msg;
    msg.client_id = client_id;
    msg.data = message;
    msg.is_text = true;
    msg.timestamp = std::chrono::system_clock::now();
    
    message_queues[client_id].push(msg);
    return true;
}

size_t WebSocketHandler::flushQueue(int client_id) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    auto it = message_queues.find(client_id);
    if (it == message_queues.end()) {
        return 0;
    }
    
    size_t sent_count = 0;
    while (!it->second.empty()) {
        const Message& msg = it->second.front();
        if (sendMessage(client_id, msg.data)) {
            sent_count++;
        }
        it->second.pop();
    }
    return sent_count;
}

void WebSocketHandler::clearQueue(int client_id) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    auto it = message_queues.find(client_id);
    if (it != message_queues.end()) {
        while (!it->second.empty()) {
            it->second.pop();
        }
    }
}

// ============================================
// STATISTICS
// ============================================

WebSocketHandler::Stats WebSocketHandler::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.current_connections = getClientCount();
    if (stats.current_connections > stats.peak_connections) {
        stats.peak_connections = stats.current_connections;
    }
    return stats;
}

void WebSocketHandler::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = Stats{};
    stats.start_time = std::chrono::system_clock::now();
}

// ============================================
// CALLBACKS
// ============================================

void WebSocketHandler::setOnMessage(MessageCallback callback) {
    on_message = callback;
}

void WebSocketHandler::setOnConnect(ConnectCallback callback) {
    on_connect = callback;
}

void WebSocketHandler::setOnDisconnect(DisconnectCallback callback) {
    on_disconnect = callback;
}

void WebSocketHandler::setOnError(ErrorCallback callback) {
    on_error = callback;
}

// ============================================
// CONFIGURATION
// ============================================

void WebSocketHandler::setMaxMessageSize(size_t size) {
    max_message_size = size;
}

void WebSocketHandler::setPingInterval(int seconds) {
    ping_interval = std::max(5, seconds);
}

void WebSocketHandler::setTimeout(int seconds) {
    timeout = std::max(10, seconds);
}

void WebSocketHandler::setAuthEnabled(bool enable) {
    auth_enabled = enable;
}

void WebSocketHandler::setAuthToken(const std::string& token) {
    auth_token = token;
}

// ============================================
// PRIVATE METHODS (SERVER IMPLEMENTATION)
// ============================================

bool WebSocketHandler::startServer() {
    // In production, this would use a WebSocket library
    // This is a placeholder
    ws_handle = (void*)1;
    return true;
}

void WebSocketHandler::stopServer() {
    ws_handle = nullptr;
}

void WebSocketHandler::acceptLoop() {
    while (running) {
        // In production, this would accept WebSocket connections
        // This is a placeholder
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void WebSocketHandler::pingLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(ping_interval));
        
        // Send ping to all connected clients
        for (auto& [id, client] : clients) {
            if (client.state == State::CONNECTED) {
                // Send ping (placeholder)
                updateStatsMessageSent(2);
            }
        }
    }
}

int WebSocketHandler::addClient(const std::string& address) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    int id = next_client_id++;
    Client client;
    client.id = id;
    client.address = address;
    client.state = State::CONNECTED;
    client.connected_at = std::chrono::system_clock::now();
    client.last_activity = client.connected_at;
    client.authenticated = !auth_enabled;
    
    clients[id] = client;
    updateStatsConnection();
    
    if (on_connect) {
        on_connect(id);
    }
    
    return id;
}

void WebSocketHandler::removeClient(int client_id) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    if (it != clients.end()) {
        if (on_disconnect) {
            on_disconnect(client_id);
        }
        clients.erase(it);
    }
}

bool WebSocketHandler::validateClient(int client_id) const {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    return it != clients.end() && it->second.state == State::CONNECTED;
}

void WebSocketHandler::updateClientActivity(int client_id) {
    std::lock_guard<std::mutex> lock(client_mutex);
    
    auto it = clients.find(client_id);
    if (it != clients.end()) {
        it->second.last_activity = std::chrono::system_clock::now();
    }
}

void WebSocketHandler::processMessage(int client_id, const std::string& data) {
    // Parse incoming message
    Message msg;
    msg.client_id = client_id;
    msg.data = data;
    msg.is_text = true;
    msg.timestamp = std::chrono::system_clock::now();
    
    handleIncomingMessage(client_id, msg);
}

void WebSocketHandler::handleIncomingMessage(int client_id, const Message& msg) {
    updateClientActivity(client_id);
    updateStatsMessageReceived(msg.data.size());
    
    if (on_message) {
        on_message(msg);
    }
}

bool WebSocketHandler::authenticateClient(int client_id, const std::string& token) {
    if (!auth_enabled) {
        return true;
    }
    
    if (token == auth_token) {
        std::lock_guard<std::mutex> lock(client_mutex);
        auto it = clients.find(client_id);
        if (it != clients.end()) {
            it->second.authenticated = true;
            return true;
        }
    }
    return false;
}

void WebSocketHandler::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "WebSocket Error: " << error << std::endl;
}

void WebSocketHandler::updateStatsMessageReceived(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.total_messages_received++;
    stats.total_bytes_received += bytes;
}

void WebSocketHandler::updateStatsMessageSent(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.total_messages_sent++;
    stats.total_bytes_sent += bytes;
}

void WebSocketHandler::updateStatsConnection() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.total_connections++;
    stats.current_connections = getClientCount();
    if (stats.current_connections > stats.peak_connections) {
        stats.peak_connections = stats.current_connections;
    }
}

std::string WebSocketHandler::getCurrentTimeStr() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}
