#include "DashboardRenderer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/ioctl.h>

// Color codes
const std::map<DashboardRenderer::Color, std::string> DashboardRenderer::color_codes = {
    {Color::RESET, "\033[0m"},
    {Color::BLACK, "\033[30m"},
    {Color::RED, "\033[31m"},
    {Color::GREEN, "\033[32m"},
    {Color::YELLOW, "\033[33m"},
    {Color::BLUE, "\033[34m"},
    {Color::MAGENTA, "\033[35m"},
    {Color::CYAN, "\033[36m"},
    {Color::WHITE, "\033[37m"},
    {Color::BRIGHT_BLACK, "\033[90m"},
    {Color::BRIGHT_RED, "\033[91m"},
    {Color::BRIGHT_GREEN, "\033[92m"},
    {Color::BRIGHT_YELLOW, "\033[93m"},
    {Color::BRIGHT_BLUE, "\033[94m"},
    {Color::BRIGHT_MAGENTA, "\033[95m"},
    {Color::BRIGHT_CYAN, "\033[96m"},
    {Color::BRIGHT_WHITE, "\033[97m"}
};

const std::map<std::string, DashboardRenderer::Color> DashboardRenderer::log_level_colors = {
    {"DEBUG", Color::BRIGHT_CYAN},
    {"INFO", Color::BRIGHT_WHITE},
    {"SUCCESS", Color::BRIGHT_GREEN},
    {"WARNING", Color::BRIGHT_YELLOW},
    {"ERROR", Color::BRIGHT_RED},
    {"CRITICAL", Color::RED}
};

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

DashboardRenderer::DashboardRenderer() : initialized(false) {
}

DashboardRenderer::~DashboardRenderer() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool DashboardRenderer::initialize(const std::string& dashboard_title) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    title = dashboard_title;
    colors_enabled = true;
    compact_mode = false;
    mode = DashboardMode::FULL;
    
    initialized = true;
    return true;
}

void DashboardRenderer::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    initialized = false;
}

// ============================================
// DATA UPDATE
// ============================================

void DashboardRenderer::updateSystemMetrics(const SystemMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex);
    system_metrics = metrics;
}

void DashboardRenderer::updateRecordingStatus(const RecordingStatus& status) {
    std::lock_guard<std::mutex> lock(mutex);
    recording_status = status;
}

void DashboardRenderer::updateStorageStatus(const StorageStatus& status) {
    std::lock_guard<std::mutex> lock(mutex);
    storage_status = status;
}

void DashboardRenderer::updateCloudStatus(const CloudStatus& status) {
    std::lock_guard<std::mutex> lock(mutex);
    cloud_status = status;
}

void DashboardRenderer::updateNetworkStatus(const NetworkStatus& status) {
    std::lock_guard<std::mutex> lock(mutex);
    network_status = status;
}

void DashboardRenderer::updateGPIOStatus(const GPIOStatus& status) {
    std::lock_guard<std::mutex> lock(mutex);
    gpio_status = status;
}

void DashboardRenderer::addLogEntry(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(log_mutex);
    logs.push_back(entry);
    if ((int)logs.size() > max_log_entries) {
        logs.erase(logs.begin());
    }
}

void DashboardRenderer::addLog(const std::string& message, const std::string& level) {
    LogEntry entry;
    entry.timestamp = formatTimestamp(std::chrono::system_clock::now());
    entry.level = level;
    entry.message = message;
    
    auto it = log_level_colors.find(level);
    if (it != log_level_colors.end()) {
        entry.color = it->second;
    } else {
        entry.color = Color::WHITE;
    }
    
    addLogEntry(entry);
}

void DashboardRenderer::clearLogs() {
    std::lock_guard<std::mutex> lock(log_mutex);
    logs.clear();
}

// ============================================
// RENDER
// ============================================

