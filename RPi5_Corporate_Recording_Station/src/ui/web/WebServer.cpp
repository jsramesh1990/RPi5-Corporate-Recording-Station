#include "WebServer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <chrono>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

// WebSocket implementation would use a library like libwebsockets or uWebSockets
// This is a simplified implementation for demonstration

WebServer::WebServer() 
    : server_handle(nullptr), websocket_handle(nullptr) {
}

WebServer::~WebServer() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool WebServer::initialize(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    config = cfg;
    
    // Validate configuration
    if (config.port <= 0 || config.port > 65535) {
        on_error("Invalid port: " + std::to_string(config.port));
        return false;
    }
    
    // Create static directory if it doesn't exist
    if (!config.static_dir.empty()) {
        std::string cmd = "mkdir -p " + config.static_dir;
        system(cmd.c_str());
    }
    
    initialized = true;
    std::cout << "Web Server initialized on port " << config.port << std::endl;
    return true;
}

bool WebServer::initialize(int port, const std::string& static_dir) {
    Config cfg;
    cfg.port = port;
    cfg.static_dir = static_dir.empty() ? "/var/www/recording-station" : static_dir;
    return initialize(cfg);
}

void WebServer::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    if (running) {
        stop();
    }
    
    initialized = false;
    std::cout << "Web Server shutdown" << std::endl;
}

// ============================================
// SERVER CONTROL
// ============================================

bool WebServer::start() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        on_error("Server not initialized");
        return false;
    }
    
    if (running) {
        return true;
    }
    
    if (!startServer()) {
        return false;
    }
    
    if (config.websocket_enabled) {
        if (!startWebSocketServer()) {
            std::cerr << "WebSocket server failed to start" << std::endl;
        }
    }
    
    running = true;
    stats.start_time = std::chrono::system_clock::now();
    
    std::cout << "Web Server started on " << getServerURL() << std::endl;
    if (config.websocket_enabled) {
        std::cout << "WebSocket server started on " << getWebSocketURL() << std::endl;
    }
    
    return true;
}

void WebServer::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!running) {
        return;
    }
    
    stopServer();
    stopWebSocketServer();
    
    running = false;
    std::cout << "Web Server stopped" << std::endl;
}

std::string WebServer::getServerURL() const {
    std::string protocol = config.ssl_enabled ? "https" : "http";
    std::string host = config.host;
    if (host == "0.0.0.0") {
        host = "localhost";
    }
    return protocol + "://" + host + ":" + std::to_string(config.port);
}

std::string WebServer::getWebSocketURL() const {
    std::string protocol = config.ssl_enabled ? "wss" : "ws";
    std::string host = config.host;
    if (host == "0.0.0.0") {
        host = "localhost";
    }
    return protocol + "://" + host + ":" + std::to_string(config.websocket_port);
}

// ============================================
// ROUTING
// ============================================

void WebServer::route(Method method, const std::string& path, RouteHandler handler) {
    std::lock_guard<std::mutex> lock(mutex);
    
    RouteEntry entry;
    entry.method = method;
    entry.path = path;
    entry.handler = handler;
    entry.is_websocket = false;
    
    // Convert path to regex pattern
    std::string pattern = path;
    std::regex param_regex(R"(\{([^}]+)\})");
    std::smatch match;
    while (std::regex_search(pattern, match, param_regex)) {
        pattern = pattern.replace(match.position(0), match.length(0), R"(([^/]+))");
    }
    entry.pattern = pattern;
    
    routes.push_back(entry);
    
    if (on_request) {
        std::cout << "Registered route: " << methodToString(method) << " " << path << std::endl;
    }
}

void WebServer::get(const std::string& path, RouteHandler handler) {
    route(Method::GET, path, handler);
}

void WebServer::post(const std::string& path, RouteHandler handler) {
    route(Method::POST, path, handler);
}

void WebServer::put(const std::string& path, RouteHandler handler) {
    route(Method::PUT, path, handler);
}

void WebServer::del(const std::string& path, RouteHandler handler) {
    route(Method::DELETE, path, handler);
}

