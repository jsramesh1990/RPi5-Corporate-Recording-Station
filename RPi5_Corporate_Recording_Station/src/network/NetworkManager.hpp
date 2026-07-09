#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>

/**
 * @brief Network Manager for Raspberry Pi 5
 * 
 * Provides network management with support for Ethernet, WiFi,
 * and network monitoring. Handles IP configuration, connection
 * status, and network statistics.
 */
class NetworkManager {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class NetworkType {
        ETHERNET,
        WIFI,
        LOOPBACK,
        UNKNOWN
    };
    
    enum class ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        FAILED,
        UNKNOWN
    };
    
    enum class IPType {
        DHCP,
        STATIC,
        AUTO
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief Network interface information
     */
    struct InterfaceInfo {
        std::string name;
        std::string mac_address;
        NetworkType type = NetworkType::UNKNOWN;
        ConnectionState state = ConnectionState::UNKNOWN;
        bool is_up = false;
        bool has_ip = false;
        std::string ip_address;
        std::string netmask;
        std::string gateway;
        std::string broadcast;
        std::vector<std::string> dns_servers;
        uint32_t mtu = 1500;
        uint64_t speed_mbps = 0;
        bool is_default = false;
        std::string driver;
        std::string hardware;
    };
    
    /**
     * @brief Network statistics
     */
    struct NetworkStats {
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t packets_sent = 0;
        uint64_t packets_received = 0;
        uint64_t errors_sent = 0;
        uint64_t errors_received = 0;
        uint64_t dropped_sent = 0;
        uint64_t dropped_received = 0;
        uint64_t collisions = 0;
        double send_rate_mbps = 0;
        double receive_rate_mbps = 0;
        std::chrono::system_clock::time_point last_update;
    };
    
    /**
     * @brief WiFi configuration
     */
    struct WiFiConfig {
        std::string ssid;
        std::string password;
        std::string security = "wpa2";
        bool hidden = false;
        int priority = 0;
    };
    
    /**
     * @brief Network configuration
     */
    struct NetworkConfig {
        std::string interface = "eth0";
        IPType ip_type = IPType::DHCP;
        std::string ip_address;
        std::string netmask = "255.255.255.0";
        std::string gateway;
        std::vector<std::string> dns_servers = {"8.8.8.8", "1.1.1.1"};
        std::string hostname;
        bool auto_connect = true;
        int timeout_seconds = 30;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using StateCallback = std::function<void(const std::string&, ConnectionState)>;
    using StatsCallback = std::function<void(const std::string&, const NetworkStats&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using WiFiScanCallback = std::function<void(const std::vector<WiFiConfig>&)>;
    
    // ============================================
    // SINGLETON
    // ============================================
    
    static NetworkManager& getInstance();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize network manager
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Shutdown network manager
     */
    void shutdown();
    
    // ============================================
    // INTERFACE MANAGEMENT
    // ============================================
    
    /**
     * @brief Get all network interfaces
     */
    std::vector<InterfaceInfo> getInterfaces() const;
    
    /**
     * @brief Get interface by name
     */
    InterfaceInfo getInterface(const std::string& name) const;
    
    /**
     * @brief Get default interface
     */
    InterfaceInfo getDefaultInterface() const;
    
    /**
     * @brief Bring interface up
     */
    bool interfaceUp(const std::string& interface);
    
    /**
     * @brief Bring interface down
     */
    bool interfaceDown(const std::string& interface);
    
    /**
     * @brief Check if interface is up
     */
    bool isInterfaceUp(const std::string& interface) const;
    
    /**
     * @brief Set interface MTU
     */
    bool setMTU(const std::string& interface, uint32_t mtu);
    
    // ============================================
    // IP CONFIGURATION
    // ============================================
    
    /**
     * @brief Configure interface with DHCP
     */
    bool configureDHCP(const std::string& interface);
    
    /**
     * @brief Configure interface with static IP
     */
    bool configureStatic(const std::string& interface, 
                        const std::string& ip,
                        const std::string& netmask = "255.255.255.0",
                        const std::string& gateway = "");
    
    /**
     * @brief Set DNS servers
     */
    bool setDNSServers(const std::string& interface, 
                      const std::vector<std::string>& dns_servers);
    
    /**
     * @brief Get DNS servers
     */
    std::vector<std::string> getDNSServers() const;
    
    /**
     * @brief Set hostname
     */
    bool setHostname(const std::string& hostname);
    
    /**
     * @brief Get hostname
     */
    std::string getHostname() const;
    
    // ============================================
    // WIFI MANAGEMENT
    // ============================================
    
    /**
     * @brief Scan for WiFi networks
     */
    std::vector<WiFiConfig> scanWiFi();
    
    /**
     * @brief Connect to WiFi network
     */
    bool connectWiFi(const WiFiConfig& config);
    
    /**
     * @brief Disconnect from WiFi
     */
    bool disconnectWiFi();
    
    /**
     * @brief Check if WiFi is connected
     */
    bool isWiFiConnected() const;
    
    /**
     * @brief Get connected WiFi SSID
     */
    std::string getWiFiSSID() const;
    
    /**
     * @brief Get WiFi signal strength
     */
    int getWiFiSignalStrength() const;
    
    /**
     * @brief Forget WiFi network
     */
    bool forgetWiFi(const std::string& ssid);
    
    // ============================================
    // CONNECTION MANAGEMENT
    // ============================================
    
    /**
     * @brief Check internet connectivity
     */
    bool checkInternetConnectivity(const std::string& host = "8.8.8.8", int timeout_ms = 3000);
    
    /**
     * @brief Ping host
     */
    bool ping(const std::string& host, int count = 4, int timeout_ms = 1000);
    
    /**
     * @brief Get connection quality
     */
    struct ConnectionQuality {
        int latency_ms = 0;
        float packet_loss = 0;
        float jitter_ms = 0;
        float quality_percent = 100;
        std::string status;
    };
    
    ConnectionQuality getConnectionQuality() const;
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get statistics for interface
     */
    NetworkStats getStats(const std::string& interface) const;
    
    /**
     * @brief Get total statistics
     */
    NetworkStats getTotalStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats(const std::string& interface);
    
    /**
     * @brief Reset all statistics
     */
    void resetAllStats();
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnStateChange(StateCallback callback);
    void setOnStats(StatsCallback callback);
    void setOnError(ErrorCallback callback);
    void setOnWiFiScan(WiFiScanCallback callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get network type name
     */
    std::string getNetworkTypeName(NetworkType type) const;
    
    /**
     * @brief Get connection state name
     */
    std::string getConnectionStateName(ConnectionState state) const;
    
    /**
     * @brief Get IP type name
     */
    std::string getIPTypeName(IPType type) const;
    
    /**
     * @brief Validate IP address
     */
    bool isValidIP(const std::string& ip) const;
    
    /**
     * @brief Validate MAC address
     */
    bool isValidMAC(const std::string& mac) const;
    
    /**
     * @brief Get local IP address
     */
    std::string getLocalIP() const;
    
    /**
     * @brief Get public IP address
     */
    std::string getPublicIP() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    NetworkManager();
    ~NetworkManager();
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    std::map<std::string, InterfaceInfo> interfaces;
    std::map<std::string, NetworkStats> interface_stats;
    std::map<std::string, std::vector<std::string>> dns_cache;
    
    // WiFi state
    WiFiConfig current_wifi;
    bool wifi_connected = false;
    std::string wifi_ssid;
    int wifi_signal = 0;
    
    // Monitoring thread
    std::thread monitor_thread;
    std::atomic<bool> monitoring;
    int monitor_interval = 5;
    
    // Callbacks
    StateCallback on_state_change;
    StatsCallback on_stats;
    ErrorCallback on_error;
    WiFiScanCallback on_wifi_scan;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    void scanInterfaces();
    void updateInterface(const std::string& name);
    void updateStats();
    void monitorLoop();
    
    bool readSysfs(const std::string& path, std::string& content) const;
    bool writeSysfs(const std::string& path, const std::string& content) const;
    bool executeCommand(const std::string& cmd, std::string& output) const;
    bool fileExists(const std::string& path) const;
    std::vector<std::string> getNetworkInterfaces() const;
    std::string getMacAddress(const std::string& interface) const;
    NetworkType getInterfaceType(const std::string& interface) const;
    bool isWifiInterface(const std::string& interface) const;
    bool isEthernetInterface(const std::string& interface) const;
    
    void notifyStateChange(const std::string& interface, ConnectionState state);
    void notifyStats(const std::string& interface, const NetworkStats& stats);
    void notifyError(const std::string& error);
    void notifyWiFiScan(const std::vector<WiFiConfig>& networks);
    
    // Linux-specific helpers
    int getInterfaceIndex(const std::string& name) const;
    bool setInterfaceFlag(const std::string& name, int flag, bool enable);
    bool getInterfaceFlag(const std::string& name, int flag) const;
    std::string getInterfaceIP(const std::string& name) const;
    std::string getInterfaceNetmask(const std::string& name) const;
    std::string getInterfaceGateway(const std::string& name) const;
    uint32_t getInterfaceSpeed(const std::string& name) const;
    
    void delayMicroseconds(int us);
    std::string getCurrentTimeStr() const;
};

#endif // NETWORK_MANAGER_HPP
