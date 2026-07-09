#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdint>

/**
 * @brief Web Server for Recording Station
 * 
 * Provides a web-based interface with:
 * - REST API endpoints
 * - WebSocket support
 * - Static file serving
 * - Authentication
 * - CORS support
 * - Real-time data streaming
 */
class WebServer {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Method {
        GET,
        POST,
        PUT,
        DELETE,
        PATCH,
        OPTIONS
    };
    
    enum class StatusCode {
        OK = 200,
        CREATED = 201,
        ACCEPTED = 202,
        NO_CONTENT = 204,
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        METHOD_NOT_ALLOWED = 405,
        CONFLICT = 409,
        TOO_MANY_REQUESTS = 429,
        INTERNAL_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        SERVICE_UNAVAILABLE = 503
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief HTTP request
     */
    struct Request {
        Method method = Method::GET;
        std::string path;
        std::string body;
        std::map<std::string, std::string> headers;
        std::map<std::string, std::string> query_params;
        std::map<std::string, std::string> path_params;
        std::string client_ip;
        std::string user_agent;
        std::string content_type;
        size_t content_length = 0;
        std::vector<uint8_t> raw_body;
    };
    
    /**
     * @brief HTTP response
     */
    struct Response {
        StatusCode status = StatusCode::OK;
        std::string body;
        std::map<std::string, std::string> headers;
        std::string content_type = "application/json";
        std::vector<uint8_t> raw_body;
        bool compression = false;
        int compression_level = 6;
    };
    
    /**
     * @brief WebSocket message
     */
    struct WebSocketMessage {
        std::string data;
        bool is_text = true;
        bool is_binary = false;
        bool is_close = false;
        uint16_t close_code = 1000;
        std::string close_reason;
    };
    
    /**
     * @brief Route handler
     */
    using RouteHandler = std::function<Response(const Request&)>;
    
    /**
     * @brief WebSocket handler
     */
    using WebSocketHandler = std::function<void(int client_id, const WebSocketMessage&)>;
    
    /**
     * @brief WebSocket connection handler
     */
    using WebSocketConnectHandler = std::function<void(int client_id)>;
    
    /**
     * @brief WebSocket disconnect handler
     */
    using WebSocketDisconnectHandler = std::function<void(int client_id)>;
    
    /**
     * @brief Middleware
     */
    using Middleware = std::function<Response(const Request&, RouteHandler)>;
    
    /**
     * @brief Server configuration
     */
    struct Config {
        std::string host = "0.0.0.0";
        int port = 8080;
        int websocket_port = 8081;
        std::string static_dir = "/var/www/recording-station";
        std::string index_file = "index.html";
        bool ssl_enabled = false;
        std::string ssl_cert = "/etc/ssl/certs/server.crt";
        std::string ssl_key = "/etc/ssl/private/server.key";
        bool cors_enabled = true;
        std::string cors_origins = "*";
        bool auth_enabled = false;
        std::string auth_token = "";
        int max_body_size = 10 * 1024 * 1024;  // 10 MB
        int timeout_seconds = 30;
        bool access_log = true;
        bool error_log = true;
        int thread_pool_size = 4;
        bool websocket_enabled = true;
        int websocket_max_message_size = 1024 * 1024;  // 1 MB
        int websocket_ping_interval = 30;  // seconds
    };
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    WebServer();
    ~WebServer();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize web server
     * @param config Server configuration
     * @return true if successful
     */
    bool initialize(const Config& config = Config());
    
    /**
     * @brief Initialize with parameters
     * @param port Server port
     * @param static_dir Static files directory
     * @return true if successful
     */
    bool initialize(int port, const std::string& static_dir = "");
    
    /**
     * @brief Shutdown web server
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
     * @brief Start server
     * @return true if successful
     */
    bool start();
    
    /**
     * @brief Stop server
     */
    void stop();
    
    /**
     * @brief Check if running
     */
    bool isRunning() const { return running; }
    
    /**
     * @brief Get server URL
     */
    std::string getServerURL() const;
    
    /**
     * @brief Get WebSocket URL
     */
    std::string getWebSocketURL() const;
    
    // ============================================
    // ROUTING
    // ============================================
    
    /**
     * @brief Register GET route
     */
    void route(Method method, const std::string& path, RouteHandler handler);
    void get(const std::string& path, RouteHandler handler);
    void post(const std::string& path, RouteHandler handler);
    void put(const std::string& path, RouteHandler handler);
    void del(const std::string& path, RouteHandler handler);
    void patch(const std::string& path, RouteHandler handler);
    void options(const std::string& path, RouteHandler handler);
    
    /**
     * @brief Register route with any method
     */
    void route(const std::string& path, RouteHandler handler);
    
    /**
     * @brief Register middleware
     */
    void use(Middleware middleware);
    
    /**
     * @brief Register static route
     */
    void staticRoute(const std::string& path, const std::string& directory);
    