void DashboardRenderer::render() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Clear screen
    std::cout << "\033[2J\033[H";
    
    // Render based on mode
    renderHeader();
    
    switch (mode) {
        case DashboardMode::FULL:
            renderSystemMetrics();
            renderSeparator();
            renderRecordingStatus();
            renderSeparator();
            renderStorageStatus();
            renderSeparator();
            renderCloudStatus();
            renderSeparator();
            renderNetworkStatus();
            renderSeparator();
            renderGPIOStatus();
            renderSeparator();
            renderLogs();
            break;
        case DashboardMode::MINIMAL:
            renderSystemMetrics();
            renderSeparator();
            renderRecordingStatus();
            renderSeparator();
            renderLogs();
            break;
        case DashboardMode::RECORDING:
            renderRecordingStatus();
            renderSeparator();
            renderSystemMetrics();
            renderSeparator();
            renderStorageStatus();
            renderSeparator();
            renderLogs();
            break;
        case DashboardMode::MONITORING:
            renderSystemMetrics();
            renderSeparator();
            renderStorageStatus();
            renderSeparator();
            renderNetworkStatus();
            renderSeparator();
            renderGPIOStatus();
            renderSeparator();
            renderLogs();
            break;
        case DashboardMode::STORAGE:
            renderStorageStatus();
            renderSeparator();
            renderSystemMetrics();
            renderSeparator();
            renderRecordingStatus();
            renderSeparator();
            renderCloudStatus();
            renderSeparator();
            renderLogs();
            break;
        case DashboardMode::NETWORK:
            renderNetworkStatus();
            renderSeparator();
            renderSystemMetrics();
            renderSeparator();
            renderCloudStatus();
            renderSeparator();
            renderGPIOStatus();
            renderSeparator();
            renderLogs();
            break;
    }
    
    renderFooter();
    std::cout.flush();
}

void DashboardRenderer::renderSection(const std::string& section) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Clear screen
    std::cout << "\033[2J\033[H";
    
    renderHeader();
    
    if (section == "system") {
        renderSystemMetrics();
    } else if (section == "recording") {
        renderRecordingStatus();
    } else if (section == "storage") {
        renderStorageStatus();
    } else if (section == "cloud") {
        renderCloudStatus();
    } else if (section == "network") {
        renderNetworkStatus();
    } else if (section == "gpio") {
        renderGPIOStatus();
    } else if (section == "logs") {
        renderLogs();
    } else {
        render();
    }
    
    renderFooter();
    std::cout.flush();
}

void DashboardRenderer::setMode(DashboardMode new_mode) {
    std::lock_guard<std::mutex> lock(mutex);
    mode = new_mode;
}

// ============================================
// RENDERING FUNCTIONS
// ============================================

void DashboardRenderer::renderHeader() {
    int width = getTerminalWidth();
    std::string title_line = " " + title + " ";
    int padding = (width - title_line.length()) / 2;
    
    std::cout << colorString(std::string(width, '='), Color::BRIGHT_BLACK) << "\n";
    if (padding > 0) {
        std::cout << std::string(padding, ' ');
    }
    std::cout << colorString(title_line, Color::BRIGHT_CYAN) << "\n";
    std::cout << colorString(std::string(width, '='), Color::BRIGHT_BLACK) << "\n";
    
    // Subtitle with mode and time
    std::string mode_str;
    switch (mode) {
        case DashboardMode::FULL: mode_str = "FULL"; break;
        case DashboardMode::MINIMAL: mode_str = "MINIMAL"; break;
        case DashboardMode::RECORDING: mode_str = "RECORDING"; break;
        case DashboardMode::MONITORING: mode_str = "MONITORING"; break;
        case DashboardMode::STORAGE: mode_str = "STORAGE"; break;
        case DashboardMode::NETWORK: mode_str = "NETWORK"; break;
    }
    
    std::string time_str = formatTimestamp(std::chrono::system_clock::now());
    std::string info = "Mode: " + mode_str + " | " + time_str;
    std::cout << colorString(padRight(info, width - 2), Color::BRIGHT_BLACK) << "\n\n";
}