void WebServer::patch(const std::string& path, RouteHandler handler) {
    route(Method::PATCH, path, handler);
}

void WebServer::options(const std::string& path, RouteHandler handler) {
    route(Method::OPTIONS, path, handler);
}

void WebServer::route(const std::string& path, RouteHandler handler) {
    // Register for all methods
    route(Method::GET, path, handler);
    route(Method::POST, path, handler);
    route(Method::PUT, path, handler);
    route(Method::DELETE, path, handler);
}

void WebServer::use(Middleware middleware) {
    std::lock_guard<std::mutex> lock(mutex);
    middlewares.push_back(middleware);
}

void WebServer::staticRoute(const std::string& path, const std::string& directory) {
    std::lock_guard<std::mutex> lock(mutex);
    static_routes[path] = directory;
}

// ============================================
// WEBSOCKET
// ============================================

void WebServer::websocket(const std::string& path, WebSocketHandler handler) {
    std::lock_guard<std::mutex> lock(mutex);
    ws_handler = handler;
    
    RouteEntry entry;
    entry.method = Method::GET;
    entry.path = path;
    entry.is_websocket = true;
    routes.push_back(entry);
}

void WebServer::setOnWebSocketConnect(WebSocketConnectHandler handler) {
    ws_connect_handler = handler;
}

void WebServer::setOnWebSocketDisconnect(WebSocketDisconnectHandler handler) {
    ws_disconnect_handler = handler;
}

bool WebServer::broadcastWebSocket(const std::string& message) {
    // Broadcast to all WebSocket clients
    if (websocket_handle) {
        // Implementation would depend on WebSocket library
        return true;
    }
    return false;
}

bool WebServer::sendWebSocket(int client_id, const std::string& message) {
    // Send to specific WebSocket client
    if (websocket_handle) {
        // Implementation would depend on WebSocket library
        return true;
    }
    return false;
}

size_t WebServer::getWebSocketClientCount() const {
    // Implementation would depend on WebSocket library
    return 0;
}

// ============================================
// API HELPERS
// ============================================

WebServer::Response WebServer::jsonResponse(const std::string& json, StatusCode status) {
    Response res;
    res.status = status;
    res.body = json;
    res.content_type = "application/json";
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = std::to_string(json.length());
    return res;
}

WebServer::Response WebServer::jsonError(const std::string& message, StatusCode status) {
    std::string json = "{\"error\": \"" + message + "\"}";
    return jsonResponse(json, status);
}

WebServer::Response WebServer::htmlResponse(const std::string& html, StatusCode status) {
    Response res;
    res.status = status;
    res.body = html;
    res.content_type = "text/html";
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = std::to_string(html.length());
    return res;
}

WebServer::Response WebServer::fileResponse(const std::string& path, const std::string& content_type) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return jsonError("File not found", StatusCode::NOT_FOUND);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    Response res;
    res.status = StatusCode::OK;
    res.body = content;
    res.content_type = content_type.empty() ? getContentType(path) : content_type;
    res.headers["Content-Type"] = res.content_type;
    res.headers["Content-Length"] = std::to_string(content.length());
    return res;
}

WebServer::Response WebServer::redirectResponse(const std::string& url, StatusCode status) {
    Response res;
    res.status = status;
    res.headers["Location"] = url;
    return res;
}

WebServer::Response WebServer::emptyResponse(StatusCode status) {
    Response res;
    res.status = status;
    return res;
}

std::string WebServer::parseJSON(const Request& req, const std::string& key,
                                const std::string& default_value) {
    if (req.body.empty()) {
        return default_value;
    }
    
    // Simple JSON parsing (production would use a proper JSON library)
    std::string pattern = "\"" + key + "\"\\s*:\\s*\"([^\"]*)\"";
    std::regex json_regex(pattern);
    std::smatch match;
    if (std::regex_search(req.body, match, json_regex)) {
        return match[1].str();
    }
    return default_value;
}

