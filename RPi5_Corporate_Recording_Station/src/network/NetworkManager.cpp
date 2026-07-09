#include "NetworkManager.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <regex>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/rtnetlink.h>

NetworkManager& NetworkManager::getInstance() {
    static NetworkManager instance;
    return instance;
}

NetworkManager::NetworkManager() 
    : monitoring(false), monitor_interval(5), wifi_connected(false), wifi_signal(0) {
}

NetworkManager::~NetworkManager() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool NetworkManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    // Scan interfaces
    scanInterfaces();
    
    // Start monitoring
    monitoring = true;
    monitor_thread = std::thread(&NetworkManager::monitorLoop, this);
    
    initialized = true;
    std::cout << "Network Manager initialized" << std::endl;
    return true;
}

void NetworkManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    monitoring = false;
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    initialized = false;
    std::cout << "Network Manager shutdown" << std::endl;
}

// ============================================
// INTERFACE MANAGEMENT
// ============================================

std::vector<NetworkManager::InterfaceInfo> NetworkManager::getInterfaces() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<InterfaceInfo> result;
    for (const auto& [name, info] : interfaces) {
        result.push_back(info);
    }
    return result;
}

NetworkManager::InterfaceInfo NetworkManager::getInterface(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = interfaces.find(name);
    if (it != interfaces.end()) {
        return it->second;
    }
    return InterfaceInfo{};
}

NetworkManager::InterfaceInfo NetworkManager::getDefaultInterface() const {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& [name, info] : interfaces) {
        if (info.is_default && info.is_up) {
            return info;
        }
    }
    return InterfaceInfo{};
}

bool NetworkManager::interfaceUp(const std::string& interface) {
    return setInterfaceFlag(interface, IFF_UP, true);
}

bool NetworkManager::interfaceDown(const std::string& interface) {
    return setInterfaceFlag(interface, IFF_UP, false);
}

bool NetworkManager::isInterfaceUp(const std::string& interface) const {
    return getInterfaceFlag(interface, IFF_UP);
}

bool NetworkManager::setMTU(const std::string& interface, uint32_t mtu) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    ifr.ifr_mtu = mtu;
    
    int result = ioctl(sock, SIOCSIFMTU, &ifr);
    close(sock);
    
    if (result == 0) {
        auto it = interfaces.find(interface);
        if (it != interfaces.end()) {
            it->second.mtu = mtu;
        }
    }
    
    return result == 0;
}

// ============================================
// IP CONFIGURATION
// ============================================

bool NetworkManager::configureDHCP(const std::string& interface) {
    std::string output;
    std::string cmd = "dhclient -v " + interface + " 2>&1";
    
    if (!executeCommand(cmd, output)) {
        notifyError("DHCP configuration failed for " + interface);
        return false;
    }
    
    // Update interface info
    updateInterface(interface);
    return true;
}

bool NetworkManager::configureStatic(const std::string& interface,
                                    const std::string& ip,
                                    const std::string& netmask,
                                    const std::string& gateway) {
    // Validate IP
    if (!isValidIP(ip)) {
        notifyError("Invalid IP address: " + ip);
        return false;
    }
    
    // Set IP address
    std::string cmd = "ip addr add " + ip + "/" + netmask + " dev " + interface;
    std::string output;
    executeCommand(cmd, output);
    
    // Set gateway if provided
    if (!gateway.empty() && isValidIP(gateway)) {
        cmd = "ip route add default via " + gateway + " dev " + interface;
        executeCommand(cmd, output);
    }
    
    // Bring interface up
    interfaceUp(interface);
    
    // Update interface info
    updateInterface(interface);
    return true;
}

bool NetworkManager::setDNSServers(const std::string& interface,
                                  const std::vector<std::string>& dns_servers) {
    // Update resolv.conf
    std::ofstream resolv("/etc/resolv.conf");
    if (!resolv.is_open()) {
        return false;
    }
    
    resolv << "# Generated by Recording Station\n";
    resolv << "nameserver " << dns_servers[0] << "\n";
    if (dns_servers.size() > 1) {
        resolv << "nameserver " << dns_servers[1] << "\n";
    }
    resolv.close();
    
    dns_cache[interface] = dns_servers;
    return true;
}