void DashboardRenderer::renderFooter() {
    int width = getTerminalWidth();
    std::string footer = " Press 'q' to quit | 'r' to refresh | 'h' for help ";
    std::cout << colorString(padLeft(footer, width), Color::BRIGHT_BLACK) << "\n";
    std::cout << colorString(std::string(width, '='), Color::BRIGHT_BLACK) << "\n";
}

void DashboardRenderer::renderSystemMetrics() {
    std::cout << colorString("📊 SYSTEM METRICS", Color::BRIGHT_GREEN) << "\n";
    
    int width = getTerminalWidth();
    int bar_width = std::min(40, width - 30);
    
    // CPU
    std::cout << colorString("  CPU: ", Color::CYAN);
    renderBar(system_metrics.cpu_usage, 100, bar_width, Color::YELLOW);
    std::cout << " " << formatPercentage(system_metrics.cpu_usage) << "\n";
    
    // CPU Temperature
    std::cout << colorString("  Temp: ", Color::CYAN);
    std::cout << std::fixed << std::setprecision(1) << system_metrics.cpu_temp << "°C\n";
    
    // Memory
    std::cout << colorString("  Memory: ", Color::CYAN);
    renderBar(system_metrics.memory_percentage, 100, bar_width, Color::BLUE);
    std::cout << " " << formatPercentage(system_metrics.memory_percentage) << "\n";
    std::cout << colorString("    ", Color::BRIGHT_BLACK);
    std::cout << formatSize(system_metrics.memory_used) << " / " 
              << formatSize(system_metrics.memory_total) << "\n";
    
    // Swap
    if (system_metrics.swap_total > 0) {
        double swap_percent = (system_metrics.swap_used / system_metrics.swap_total) * 100;
        std::cout << colorString("  Swap: ", Color::CYAN);
        renderBar(swap_percent, 100, bar_width, Color::MAGENTA);
        std::cout << " " << formatPercentage(swap_percent) << "\n";
    }
    
    // Load average
    std::cout << colorString("  Load: ", Color::CYAN);
    std::cout << system_metrics.load_avg_1min << ", " 
              << system_metrics.load_avg_5min << ", " 
              << system_metrics.load_avg_15min << "\n";
    
    // Uptime
    std::cout << colorString("  Uptime: ", Color::CYAN);
    std::cout << formatDuration((int)system_metrics.uptime_seconds) << "\n";
    
    // Hostname
    std::cout << colorString("  Host: ", Color::CYAN);
    std::cout << system_metrics.hostname << "\n";
    
    std::cout << "\n";
}

void DashboardRenderer::renderRecordingStatus() {
    std::cout << colorString("🎥 RECORDING STATUS", Color::BRIGHT_GREEN) << "\n";
    
    std::string status_icon = getRecordingIcon();
    std::string status_text = recording_status.is_recording ? 
        (recording_status.is_paused ? "PAUSED" : "RECORDING") : "IDLE";
    Color status_color = recording_status.is_recording ? 
        (recording_status.is_paused ? Color::YELLOW : Color::RED) : Color::BRIGHT_BLACK;
    
    std::cout << "  Status: " << colorString(status_icon + " " + status_text, status_color) << "\n";
    
    if (recording_status.is_recording) {
        std::cout << colorString("  File: ", Color::CYAN) << recording_status.filename << "\n";
        std::cout << colorString("  Duration: ", Color::CYAN) 
                  << formatDuration(recording_status.duration_seconds) << "\n";
        std::cout << colorString("  Size: ", Color::CYAN) 
                  << formatSize(recording_status.file_size_bytes) << "\n";
        std::cout << colorString("  FPS: ", Color::CYAN) 
                  << std::fixed << std::setprecision(1) << recording_status.fps << "\n";
        std::cout << colorString("  Bitrate: ", Color::CYAN) 
                  << std::fixed << std::setprecision(1) << recording_status.bitrate / 1000000 << " Mbps\n";
        std::cout << colorString("  Video: ", Color::CYAN) << recording_status.video_codec << "\n";
        std::cout << colorString("  Audio: ", Color::CYAN) << recording_status.audio_codec << "\n";
    }
    
    std::cout << "\n";
}

