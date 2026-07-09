#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <cstdint>

/**
 * @brief TCP Client for Recording Station
 * 
 * Provides TCP client functionality with support for
 * connection management, data transmission, and
 * asynchronous operations.
 */
class TCPClient {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        ERROR
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief TCP configuration
     */
    struct TCPConfig {
        std::string host = "localhost";
        int port = 8080;
        int timeout_ms = 5000;
        int retry_count = 3;
        int retry_delay_ms = 1000;
        int buffer_size = 8192;
        bool keep_alive = true;
        bool auto_reconnect = true;
        bool ssl_enabled = false;
        std::string ssl_cert_path;
    };
    
    /**
     * @brief Connection statistics
     */
    struct TCPStats {
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t send_errors = 0;
        uint64_t receive_errors = 0;
        uint64_t reconnections = 0;
        std::chrono::system_clock::time_point connected_since;
        std::chrono::system_clock::time_point last_activity;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    using StateCallback = std::function<void(State)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using StatsCallback = std::function<void(const TCPStats&)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    TCPClient();
    ~TCPClient();
    
    // ============================================
    // CONNECTION MANAGEMENT
    // ============================================
    
    /**
     * @brief Connect to server
     * @param config TCP configuration
     * @return true if successful
     */
    bool connect(const TCPConfig& config);
    
    /**
     * @brief Connect with parameters
     * @param host Server host
     * @param port Server port
     * @return true if successful
     */
    bool connect(const std::string& host, int port);
    
    /**
     * @brief Disconnect from server
     */
    void disconnect();
    
    /**
     * @brief Check if connected
     */
    bool isConnected() const { return state == State::CONNECTED; }
    
    /**
     * @brief Get current state
     */
    State getState() const { return state; }
    
    /**
     * @brief Get state name
     */
    std::string getStateName() const;
    
    /**
     * @brief Get configuration
     */
    TCPConfig getConfig() const { return config; }
    
    // ============================================
    // DATA TRANSMISSION
    // ============================================
    
    /**
     * @brief Send data
     * @param data Data to send
     * @return true if successful
     */
    bool sendData(const std::vector<uint8_t>& data);
    
    /**
     * @brief Send string
     * @param data String to send
     * @return true if successful
     */
    bool sendData(const std::string& data);
    
    /**
     * @brief Send message with length prefix
     * @param data Data to send
     * @return true if successful
     */
    bool sendMessage(const std::vector<uint8_t>& data);
    
    /**
     * @brief Send JSON message
     * @param json JSON string
     * @return true if successful
     */
    bool sendJSON(const std::string& json);
    
    /**
     * @brief Send file
     * @param file_path File path
     * @return true if successful
     */
    bool sendFile(const std::string& file_path);
    
    // ============================================
    // DATA RECEPTION
    // ============================================
    
    /**
     * @brief Set data callback
     * @param callback Function called with received data
     */
    void setOnData(DataCallback callback);
    
    /**
     * @brief Read data (blocking)
     * @param length Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> readData(size_t length);
    
    /**
     * @brief Read line (blocking)
     * @return Line read
     */
    std::string readLine();
    
    /**
     * @brief Read JSON message
     * @return JSON string
     */
    std::string readJSON();
    
    // ============================================
    // ASYNCHRONOUS OPERATIONS
    // ============================================
    
    /**
     * @brief Start async receive
     * @return true if successful
     */
    bool startAsyncReceive();
    
    /**
     * @brief Stop async receive
     */
    void stopAsyncReceive();
    
    /**
     * @brief Check if async receive is running
     */
    bool isAsyncReceiveRunning() const { return async_running; }
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get statistics
     */
    TCPStats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnStateChange(StateCallback callback);
    void setOnError(ErrorCallback callback);
    void setOnStats(StatsCallback callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get local address
     */
    std::string getLocalAddress() const;
    
    /**
     * @brief Get remote address
     */
    std::string getRemoteAddress() const;
    
    /**
     * @brief Get local port
     */
    int getLocalPort() const;
    
    /**
     * @brief Get remote port
     */
    int getRemotePort() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    TCPConfig config;
    State state = State::DISCONNECTED;
    TCPStats stats;
    int socket_fd = -1;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    std::string local_address;
    std::string remote_address;
    int local_port = 0;
    int remote_port = 0;
    
    // Async receive
    std::thread async_thread;
    std::atomic<bool> async_running;
    DataCallback data_callback;
    
    // Callbacks
    StateCallback on_state_change;
    ErrorCallback on_error;
    StatsCallback on_stats;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    bool createSocket();
    void closeSocket();
    bool setSocketOptions();
    bool connectSocket();
    void reconnectLoop();
    void receiveLoop();
    
    void setState(State new_state);
    void notifyStateChange();
    void notifyError(const std::string& error);
    void notifyStats();
    
    void updateStatsSent(size_t bytes);
    void updateStatsReceived(size_t bytes);
    void resetStatsImpl();
    
    // Helper functions
    bool isSocketValid() const;
    std::string getSocketError() const;
    void delayMilliseconds(int ms);
    std::string getCurrentTimeStr() const;
};

#endif // TCP_CLIENT_HPP