std::vector<std::string> NetworkManager::getDNSServers() const {
    std::vector<std::string> servers;
    std::ifstream resolv("/etc/resolv.conf");
    if (!resolv.is_open()) {
        return servers;
    }
    
    std::string line;
    while (std::getline(resolv, line)) {
        if (line.find("nameserver") == 0) {
            std::string ip = line.substr(10);
            ip.erase(0, ip.find_first_not_of(" \t"));
            ip.erase(ip.find_last_not_of(" \t") + 1);
            if (!ip.empty() && isValidIP(ip)) {
                servers.push_back(ip);
            }
        }
    }
    resolv.close();
    
    return servers;
}

bool NetworkManager::setHostname(const std::string& hostname) {
    if (sethostname(hostname.c_str(), hostname.length()) < 0) {
        return false;
    }
    return true;
}

std::string NetworkManager::getHostname() const {
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) < 0) {
        return "localhost";
    }
    return std::string(buffer);
}

// ============================================
// WIFI MANAGEMENT
// ============================================

std::vector<NetworkManager::WiFiConfig> NetworkManager::scanWiFi() {
    std::vector<WiFiConfig> networks;
    std::string output;
    
    // Use iwlist for WiFi scanning
    if (!executeCommand("iwlist wlan0 scan 2>/dev/null", output)) {
        return networks;
    }
    
    std::istringstream iss(output);
    std::string line;
    WiFiConfig current;
    bool in_network = false;
    
    while (std::getline(iss, line)) {
        if (line.find("Cell") != std::string::npos) {
            if (in_network && !current.ssid.empty()) {
                networks.push_back(current);
            }
            current = WiFiConfig{};
            in_network = true;
        }
        
        if (line.find("ESSID:") != std::string::npos) {
            size_t start = line.find('"') + 1;
            size_t end = line.rfind('"');
            if (start != std::string::npos && end != std::string::npos) {
                current.ssid = line.substr(start, end - start);
            }
        }
        
        if (line.find("Encryption key:") != std::string::npos) {
            if (line.find("on") != std::string::npos) {
                current.security = "wpa2";
            } else {
                current.security = "none";
            }
        }
        
        if (line.find("Quality=") != std::string::npos) {
            // Parse quality/signal strength
        }
    }
    
    if (in_network && !current.ssid.empty()) {
        networks.push_back(current);
    }
    
    notifyWiFiScan(networks);
    return networks;
}