void DashboardRenderer::renderStorageStatus() {
    std::cout << colorString("💾 STORAGE STATUS", Color::BRIGHT_GREEN) << "\n";
    
    if (storage_status.total_bytes > 0) {
        double used_percent = (double)storage_status.used_bytes / storage_status.total_bytes * 100;
        int width = getTerminalWidth();
        int bar_width = std::min(40, width - 30);
        
        std::cout << "  Used: ";
        Color bar_color = used_percent > 85 ? Color::RED : (used_percent > 70 ? Color::YELLOW : Color::GREEN);
        renderBar(used_percent, 100, bar_width, bar_color);
        std::cout << " " << formatPercentage(used_percent) << "\n";
        
        std::cout << colorString("    ", Color::BRIGHT_BLACK);
        std::cout << formatSize(storage_status.used_bytes) << " / " 
                  << formatSize(storage_status.total_bytes) << "\n";
        
        std::cout << colorString("  Free: ", Color::CYAN) 
                  << formatSize(storage_status.free_bytes) << "\n";
        std::cout << colorString("  Files: ", Color::CYAN) 
                  << storage_status.file_count << "\n";
        std::cout << colorString("  Directories: ", Color::CYAN) 
                  << storage_status.directory_count << "\n";
        
        // File type distribution
        if (!storage_status.file_type_distribution.empty()) {
            std::cout << colorString("  Types: ", Color::CYAN);
            bool first = true;
            for (const auto& [type, count] : storage_status.file_type_distribution) {
                if (!first) std::cout << ", ";
                std::cout << type << "(" << count << ")";
                first = false;
            }
            std::cout << "\n";
        }
    } else {
        std::cout << colorString("  No storage data available", Color::BRIGHT_BLACK) << "\n";
    }
    
    std::cout << "\n";
}

void DashboardRenderer::renderCloudStatus() {
    std::cout << colorString("☁️ CLOUD STATUS", Color::BRIGHT_GREEN) << "\n";
    
    std::string cloud_icon = getCloudIcon();
    std::string status_text = cloud_status.is_connected ? "CONNECTED" : "DISCONNECTED";
    Color status_color = cloud_status.is_connected ? Color::GREEN : Color::RED;
    
    std::cout << "  Status: " << colorString(cloud_icon + " " + status_text, status_color) << "\n";
    std::cout << colorString("  Provider: ", Color::CYAN) << cloud_status.provider << "\n";
    std::cout << colorString("  Username: ", Color::CYAN) << cloud_status.username << "\n";
    
    if (cloud_status.is_connected) {
        std::cout << colorString("  Total Space: ", Color::CYAN) 
                  << formatSize(cloud_status.total_space) << "\n";
        std::cout << colorString("  Used Space: ", Color::CYAN) 
                  << formatSize(cloud_status.used_space) << "\n";
        std::cout << colorString("  Free Space: ", Color::CYAN) 
                  << formatSize(cloud_status.free_space) << "\n";
        
        if (cloud_status.is_syncing) {
            std::cout << colorString("  Syncing: ", Color::YELLOW) << cloud_status.current_file << "\n";
            int width = getTerminalWidth();
            int bar_width = std::min(40, width - 30);
            renderBar(cloud_status.sync_progress, 100, bar_width, Color::BLUE);
            std::cout << " " << formatPercentage(cloud_status.sync_progress) << "\n";
        }
        
        std::cout << colorString("  Queue: ", Color::CYAN) << cloud_status.queue_size << " files\n";
        std::cout << colorString("  Last Sync: ", Color::CYAN) << cloud_status.last_sync << "\n";
    }
    
    std::cout << "\n";
}