std::string WebServer::parseQuery(const Request& req, const std::string& key,
                                 const std::string& default_value) {
    auto it = req.query_params.find(key);
    if (it != req.query_params.end()) {
        return it->second;
    }
    return default_value;
}

// ============================================
// STATISTICS
// ============================================

WebServer::ServerStats WebServer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void WebServer::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = ServerStats{};
}

// ============================================
// CALLBACKS
// ============================================

void WebServer::setOnRequest(std::function<void(const Request&)> callback) {
    on_request = callback;
}

void WebServer::setOnResponse(std::function<void(const Response&)> callback) {
    on_response = callback;
}

void WebServer::setOnError(std::function<void(const std::string&)> callback) {
    on_error = callback;
}

// ============================================
// PRIVATE METHODS (SERVER IMPLEMENTATION)
// ============================================

bool WebServer::startServer() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        on_error("Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        on_error("Failed to set socket options: " + std::string(strerror(errno)));
        close(server_fd);
        return false;
    }
    
    // Bind socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = config.host == "0.0.0.0" ? INADDR_ANY : inet_addr(config.host.c_str());
    addr.sin_port = htons(config.port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        on_error("Failed to bind socket: " + std::string(strerror(errno)));
        close(server_fd);
        return false;
    }
    
    // Listen
    if (listen(server_fd, 128) < 0) {
        on_error("Failed to listen: " + std::string(strerror(errno)));
        close(server_fd);
        return false;
    }
    
    server_handle = (void*)(intptr_t)server_fd;
    
    // Start server thread
    std::thread server_thread(&WebServer::serverLoop, this);
    server_thread.detach();
    
    return true;
}

void WebServer::stopServer() {
    if (server_handle) {
        int server_fd = (int)(intptr_t)server_handle;
        close(server_fd);
        server_handle = nullptr;
    }
}

bool WebServer::startWebSocketServer() {
    // WebSocket server implementation would go here
    // This is a placeholder
    return true;
}

void WebServer::stopWebSocketServer() {
    // WebSocket server shutdown
    websocket_handle = nullptr;
}

void WebServer::serverLoop() {
    int server_fd = (int)(intptr_t)server_handle;
    
    while (running && server_fd >= 0) {
        struct pollfd pfd;
        pfd.fd = server_fd;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, 100);
        if (ret < 0) {
            break;
        }
        
        if (ret == 0) {
            continue;
        }
        
        if (pfd.revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd >= 0) {
                // Handle client request
                std::thread client_thread([this, client_fd, client_addr]() {
                    handleClientRequest(client_fd, client_addr);
                });
                client_thread.detach();
            }
        }
    }
}

void WebServer::handleClientRequest(int client_fd, struct sockaddr_in client_addr) {
    char buffer[8192];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_read] = '\0';
    std::string request_str(buffer);
    
    // Parse request
    Request req;
    req.client_ip = inet_ntoa(client_addr.sin_addr);
    
    // Parse request line
    std::istringstream iss(request_str);
    std::string method_str, path_str, version;
    iss >> method_str >> path_str >> version;
    
    req.method = stringToMethod(method_str);
    
    // Parse path and query string
    size_t query_pos = path_str.find('?');
    if (query_pos != std::string::npos) {
        req.path = path_str.substr(0, query_pos);
        req.query_params = parseQueryString(path_str.substr(query_pos + 1));
    } else {
        req.path = path_str;
    }
    
    // Parse headers
    std::string line;
    while (std::getline(iss, line) && line != "\r") {
        if (line.empty()) continue;
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            req.headers[key] = value;
        }
    }
    
    // Parse body
    auto content_length_it = req.headers.find("Content-Length");
    if (content_length_it != req.headers.end()) {
        size_t content_length = std::stoul(content_length_it->second);
        if (content_length > 0) {
            // Read body
            std::vector<char> body_buffer(content_length);
            size_t total_read = 0;
            while (total_read < content_length) {
                ssize_t read = recv(client_fd, body_buffer.data() + total_read, 
                                   content_length - total_read, 0);
                if (read <= 0) break;
                total_read += read;
            }
            req.body = std::string(body_buffer.data(), total_read);
        }
    }
    
    // Process request
    Response res;
    try {
        res = handleRoute(req);
    } catch (const std::exception& e) {
        res = jsonError("Internal server error: " + std::string(e.what()), 
                       StatusCode::INTERNAL_ERROR);
    }
    
    // Send response
    std::string response_str = buildResponse(res);
    send(client_fd, response_str.c_str(), response_str.length(), 0);
    
    // Update stats
    updateStats(req, res);
    
    close(client_fd);
}

