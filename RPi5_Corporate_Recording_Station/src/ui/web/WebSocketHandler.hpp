#ifndef WEBSOCKET_HANDLER_HPP
#define WEBSOCKET_HANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <cstdint>

/**
 * @brief WebSocket Handler for Recording Station
 * 
 * Provides WebSocket connection management with:
 * - Connection handling
 * - Message broadcasting
 * - Client management
 * - Ping/Pong keepalive
 * - Reconnection support
 */
class WebSocketHandler {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CLOSING,
        RECONNECTING
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    struct Client {
        int id = -1;
        std::string address;
        State state = State::DISCONNECTED;
        std::chrono::system_clock::time_point connected_at;
        std::chrono::system_clock::time_point last_activity;
        std::string user_agent;
        std::map<std::string, std::string> metadata;
        bool authenticated = false;
        uint64_t messages_received = 0;
        uint64_t messages_sent = 0;
    };
    
    struct Message {
        int client_id = -1;
        std::string data;
        bool is_binary = false;
        bool is_text = true;
        std::chrono::system_clock::time_point timestamp;
        std::string type = "text";
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using MessageCallback = std::function<void(const Message&)>;
    using ConnectCallback = std::function<void(int client_id)>;
    using DisconnectCallback = std::function<void(int client_id)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    WebSocketHandler();
    ~WebSocketHandler();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize WebSocket handler
     * @param port WebSocket port
     * @param max_clients Maximum concurrent clients
     * @return true if successful
     */
    bool initialize(int port = 8081, int max_clients = 100);
    
    /**
     * @brief Shutdown WebSocket handler
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // SERVER CONTROL
    // ============================================
    
    /**
     * @brief Start WebSocket server
     * @return true if successful
     */
    bool start();
    
    /**
     * @brief Stop WebSocket server
     */
    void stop();
    
    /**
     * @brief Check if running
     */
    bool isRunning() const { return running; }
    
    // ============================================
    // CLIENT MANAGEMENT
    // ============================================
    
    /**
     * @brief Get connected clients
     */
    std::vector<Client> getClients() const;
    
    /**
     * @brief Get client by ID
     */
    Client getClient(int client_id) const;
    
    /**
     * @brief Get client count
     */
    size_t getClientCount() const;
    
    /**
     * @brief Disconnect client
     */
    bool disconnectClient(int client_id, const std::string& reason = "Closed by server");
    
    /**
     * @brief Disconnect all clients
     */
    void disconnectAll(const std::string& reason = "Server shutting down");
    
    /**
     * @brief Set client metadata
     */
    bool setClientMetadata(int client_id, const std::string& key, const std::string& value);
    
    /**
     * @brief Get client metadata
     */
    std::string getClientMetadata(int client_id, const std::string& key) const;
    
    // ============================================
    // MESSAGE HANDLING
    // ============================================
    
    /**
     * @brief Send message to client
     * @param client_id Client ID
     * @param message Message data
     * @return true if sent
     */
    bool sendMessage(int client_id, const std::string& message);
    
    /**
     * @brief Send binary message to client
     * @param client_id Client ID
     * @param data Binary data
     * @return true if sent
     */
    bool sendBinary(int client_id, const std::vector<uint8_t>& data);
    
    /**
     * @brief Broadcast message to all clients
     * @param message Message data
     * @return Number of clients sent to
     */
    size_t broadcast(const std::string& message);
    
    /**
     * @brief Broadcast to clients matching filter
     * @param message Message data
     * @param filter Function returning true for matching clients
     * @return Number of clients sent to
     */
    size_t broadcastTo(const std::string& message, std::function<bool(const Client&)> filter);
    
    /**
     * @brief Queue message for later delivery
     * @param client_id Client ID
     * @param message Message data
     * @return true if queued
     */
    bool queueMessage(int client_id, const std::string& message);
    
    /**
     * @brief Send queued messages
     * @param client_id Client ID
     * @return Number of messages sent
     */
    size_t flushQueue(int client_id);
    
    /**
     * @brief Clear message queue
     * @param client_id Client ID
     */
    void clearQueue(int client_id);
    
    // ============================================
    // STATISTICS
    // ============================================
    
    struct Stats {
        uint64_t total_connections = 0;
        uint64_t total_messages_received = 0;
        uint64_t total_messages_sent = 0;
        uint64_t total_bytes_received = 0;
        uint64_t total_bytes_sent = 0;
        uint64_t total_errors = 0;
        uint64_t current_connections = 0;
        uint64_t peak_connections = 0;
        std::chrono::system_clock::time_point start_time;
    };
    
    /**
     * @brief Get statistics
     */
    Stats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnMessage(MessageCallback callback);
    void setOnConnect(ConnectCallback callback);
    void setOnDisconnect(DisconnectCallback callback);
    void setOnError(ErrorCallback callback);
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Set maximum message size
     */
    void setMaxMessageSize(size_t size);
    
    /**
     * @brief Set ping interval
     */
    void setPingInterval(int seconds);
    
    /**
     * @brief Set timeout
     */
    void setTimeout(int seconds);
    
    /**
     * @brief Enable authentication
     */
    void setAuthEnabled(bool enable);
    
    /**
     * @brief Set authentication token
     */
    void setAuthToken(const std::string& token);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    bool initialized = false;
    bool running = false;
    int port = 8081;
    int max_clients = 100;
    int ping_interval = 30;
    int timeout = 60;
    size_t max_message_size = 1024 * 1024;
    bool auth_enabled = false;
    std::string auth_token;
    mutable std::mutex mutex;
    
    // Internal handle
    void* ws_handle = nullptr;
    
    // Clients
    std::map<int, Client> clients;
    std::map<int, std::queue<Message>> message_queues;
    std::atomic<int> next_client_id{0};
    mutable std::mutex client_mutex;
    mutable std::mutex queue_mutex;
    
    // Statistics
    Stats stats;
    mutable std::mutex stats_mutex;
    
    // Threads
    std::thread accept_thread;
    std::thread ping_thread;
    std::atomic<bool> running;
    
    // Callbacks
    MessageCallback on_message;
    ConnectCallback on_connect;
    DisconnectCallback on_disconnect;
    ErrorCallback on_error;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // Server implementation
    bool startServer();
    void stopServer();
    void acceptLoop();
    void pingLoop();
    
    // Client management
    int addClient(const std::string& address);
    void removeClient(int client_id);
    bool validateClient(int client_id) const;
    void updateClientActivity(int client_id);
    
    // Message handling
    void processMessage(int client_id, const std::string& data);
    void handleIncomingMessage(int client_id, const Message& msg);
    
    // Authentication
    bool authenticateClient(int client_id, const std::string& token);
    
    // Helper functions
    std::string getCurrentTimeStr() const;
    void notifyError(const std::string& error);
    void updateStatsMessageReceived(size_t bytes);
    void updateStatsMessageSent(size_t bytes);
    void updateStatsConnection();
};

#endif // WEBSOCKET_HANDLER_HPP