void DashboardRenderer::renderNetworkStatus() {
    std::cout << colorString("🌐 NETWORK STATUS", Color::BRIGHT_GREEN) << "\n";
    
    std::string net_icon = getNetworkIcon();
    std::string status_text = network_status.is_connected ? "CONNECTED" : "DISCONNECTED";
    Color status_color = network_status.is_connected ? Color::GREEN : Color::RED;
    
    std::cout << "  Status: " << colorString(net_icon + " " + status_text, status_color) << "\n";
    std::cout << colorString("  Interface: ", Color::CYAN) << network_status.interface << "\n";
    std::cout << colorString("  IP Address: ", Color::CYAN) << network_status.ip_address << "\n";
    std::cout << colorString("  MAC Address: ", Color::CYAN) << network_status.mac_address << "\n";
    
    if (!network_status.wifi_ssid.empty()) {
        std::cout << colorString("  WiFi SSID: ", Color::CYAN) << network_status.wifi_ssid << "\n";
        std::cout << colorString("  Signal: ", Color::CYAN) << network_status.wifi_signal << " dBm\n";
    }
    
    std::cout << colorString("  Gateway: ", Color::CYAN) << network_status.gateway << "\n";
    std::cout << colorString("  DNS: ", Color::CYAN);
    for (size_t i = 0; i < network_status.dns_servers.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << network_status.dns_servers[i];
    }
    std::cout << "\n";
    
    // Network traffic
    std::cout << colorString("  Traffic: ", Color::CYAN);
    std::cout << "↓ " << formatSize(network_status.bytes_received) << " ";
    std::cout << "↑ " << formatSize(network_status.bytes_sent) << "\n";
    
    std::cout << colorString("  Rate: ", Color::CYAN);
    std::cout << "↓ " << std::fixed << std::setprecision(1) 
              << network_status.receive_rate_mbps << " Mbps ";
    std::cout << "↑ " << std::fixed << std::setprecision(1) 
              << network_status.send_rate_mbps << " Mbps\n";
    
    std::cout << "\n";
}

void DashboardRenderer::renderGPIOStatus() {
    std::cout << colorString("🔌 GPIO STATUS", Color::BRIGHT_GREEN) << "\n";
    
    if (gpio_status.pins.empty()) {
        std::cout << colorString("  No GPIO data available", Color::BRIGHT_BLACK) << "\n";
        std::cout << "\n";
        return;
    }
    
    // Table header
    std::cout << colorString("  Pin  Name          Dir   Pull  Value  Changes", Color::BRIGHT_BLACK) << "\n";
    std::cout << colorString("  " + std::string(38, '-'), Color::BRIGHT_BLACK) << "\n";
    
    // Show up to 10 pins
    int count = 0;
    for (const auto& pin : gpio_status.pins) {
        if (count++ >= 10) break;
        
        std::string value_str = pin.value ? "HIGH" : "LOW ";
        Color value_color = pin.value ? Color::GREEN : Color::RED;
        
        std::stringstream ss;
        ss << std::setw(4) << pin.pin << " ";
        ss << std::setw(12) << pin.name.substr(0, 12) << " ";
        ss << std::setw(5) << pin.direction.substr(0, 5) << " ";
        ss << std::setw(5) << pin.pull.substr(0, 5) << " ";
        
        std::cout << "  " << ss.str();
        std::cout << colorString(value_str, value_color) << " ";
        std::cout << std::setw(7) << pin.change_count << "\n";
    }
    
    if ((int)gpio_status.pins.size() > 10) {
        std::cout << colorString("  ... and " + std::to_string(gpio_status.pins.size() - 10) + " more pins",
                                Color::BRIGHT_BLACK) << "\n";
    }
    
    std::cout << "\n";
}