WebServer::Response WebServer::handleRoute(const Request& req) {
    // Check for WebSocket upgrade
    auto upgrade_it = req.headers.find("Upgrade");
    if (upgrade_it != req.headers.end() && upgrade_it->second == "websocket") {
        // Handle WebSocket upgrade
        if (ws_handler) {
            // WebSocket connection would be established here
            // This is a placeholder
            Response res;
            res.status = StatusCode::SWITCHING_PROTOCOLS;
            res.headers["Upgrade"] = "websocket";
            res.headers["Connection"] = "Upgrade";
            return res;
        }
    }
    
    // Check static routes
    for (const auto& [prefix, dir] : static_routes) {
        if (req.path.find(prefix) == 0) {
            std::string file_path = dir + req.path.substr(prefix.length());
            if (file_path.back() == '/') {
                file_path += "index.html";
            }
            return fileResponse(file_path);
        }
    }
    
    // Check dynamic routes
    for (const auto& route : routes) {
        if (matchesRoute(route, req)) {
            // Extract path parameters
            req.path_params = extractPathParams(route.pattern, req.path);
            
            // Apply middlewares
            return processMiddlewares(req, route.handler);
        }
    }
    
    // Not found
    return jsonError("Not found", StatusCode::NOT_FOUND);
}

bool WebServer::matchesRoute(const RouteEntry& route, const Request& req) {
    // Check method
    if (route.method != req.method && route.method != Method::GET) {
        return false;
    }
    
    // Check path pattern
    try {
        std::regex pattern_regex(route.pattern);
        return std::regex_match(req.path, pattern_regex);
    } catch (const std::exception&) {
        return req.path == route.path;
    }
}

std::map<std::string, std::string> WebServer::extractPathParams(const std::string& pattern, 
                                                               const std::string& path) {
    std::map<std::string, std::string> params;
    
    try {
        std::regex param_regex(R"(\{([^}]+)\})");
        std::string pattern_copy = pattern;
        std::smatch match;
        
        std::vector<std::string> param_names;
        std::string search_str = pattern_copy;
        while (std::regex_search(search_str, match, param_regex)) {
            param_names.push_back(match[1].str());
            search_str = match.suffix().str();
        }
        
        // Convert pattern to regex and extract values
        std::regex path_regex(pattern);
        std::smatch path_match;
        if (std::regex_match(path, path_match, path_regex)) {
            for (size_t i = 0; i < param_names.size() && i < path_match.size() - 1; i++) {
                params[param_names[i]] = path_match[i + 1].str();
            }
        }
    } catch (const std::exception&) {
        // Fallback: simple matching
        std::vector<std::string> path_parts;
        std::stringstream ss(pattern);
        std::string part;
        while (std::getline(ss, part, '/')) {
            if (part.find('{') == 0 && part.find('}') == part.length() - 1) {
                std::string param_name = part.substr(1, part.length() - 2);
                // Extract from path
                // Simplified implementation
            }
        }
    }
    
    return params;
}

WebServer::Response WebServer::processMiddlewares(const Request& req, RouteHandler handler) {
    if (middlewares.empty()) {
        return handler(req);
    }
    
    // Process middlewares in reverse order
    RouteHandler current_handler = handler;
    for (auto it = middlewares.rbegin(); it != middlewares.rend(); ++it) {
        current_handler = [it, current_handler](const Request& req) {
            return (*it)(req, current_handler);
        };
    }
    
    return current_handler(req);
}