    // ============================================
    // WEBSOCKET
    // ============================================
    
    /**
     * @brief Register WebSocket route
     */
    void websocket(const std::string& path, WebSocketHandler handler);
    
    /**
     * @brief Set WebSocket connect handler
     */
    void setOnWebSocketConnect(WebSocketConnectHandler handler);
    
    /**
     * @brief Set WebSocket disconnect handler
     */
    void setOnWebSocketDisconnect(WebSocketDisconnectHandler handler);
    
    /**
     * @brief Broadcast WebSocket message
     */
    bool broadcastWebSocket(const std::string& message);
    
    /**
     * @brief Send WebSocket message to client
     */
    bool sendWebSocket(int client_id, const std::string& message);
    
    /**
     * @brief Get WebSocket client count
     */
    size_t getWebSocketClientCount() const;
    
    // ============================================
    // API HELPERS
    // ============================================
    
    /**
     * @brief JSON response helper
     */
    static Response jsonResponse(const std::string& json, StatusCode status = StatusCode::OK);
    
    /**
     * @brief JSON error response
     */
    static Response jsonError(const std::string& message, StatusCode status = StatusCode::BAD_REQUEST);
    
    /**
     * @brief HTML response
     */
    static Response htmlResponse(const std::string& html, StatusCode status = StatusCode::OK);
    
    /**
     * @brief File response
     */
    static Response fileResponse(const std::string& path, const std::string& content_type = "");
    
    /**
     * @brief Redirect response
     */
    static Response redirectResponse(const std::string& url, StatusCode status = StatusCode::FOUND);
    
    /**
     * @brief Empty response
     */
    static Response emptyResponse(StatusCode status = StatusCode::NO_CONTENT);
    
    /**
     * @brief Parse JSON from request
     */
    static std::string parseJSON(const Request& req, const std::string& key, 
                                 const std::string& default_value = "");
    
    /**
     * @brief Parse query parameter
     */
    static std::string parseQuery(const Request& req, const std::string& key,
                                  const std::string& default_value = "");
    
    // ============================================
    // STATISTICS
    // ============================================
    
    struct ServerStats {
        uint64_t total_requests = 0;
        uint64_t total_responses = 0;
        uint64_t total_websocket_messages = 0;
        uint64_t active_connections = 0;
        uint64_t active_websocket_connections = 0;
        uint64_t bytes_received = 0;
        uint64_t bytes_sent = 0;
        std::map<std::string, uint64_t> route_hits;
        std::chrono::system_clock::time_point start_time;
    };
    
    /**
     * @brief Get server statistics
     */
    ServerStats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnRequest(std::function<void(const Request&)> callback);
    void setOnResponse(std::function<void(const Response&)> callback);
    void setOnError(std::function<void(const std::string&)> callback);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    bool initialized = false;
    bool running = false;
    Config config;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    // Internal server handle (opaque)
    void* server_handle = nullptr;
    void* websocket_handle = nullptr;
    
    // Route registry
    struct RouteEntry {
        Method method;
        std::string path;
        RouteHandler handler;
        std::string pattern;
        bool is_websocket = false;
    };
    std::vector<RouteEntry> routes;
    std::vector<Middleware> middlewares;
    std::map<std::string, std::string> static_routes;
    
    // WebSocket handlers
    WebSocketHandler ws_handler;
    WebSocketConnectHandler ws_connect_handler;
    WebSocketDisconnectHandler ws_disconnect_handler;
    
    // Server statistics
    ServerStats stats;
    mutable std::mutex stats_mutex;
    
    // Callbacks
    std::function<void(const Request&)> on_request;
    std::function<void(const Response&)> on_response;
    std::function<void(const std::string&)> on_error;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // Server implementation
    bool startServer();
    void stopServer();
    bool startWebSocketServer();
    void stopWebSocketServer();
    void serverLoop();
    void handleRequest(const Request& req, Response& res);
    Response handleRoute(const Request& req);
    bool matchesRoute(const RouteEntry& route, const Request& req);
    std::map<std::string, std::string> extractPathParams(const std::string& pattern, 
                                                        const std::string& path);
    bool isStaticRoute(const std::string& path, std::string& file_path);
    std::string getContentType(const std::string& path);
    void updateStats(const Request& req, const Response& res);
    
    // Middleware processing
    Response processMiddlewares(const Request& req, RouteHandler handler);
    
    // WebSocket
    void onWebSocketMessage(int client_id, const std::string& message);
    void onWebSocketOpen(int client_id);
    void onWebSocketClose(int client_id);
    
    // Helper functions
    std::string methodToString(Method method) const;
    Method stringToMethod(const std::string& str) const;
    std::string statusCodeToString(StatusCode code) const;
    std::string urlDecode(const std::string& str);
    std::string urlEncode(const std::string& str);
    std::map<std::string, std::string> parseQueryString(const std::string& query);
    std::string getCurrentTimeStr() const;
};

#endif // WEB_SERVER_HPP