void DashboardRenderer::renderLogs() {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    std::cout << colorString("📋 LOGS", Color::BRIGHT_GREEN) << "\n";
    
    if (logs.empty()) {
        std::cout << colorString("  No logs available", Color::BRIGHT_BLACK) << "\n";
        std::cout << "\n";
        return;
    }
    
    // Show last 10 logs
    int start = std::max(0, (int)logs.size() - 10);
    for (int i = start; i < (int)logs.size(); i++) {
        const auto& entry = logs[i];
        std::string timestamp = entry.timestamp.substr(11, 8); // HH:MM:SS
        
        std::cout << colorString("  " + timestamp, Color::BRIGHT_BLACK) << " ";
        std::cout << colorString("[" + entry.level + "]", entry.color) << " ";
        std::cout << entry.message << "\n";
    }
    
    std::cout << "\n";
}

// ============================================
// RENDER HELPERS
// ============================================

void DashboardRenderer::renderSeparator(char fill, int width) {
    if (width < 0) {
        width = getTerminalWidth();
    }
    std::cout << colorString(std::string(width, fill), Color::BRIGHT_BLACK) << "\n";
}

void DashboardRenderer::renderProgressBar(float progress, int width, Color color) {
    int filled = std::max(0, std::min(width, (int)(progress / 100.0f * width)));
    int empty = width - filled;
    
    std::string bar;
    bar += colorString(std::string(filled, '█'), color);
    if (empty > 0) {
        bar += colorString(std::string(empty, '░'), Color::BRIGHT_BLACK);
    }
    
    std::cout << bar;
}

void DashboardRenderer::renderBar(float value, float max, int width, Color color) {
    float percent = std::max(0.0f, std::min(100.0f, (value / max) * 100));
    renderProgressBar(percent, width, color);
}

// ============================================
// FORMATTING FUNCTIONS
// ============================================

std::string DashboardRenderer::formatSize(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int i = 0;
    double size = bytes;
    while (size >= 1024 && i < 5) {
        size /= 1024;
        i++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << size << " " << units[i];
    return ss.str();
}

std::string DashboardRenderer::formatDuration(int seconds) const {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    if (hours > 0) {
        return std::to_string(hours) + "h " + 
               std::to_string(minutes) + "m " + 
               std::to_string(secs) + "s";
    } else if (minutes > 0) {
        return std::to_string(minutes) + "m " + 
               std::to_string(secs) + "s";
    } else {
        return std::to_string(secs) + "s";
    }
}

std::string DashboardRenderer::formatPercentage(double value) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << value << "%";
    return ss.str();
}

std::string DashboardRenderer::formatTimestamp(const std::chrono::system_clock::time_point& tp) const {
    auto time_t_now = std::chrono::system_clock::to_time_t(tp);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}

std::string DashboardRenderer::colorString(const std::string& text, Color color) const {
    if (!colors_enabled) {
        return text;
    }
    auto it = color_codes.find(color);
    if (it != color_codes.end()) {
        return it->second + text + color_codes.at(Color::RESET);
    }
    return text;
}

std::string DashboardRenderer::boldString(const std::string& text) const {
    if (!colors_enabled) {
        return text;
    }
    return "\033[1m" + text + "\033[0m";
}

std::string DashboardRenderer::padRight(const std::string& text, int width) const {
    if ((int)text.length() >= width) {
        return text.substr(0, width);
    }
    return text + std::string(width - text.length(), ' ');
}

std::string DashboardRenderer::padLeft(const std::string& text, int width) const {
    if ((int)text.length() >= width) {
        return text.substr(0, width);
    }
    return std::string(width - text.length(), ' ') + text;
}

std::string DashboardRenderer::centerString(const std::string& text, int width) const {
    if ((int)text.length() >= width) {
        return text.substr(0, width);
    }
    int padding = (width - text.length()) / 2;
    return std::string(padding, ' ') + text + std::string(width - text.length() - padding, ' ');
}

std::string DashboardRenderer::truncateString(const std::string& text, int max_width) const {
    if ((int)text.length() <= max_width) {
        return text;
    }
    return text.substr(0, max_width - 3) + "...";
}

