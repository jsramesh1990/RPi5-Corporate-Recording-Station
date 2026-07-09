#include "TCPClient.hpp"

#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

TCPClient::TCPClient() : socket_fd(-1), async_running(false), state(State::DISCONNECTED) {
}

TCPClient::~TCPClient() {
    disconnect();
}

// ============================================
// CONNECTION MANAGEMENT
// ============================================

bool TCPClient::connect(const TCPConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == State::CONNECTED) {
        disconnect();
    }
    
    config = cfg;
    setState(State::CONNECTING);
    
    if (createSocket() && setSocketOptions() && connectSocket()) {
        setState(State::CONNECTED);
        stats.connected_since = std::chrono::system_clock::now();
        stats.last_activity = stats.connected_since;
        notifyStateChange();
        
        std::cout << "TCP connected to " << config.host << ":" << config.port << std::endl;
        return true;
    }
    
    setState(State::ERROR);
    notifyError("Connection failed: " + getSocketError());
    return false;
}

bool TCPClient::connect(const std::string& host, int port) {
    TCPConfig cfg;
    cfg.host = host;
    cfg.port = port;
    return connect(cfg);
}

void TCPClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (async_running) {
        stopAsyncReceive();
    }
    
    closeSocket();
    setState(State::DISCONNECTED);
    notifyStateChange();
    
    std::cout << "TCP disconnected" << std::endl;
}

std::string TCPClient::getStateName() const {
    switch (state) {
        case State::DISCONNECTED: return "Disconnected";
        case State::CONNECTING: return "Connecting";
        case State::CONNECTED: return "Connected";
        case State::RECONNECTING: return "Reconnecting";
        case State::ERROR: return "Error";
        default: return "Unknown";
    }
}

// ============================================
// DATA TRANSMISSION
// ============================================

bool TCPClient::sendData(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isSocketValid() || state != State::CONNECTED) {
        return false;
    }
    
    ssize_t sent = send(socket_fd, data.data(), data.size(), 0);
    if (sent < 0) {
        stats.send_errors++;
        notifyError("Send error: " + getSocketError());
        return false;
    }
    
    updateStatsSent(sent);
    return true;
}

bool TCPClient::sendData(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return sendData(bytes);
}

bool TCPClient::sendMessage(const std::vector<uint8_t>& data) {
    // Send length prefix (4 bytes) + data
    uint32_t length = htonl(data.size());
    std::vector<uint8_t> message;
    message.reserve(4 + data.size());
    
    message.push_back((length >> 24) & 0xFF);
    message.push_back((length >> 16) & 0xFF);
    message.push_back((length >> 8) & 0xFF);
    message.push_back(length & 0xFF);
    message.insert(message.end(), data.begin(), data.end());
    
    return sendData(message);
}

bool TCPClient::sendJSON(const std::string& json) {
    return sendMessage(std::vector<uint8_t>(json.begin(), json.end()));
}

bool TCPClient::sendFile(const std::string& file_path) {
    // This would read the file and send it in chunks
    // Simplified implementation
    return false;
}

// ============================================
// DATA RECEPTION
// ============================================

void TCPClient::setOnData(DataCallback callback) {
    data_callback = callback;
}

std::vector<uint8_t> TCPClient::readData(size_t length) {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<uint8_t> data;
    
    if (!isSocketValid() || state != State::CONNECTED) {
        return data;
    }
    
    data.resize(length);
    ssize_t read_bytes = recv(socket_fd, data.data(), length, 0);
    
    if (read_bytes < 0) {
        stats.receive_errors++;
        notifyError("Receive error: " + getSocketError());
        return std::vector<uint8_t>();
    }
    
    data.resize(read_bytes);
    updateStatsReceived(read_bytes);
    return data;
}

std::string TCPClient::readLine() {
    std::string line;
    char c;
    
    while (true) {
        auto data = readData(1);
        if (data.empty()) {
            break;
        }
        c = data[0];
        if (c == '\n') {
            break;
        }
        if (c != '\r') {
            line += c;
        }
    }
    
    return line;
}

std::string TCPClient::readJSON() {
    // Read length prefix (4 bytes)
    auto len_data = readData(4);
    if (len_data.size() < 4) {
        return "";
    }
    
    uint32_t length = (len_data[0] << 24) | (len_data[1] << 16) |
                      (len_data[2] << 8) | len_data[3];
    
    auto data = readData(length);
    return std::string(data.begin(), data.end());
}