std::string WebServer::buildResponse(const Response& res) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << static_cast<int>(res.status) << " " 
       << statusCodeToString(res.status) << "\r\n";
    
    for (const auto& [key, value] : res.headers) {
        ss << key << ": " << value << "\r\n";
    }
    
    if (!res.body.empty()) {
        ss << "Content-Length: " << res.body.length() << "\r\n";
    }
    
    ss << "\r\n";
    ss << res.body;
    
    return ss.str();
}

// ============================================
// HELPER FUNCTIONS
// ============================================

std::string WebServer::methodToString(Method method) const {
    switch (method) {
        case Method::GET: return "GET";
        case Method::POST: return "POST";
        case Method::PUT: return "PUT";
        case Method::DELETE: return "DELETE";
        case Method::PATCH: return "PATCH";
        case Method::OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

WebServer::Method WebServer::stringToMethod(const std::string& str) const {
    if (str == "GET") return Method::GET;
    if (str == "POST") return Method::POST;
    if (str == "PUT") return Method::PUT;
    if (str == "DELETE") return Method::DELETE;
    if (str == "PATCH") return Method::PATCH;
    if (str == "OPTIONS") return Method::OPTIONS;
    return Method::GET;
}

std::string WebServer::statusCodeToString(StatusCode code) const {
    switch (code) {
        case StatusCode::OK: return "OK";
        case StatusCode::CREATED: return "Created";
        case StatusCode::ACCEPTED: return "Accepted";
        case StatusCode::NO_CONTENT: return "No Content";
        case StatusCode::BAD_REQUEST: return "Bad Request";
        case StatusCode::UNAUTHORIZED: return "Unauthorized";
        case StatusCode::FORBIDDEN: return "Forbidden";
        case StatusCode::NOT_FOUND: return "Not Found";
        case StatusCode::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case StatusCode::CONFLICT: return "Conflict";
        case StatusCode::TOO_MANY_REQUESTS: return "Too Many Requests";
        case StatusCode::INTERNAL_ERROR: return "Internal Server Error";
        case StatusCode::NOT_IMPLEMENTED: return "Not Implemented";
        case StatusCode::SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

std::string WebServer::urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            char hex[3] = {str[i+1], str[i+2], '\0'};
            int val = strtol(hex, nullptr, 16);
            result += static_cast<char>(val);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string WebServer::urlEncode(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            result += hex;
        }
    }
    return result;
}

std::map<std::string, std::string> WebServer::parseQueryString(const std::string& query) {
    std::map<std::string, std::string> params;
    std::stringstream ss(query);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, eq_pos));
            std::string value = urlDecode(pair.substr(eq_pos + 1));
            params[key] = value;
        }
    }
    return params;
}

std::string WebServer::getContentType(const std::string& path) {
    static const std::map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".txt", "text/plain"},
        {".xml", "application/xml"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"},
        {".mp4", "video/mp4"},
        {".webm", "video/webm"},
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".ogg", "audio/ogg"}
    };
    
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string ext = path.substr(dot_pos);
        auto it = mime_types.find(ext);
        if (it != mime_types.end()) {
            return it->second;
        }
    }
    return "application/octet-stream";
}

void WebServer::updateStats(const Request& req, const Response& res) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.total_requests++;
    stats.bytes_received += req.body.length();
    stats.bytes_sent += res.body.length();
    
    std::string route_key = req.path;
    stats.route_hits[route_key]++;
}

std::string WebServer::getCurrentTimeStr() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}

// WebSocket handlers (simplified)
void WebServer::onWebSocketMessage(int client_id, const std::string& message) {
    if (ws_handler) {
        WebSocketMessage msg;
        msg.data = message;
        msg.is_text = true;
        ws_handler(client_id, msg);
    }
}

void WebServer::onWebSocketOpen(int client_id) {
    if (ws_connect_handler) {
        ws_connect_handler(client_id);
    }
}

void WebServer::onWebSocketClose(int client_id) {
    if (ws_disconnect_handler) {
        ws_disconnect_handler(client_id);
    }
}