bool NetworkManager::connectWiFi(const WiFiConfig& config) {
    // Use wpa_supplicant for WiFi connection
    std::string wpa_config = "/etc/wpa_supplicant/wpa_supplicant.conf";
    std::ofstream wpa_file(wpa_config, std::ios::app);
    if (!wpa_file.is_open()) {
        return false;
    }
    
    wpa_file << "\nnetwork={\n";
    wpa_file << "    ssid=\"" << config.ssid << "\"\n";
    if (!config.password.empty()) {
        wpa_file << "    psk=\"" << config.password << "\"\n";
    }
    if (config.hidden) {
        wpa_file << "    scan_ssid=1\n";
    }
    if (config.priority > 0) {
        wpa_file << "    priority=" << config.priority << "\n";
    }
    wpa_file << "}\n";
    wpa_file.close();
    
    // Reload wpa_supplicant
    std::string output;
    executeCommand("wpa_cli -i wlan0 reconfigure", output);
    executeCommand("wpa_cli -i wlan0 reassociate", output);
    
    // Wait for connection
    for (int i = 0; i < 30; i++) {
        if (isWiFiConnected()) {
            current_wifi = config;
            wifi_connected = true;
            wifi_ssid = config.ssid;
            notifyStateChange("wlan0", ConnectionState::CONNECTED);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    notifyStateChange("wlan0", ConnectionState::FAILED);
    return false;
}

bool NetworkManager::disconnectWiFi() {
    std::string output;
    if (executeCommand("wpa_cli -i wlan0 disconnect", output)) {
        wifi_connected = false;
        wifi_ssid.clear();
        notifyStateChange("wlan0", ConnectionState::DISCONNECTED);
        return true;
    }
    return false;
}

bool NetworkManager::isWiFiConnected() const {
    std::string output;
    if (executeCommand("wpa_cli -i wlan0 status 2>/dev/null | grep 'wpa_state'", output)) {
        return output.find("COMPLETED") != std::string::npos;
    }
    return false;
}

std::string NetworkManager::getWiFiSSID() const {
    std::string output;
    if (executeCommand("wpa_cli -i wlan0 status 2>/dev/null | grep 'ssid'", output)) {
        size_t pos = output.find('=');
        if (pos != std::string::npos) {
            return output.substr(pos + 1);
        }
    }
    return "";
}

int NetworkManager::getWiFiSignalStrength() const {
    std::string output;
    if (executeCommand("wpa_cli -i wlan0 signal_poll 2>/dev/null", output)) {
        std::regex signal_regex(R"([^0-9]*(-?\d+)[^0-9]*)");
        std::smatch match;
        if (std::regex_search(output, match, signal_regex)) {
            return std::stoi(match[1].str());
        }
    }
    return 0;
}

bool NetworkManager::forgetWiFi(const std::string& ssid) {
    std::string output;
    std::string cmd = "wpa_cli -i wlan0 remove_network 0";
    if (executeCommand(cmd, output)) {
        return true;
    }
    return false;
}

// ============================================
// CONNECTION MANAGEMENT
// ============================================

bool NetworkManager::checkInternetConnectivity(const std::string& host, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect to host
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        close(sock);
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    addr.sin_port = htons(80);
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    
    return result == 0;
}

bool NetworkManager::ping(const std::string& host, int count, int timeout_ms) {
    std::string output;
    std::string cmd = "ping -c " + std::to_string(count) + 
                     " -W " + std::to_string(timeout_ms / 1000) + 
                     " " + host + " 2>/dev/null";
    return executeCommand(cmd, output);
}

NetworkManager::ConnectionQuality NetworkManager::getConnectionQuality() const {
    ConnectionQuality quality;
    
    // Ping test
    int success = 0;
    int total = 10;
    std::vector<int> latencies;
    
    for (int i = 0; i < total; i++) {
        auto start = std::chrono::steady_clock::now();
        if (ping("8.8.8.8", 1, 1000)) {
            success++;
            auto end = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            latencies.push_back(latency);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    quality.packet_loss = ((total - success) * 100.0f) / total;
    
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        quality.latency_ms = latencies[latencies.size() / 2];
        
        // Calculate jitter
        if (latencies.size() > 1) {
            float sum = 0;
            for (size_t i = 1; i < latencies.size(); i++) {
                sum += std::abs(latencies[i] - latencies[i-1]);
            }
            quality.jitter_ms = sum / (latencies.size() - 1);
        }
    }
    
    // Calculate quality percentage
    quality.quality_percent = 100 - (quality.packet_loss + (quality.latency_ms / 10));
    quality.quality_percent = std::max(0.0f, std::min(100.0f, quality.quality_percent));
    
    if (quality.quality_percent >= 80) {
        quality.status = "Excellent";
    } else if (quality.quality_percent >= 60) {
        quality.status = "Good";
    } else if (quality.quality_percent >= 40) {
        quality.status = "Fair";
    } else {
        quality.status = "Poor";
    }
    
    return quality;
}

// ============================================
// STATISTICS
// ============================================

NetworkManager::NetworkStats NetworkManager::getStats(const std::string& interface) const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    auto it = interface_stats.find(interface);
    if (it != interface_stats.end()) {
        return it->second;
    }
    return NetworkStats{};
}

NetworkManager::NetworkStats NetworkManager::getTotalStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    NetworkStats total;
    for (const auto& [name, stats] : interface_stats) {
        total.bytes_sent += stats.bytes_sent;
        total.bytes_received += stats.bytes_received;
        total.packets_sent += stats.packets_sent;
        total.packets_received += stats.packets_received;
        total.errors_sent += stats.errors_sent;
        total.errors_received += stats.errors_received;
        total.dropped_sent += stats.dropped_sent;
        total.dropped_received += stats.dropped_received;
        total.collisions += stats.collisions;
    }
    return total;
}

void NetworkManager::resetStats(const std::string& interface) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    interface_stats[interface] = NetworkStats{};
}