// ============================================
// ASYNCHRONOUS OPERATIONS
// ============================================

bool TCPClient::startAsyncReceive() {
    if (async_running) {
        return false;
    }
    
    if (state != State::CONNECTED) {
        return false;
    }
    
    async_running = true;
    async_thread = std::thread(&TCPClient::receiveLoop, this);
    return true;
}

void TCPClient::stopAsyncReceive() {
    if (!async_running) {
        return;
    }
    
    async_running = false;
    if (async_thread.joinable()) {
        async_thread.join();
    }
}

void TCPClient::receiveLoop() {
    std::vector<uint8_t> buffer(config.buffer_size);
    
    while (async_running && state == State::CONNECTED) {
        if (!isSocketValid()) {
            break;
        }
        
        ssize_t read_bytes = recv(socket_fd, buffer.data(), buffer.size(), 0);
        
        if (read_bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            stats.receive_errors++;
            notifyError("Receive error: " + getSocketError());
            break;
        }
        
        if (read_bytes == 0) {
            // Connection closed by peer
            break;
        }
        
        updateStatsReceived(read_bytes);
        
        if (data_callback) {
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + read_bytes);
            data_callback(data);
        }
    }
}

// ============================================
// PRIVATE METHODS
// ============================================

bool TCPClient::createSocket() {
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        notifyError("Failed to create socket: " + getSocketError());
        return false;
    }
    return true;
}

void TCPClient::closeSocket() {
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }
}

bool TCPClient::setSocketOptions() {
    if (socket_fd < 0) {
        return false;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = config.timeout_ms / 1000;
    timeout.tv_usec = (config.timeout_ms % 1000) * 1000;
    
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Enable keep-alive
    if (config.keep_alive) {
        int keepalive = 1;
        setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    }
    
    // Set TCP_NODELAY
    int nodelay = 1;
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    
    return true;
}

bool TCPClient::connectSocket() {
    struct hostent* server = gethostbyname(config.host.c_str());
    if (server == nullptr) {
        notifyError("Failed to resolve host: " + config.host);
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    addr.sin_port = htons(config.port);
    
    if (::connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }
    
    // Get local address info
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    if (getsockname(socket_fd, (struct sockaddr*)&local_addr, &len) == 0) {
        local_address = inet_ntoa(local_addr.sin_addr);
        local_port = ntohs(local_addr.sin_port);
    }
    
    remote_address = inet_ntoa(addr.sin_addr);
    remote_port = ntohs(addr.sin_port);
    
    return true;
}

void TCPClient::reconnectLoop() {
    while (config.auto_reconnect && state == State::RECONNECTING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
        
        if (connect(config)) {
            stats.reconnections++;
            break;
        }
    }
}

// ============================================
// STATISTICS
// ============================================

TCPClient::TCPStats TCPClient::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void TCPClient::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    resetStatsImpl();
}

void TCPClient::resetStatsImpl() {
    stats = TCPStats{};
}

void TCPClient::updateStatsSent(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.bytes_sent += bytes;
    stats.messages_sent++;
    stats.last_activity = std::chrono::system_clock::now();
}

void TCPClient::updateStatsReceived(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.bytes_received += bytes;
    stats.messages_received++;
    stats.last_activity = std::chrono::system_clock::now();
}

// ============================================
// STATE MANAGEMENT
// ============================================

void TCPClient::setState(State new_state) {
    if (state != new_state) {
        state = new_state;
        notifyStateChange();
    }
}

void TCPClient::notifyStateChange() {
    if (on_state_change) {
        on_state_change(state);
    }
}

void TCPClient::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "TCP Error: " << error << std::endl;
}

void TCPClient::notifyStats() {
    if (on_stats) {
        on_stats(getStats());
    }
}

// ============================================
// CALLBACKS
// ============================================

void TCPClient::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void TCPClient::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void TCPClient::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

// ============================================
// UTILITY
// ============================================

std::string TCPClient::getLocalAddress() const {
    return local_address;
}

std::string TCPClient::getRemoteAddress() const {
    return remote_address;
}

int TCPClient::getLocalPort() const {
    return local_port;
}

int TCPClient::getRemotePort() const {
    return remote_port;
}

bool TCPClient::isSocketValid() const {
    return socket_fd >= 0;
}

std::string TCPClient::getSocketError() const {
    return strerror(errno);
}

void TCPClient::delayMilliseconds(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

std::string TCPClient::getCurrentTimeStr() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}