int DashboardRenderer::getTerminalWidth() const {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return std::max(40, (int)w.ws_col);
    }
    return 80;
}

// ============================================
// ICON HELPERS
// ============================================

std::string DashboardRenderer::getStatusIcon(bool status) const {
    return status ? "✅" : "❌";
}

std::string DashboardRenderer::getRecordingIcon() const {
    if (recording_status.is_recording) {
        return recording_status.is_paused ? "⏸" : "🔴";
    }
    return "⏹";
}

std::string DashboardRenderer::getCloudIcon() const {
    return cloud_status.is_connected ? "☁️" : "🌧";
}

std::string DashboardRenderer::getNetworkIcon() const {
    return network_status.is_connected ? "📶" : "📡";
}

std::string DashboardRenderer::getMemoryBar(double percentage) const {
    int bars = std::min(10, (int)(percentage / 10));
    return std::string(bars, '█') + std::string(10 - bars, '░');
}

std::string DashboardRenderer::getStorageBar(double percentage) const {
    int bars = std::min(10, (int)(percentage / 10));
    return std::string(bars, '█') + std::string(10 - bars, '░');
}

// ============================================
// CONFIGURATION
// ============================================

void DashboardRenderer::setRefreshInterval(int seconds) {
    refresh_interval = std::max(1, seconds);
}

void DashboardRenderer::setAutoRefresh(bool enable) {
    auto_refresh = enable;
}

void DashboardRenderer::setMaxLogEntries(int max) {
    max_log_entries = std::max(10, max);
}

void DashboardRenderer::setColorsEnabled(bool enable) {
    colors_enabled = enable;
}

void DashboardRenderer::setCompactMode(bool enable) {
    compact_mode = enable;
}

// ============================================
// CALLBACKS
// ============================================

void DashboardRenderer::setActionCallback(ActionCallback callback) {
    action_callback = callback;
}

void DashboardRenderer::setUpdateCallback(UpdateCallback callback) {
    update_callback = callback;
}

// ============================================
// EXPORT
// ============================================

std::string DashboardRenderer::exportToString() const {
    std::stringstream ss;
    
    ss << "=== " << title << " ===\n\n";
    
    // System metrics
    ss << "SYSTEM METRICS:\n";
    ss << "  CPU: " << std::fixed << std::setprecision(1) << system_metrics.cpu_usage << "%\n";
    ss << "  Temp: " << std::fixed << std::setprecision(1) << system_metrics.cpu_temp << "°C\n";
    ss << "  Memory: " << formatSize(system_metrics.memory_used) << " / " 
       << formatSize(system_metrics.memory_total) << "\n";
    ss << "  Uptime: " << formatDuration((int)system_metrics.uptime_seconds) << "\n\n";
    
    // Recording status
    ss << "RECORDING:\n";
    ss << "  Status: " << (recording_status.is_recording ? "RECORDING" : "IDLE") << "\n";
    if (recording_status.is_recording) {
        ss << "  File: " << recording_status.filename << "\n";
        ss << "  Duration: " << formatDuration(recording_status.duration_seconds) << "\n";
        ss << "  Size: " << formatSize(recording_status.file_size_bytes) << "\n";
    }
    ss << "\n";
    
    // Storage
    ss << "STORAGE:\n";
    if (storage_status.total_bytes > 0) {
        ss << "  Used: " << formatSize(storage_status.used_bytes) << " / " 
           << formatSize(storage_status.total_bytes) << "\n";
        ss << "  Free: " << formatSize(storage_status.free_bytes) << "\n";
    }
    ss << "  Files: " << storage_status.file_count << "\n\n";
    
    // Cloud
    ss << "CLOUD:\n";
    ss << "  Status: " << (cloud_status.is_connected ? "CONNECTED" : "DISCONNECTED") << "\n";
    if (cloud_status.is_connected) {
        ss << "  Provider: " << cloud_status.provider << "\n";
        ss << "  Used: " << formatSize(cloud_status.used_space) << " / " 
           << formatSize(cloud_status.total_space) << "\n";
    }
    
    return ss.str();
}