void NetworkManager::resetAllStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    for (auto& [name, stats] : interface_stats) {
        stats = NetworkStats{};
    }
}

// ============================================
// PRIVATE METHODS
// ============================================

void NetworkManager::scanInterfaces() {
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) < 0) {
        return;
    }
    
    std::map<std::string, InterfaceInfo> new_interfaces;
    
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        
        std::string name = ifa->ifa_name;
        if (name == "lo" || name == "docker0" || name == "veth") {
            continue;
        }
        
        InterfaceInfo info;
        info.name = name;
        info.is_up = (ifa->ifa_flags & IFF_UP) != 0;
        info.type = getInterfaceType(name);
        info.mac_address = getMacAddress(name);
        
        // Get IP address
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            info.ip_address = inet_ntoa(addr->sin_addr);
            info.has_ip = true;
            
            // Get netmask
            if (ifa->ifa_netmask) {
                struct sockaddr_in* mask = (struct sockaddr_in*)ifa->ifa_netmask;
                info.netmask = inet_ntoa(mask->sin_addr);
            }
        }
        
        // Get speed
        info.speed_mbps = getInterfaceSpeed(name);
        
        // Check if default
        info.is_default = (name == "eth0" || name == "wlan0");
        
        new_interfaces[name] = info;
    }
    
    freeifaddrs(ifaddr);
    interfaces = new_interfaces;
}

void NetworkManager::updateInterface(const std::string& name) {
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
    
    auto it = interfaces.find(name);
    if (it == interfaces.end()) {
        close(sock);
        return;
    }
    
    // Check if up
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        it->second.is_up = (ifr.ifr_flags & IFF_UP) != 0;
    }
    
    // Get IP
    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
        it->second.ip_address = inet_ntoa(addr->sin_addr);
        it->second.has_ip = true;
    }
    
    close(sock);
}

void NetworkManager::updateStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    
    for (auto& [name, info] : interfaces) {
        std::string stats_path = "/sys/class/net/" + name + "/statistics/";
        NetworkStats stats;
        
        auto read_stat = [&](const std::string& file) -> uint64_t {
            std::string content;
            if (readSysfs(stats_path + file, content)) {
                try {
                    return std::stoull(content);
                } catch (...) {}
            }
            return 0;
        };
        
        stats.bytes_sent = read_stat("tx_bytes");
        stats.bytes_received = read_stat("rx_bytes");
        stats.packets_sent = read_stat("tx_packets");
        stats.packets_received = read_stat("rx_packets");
        stats.errors_sent = read_stat("tx_errors");
        stats.errors_received = read_stat("rx_errors");
        stats.dropped_sent = read_stat("tx_dropped");
        stats.dropped_received = read_stat("rx_dropped");
        stats.collisions = read_stat("collisions");
        stats.last_update = std::chrono::system_clock::now();
        
        // Calculate rates
        auto old_it = interface_stats.find(name);
        if (old_it != interface_stats.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                stats.last_update - old_it->second.last_update).count();
            if (elapsed > 0) {
                stats.send_rate_mbps = (stats.bytes_sent - old_it->second.bytes_sent) * 8.0 / (elapsed * 1000000);
                stats.receive_rate_mbps = (stats.bytes_received - old_it->second.bytes_received) * 8.0 / (elapsed * 1000000);
            }
        }
        
        interface_stats[name] = stats;
        notifyStats(name, stats);
    }
}

void NetworkManager::monitorLoop() {
    while (monitoring) {
        scanInterfaces();
        updateStats();
        
        // Check WiFi connection
        if (wifi_connected) {
            if (!isWiFiConnected()) {
                wifi_connected = false;
                notifyStateChange("wlan0", ConnectionState::DISCONNECTED);
            } else {
                wifi_signal = getWiFiSignalStrength();
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(monitor_interval));
    }
}

bool NetworkManager::setInterfaceFlag(const std::string& name, int flag, bool enable) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
    
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        return false;
    }
    
    if (enable) {
        ifr.ifr_flags |= flag;
    } else {
        ifr.ifr_flags &= ~flag;
    }
    
    int result = ioctl(sock, SIOCSIFFLAGS, &ifr);
    close(sock);
    return result == 0;
}