bool DashboardRenderer::exportToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    file << exportToString();
    file.close();
    return true;
}

std::string DashboardRenderer::exportToHTML() const {
    std::stringstream ss;
    
    ss << "<!DOCTYPE html>\n";
    ss << "<html>\n<head>\n";
    ss << "<title>" << title << "</title>\n";
    ss << "<style>\n";
    ss << "body { font-family: monospace; background: #1a1a2e; color: #e0e7ee; padding: 20px; }\n";
    ss << "h1 { color: #64b5f6; }\n";
    ss << ".section { margin: 20px 0; padding: 10px; border: 1px solid #333; border-radius: 5px; }\n";
    ss << ".label { color: #8899aa; }\n";
    ss << ".value { color: #e0e7ee; }\n";
    ss << ".good { color: #66bb6a; }\n";
    ss << ".warning { color: #ffd54f; }\n";
    ss << ".error { color: #ef5350; }\n";
    ss << ".recording { color: #ef5350; font-weight: bold; }\n";
    ss << "</style>\n";
    ss << "</head>\n<body>\n";
    
    ss << "<h1>" << title << "</h1>\n";
    
    // System
    ss << "<div class='section'>\n";
    ss << "<h2>📊 System Metrics</h2>\n";
    ss << "<span class='label'>CPU:</span> <span class='value'>" 
       << std::fixed << std::setprecision(1) << system_metrics.cpu_usage << "%</span><br>\n";
    ss << "<span class='label'>Temperature:</span> <span class='value'>" 
       << std::fixed << std::setprecision(1) << system_metrics.cpu_temp << "°C</span><br>\n";
    ss << "<span class='label'>Memory:</span> <span class='value'>" 
       << formatSize(system_metrics.memory_used) << " / " 
       << formatSize(system_metrics.memory_total) << "</span><br>\n";
    ss << "<span class='label'>Uptime:</span> <span class='value'>" 
       << formatDuration((int)system_metrics.uptime_seconds) << "</span><br>\n";
    ss << "</div>\n";
    
    // Recording
    ss << "<div class='section'>\n";
    ss << "<h2>🎥 Recording Status</h2>\n";
    ss << "<span class='label'>Status:</span> <span class='";
    if (recording_status.is_recording) {
        ss << "recording";
    } else {
        ss << "value";
    }
    ss << "'>" << (recording_status.is_recording ? "🔴 RECORDING" : "⏹ IDLE") << "</span><br>\n";
    if (recording_status.is_recording) {
        ss << "<span class='label'>File:</span> <span class='value'>" 
           << recording_status.filename << "</span><br>\n";
        ss << "<span class='label'>Duration:</span> <span class='value'>" 
           << formatDuration(recording_status.duration_seconds) << "</span><br>\n";
        ss << "<span class='label'>Size:</span> <span class='value'>" 
           << formatSize(recording_status.file_size_bytes) << "</span><br>\n";
    }
    ss << "</div>\n";
    
    // Cloud
    ss << "<div class='section'>\n";
    ss << "<h2>☁️ Cloud Status</h2>\n";
    ss << "<span class='label'>Status:</span> <span class='" 
       << (cloud_status.is_connected ? "good" : "error") << "'>" 
       << (cloud_status.is_connected ? "✅ CONNECTED" : "❌ DISCONNECTED") << "</span><br>\n";
    if (cloud_status.is_connected) {
        ss << "<span class='label'>Provider:</span> <span class='value'>" 
           << cloud_status.provider << "</span><br>\n";
        ss << "<span class='label'>Used:</span> <span class='value'>" 
           << formatSize(cloud_status.used_space) << " / " 
           << formatSize(cloud_status.total_space) << "</span><br>\n";
    }
    ss << "</div>\n";
    
    ss << "</body>\n</html>\n";
    
    return ss.str();
}