bool NetworkManager::getInterfaceFlag(const std::string& name, int flag) const {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
    
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        return false;
    }
    
    close(sock);
    return (ifr.ifr_flags & flag) != 0;
}

std::string NetworkManager::getMacAddress(const std::string& interface) const {
    std::string path = "/sys/class/net/" + interface + "/address";
    std::string content;
    if (readSysfs(path, content)) {
        return content;
    }
    return "";
}

NetworkManager::NetworkType NetworkManager::getInterfaceType(const std::string& interface) const {
    if (isWifiInterface(interface)) {
        return NetworkType::WIFI;
    }
    if (isEthernetInterface(interface)) {
        return NetworkType::ETHERNET;
    }
    if (interface == "lo") {
        return NetworkType::LOOPBACK;
    }
    return NetworkType::UNKNOWN;
}

bool NetworkManager::isWifiInterface(const std::string& interface) const {
    std::string path = "/sys/class/net/" + interface + "/wireless";
    return fileExists(path);
}

bool NetworkManager::isEthernetInterface(const std::string& interface) const {
    std::string path = "/sys/class/net/" + interface + "/device";
    return fileExists(path) && !isWifiInterface(interface);
}

bool NetworkManager::readSysfs(const std::string& path, std::string& content) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::getline(file, content);
    file.close();
    return true;
}

bool NetworkManager::writeSysfs(const std::string& path, const std::string& content) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    file.close();
    return true;
}

bool NetworkManager::executeCommand(const std::string& cmd, std::string& output) const {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int status = pclose(pipe);
    return status == 0;
}

bool NetworkManager::fileExists(const std::string& path) const {
    return access(path.c_str(), F_OK) == 0;
}

bool NetworkManager::isValidIP(const std::string& ip) const {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1;
}

bool NetworkManager::isValidMAC(const std::string& mac) const {
    std::regex mac_regex(R"(([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})");
    return std::regex_match(mac, mac_regex);
}

std::string NetworkManager::getLocalIP() const {
    auto default_iface = getDefaultInterface();
    return default_iface.ip_address;
}

std::string NetworkManager::getPublicIP() const {
    std::string output;
    if (executeCommand("curl -s ifconfig.me", output)) {
        return output;
    }
    return "";
}

uint32_t NetworkManager::getInterfaceSpeed(const std::string& name) const {
    std::string path = "/sys/class/net/" + name + "/speed";
    std::string content;
    if (readSysfs(path, content)) {
        try {
            return std::stoul(content);
        } catch (...) {}
    }
    return 0;
}

std::string NetworkManager::getNetworkTypeName(NetworkType type) const {
    switch (type) {
        case NetworkType::ETHERNET: return "Ethernet";
        case NetworkType::WIFI: return "WiFi";
        case NetworkType::LOOPBACK: return "Loopback";
        default: return "Unknown";
    }
}

std::string NetworkManager::getConnectionStateName(ConnectionState state) const {
    switch (state) {
        case ConnectionState::DISCONNECTED: return "Disconnected";
        case ConnectionState::CONNECTING: return "Connecting";
        case ConnectionState::CONNECTED: return "Connected";
        case ConnectionState::FAILED: return "Failed";
        default: return "Unknown";
    }
}

std::string NetworkManager::getIPTypeName(IPType type) const {
    switch (type) {
        case IPType::DHCP: return "DHCP";
        case IPType::STATIC: return "Static";
        case IPType::AUTO: return "Auto";
        default: return "Unknown";
    }
}

void NetworkManager::notifyStateChange(const std::string& interface, ConnectionState state) {
    if (on_state_change) {
        on_state_change(interface, state);
    }
}

void NetworkManager::notifyStats(const std::string& interface, const NetworkStats& stats) {
    if (on_stats) {
        on_stats(interface, stats);
    }
}

void NetworkManager::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "Network Error: " << error << std::endl;
}

void NetworkManager::notifyWiFiScan(const std::vector<WiFiConfig>& networks) {
    if (on_wifi_scan) {
        on_wifi_scan(networks);
    }
}

// ============================================
// CALLBACKS
// ============================================

void NetworkManager::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void NetworkManager::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

void NetworkManager::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void NetworkManager::setOnWiFiScan(WiFiScanCallback callback) {
    on_wifi_scan = callback;
}
