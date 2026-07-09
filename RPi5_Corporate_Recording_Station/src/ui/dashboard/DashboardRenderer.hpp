#ifndef DASHBOARD_RENDERER_HPP
#define DASHBOARD_RENDERER_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

/**
 * @brief Dashboard Renderer for Recording Station
 * 
 * Provides a real-time dashboard view with:
 * - System metrics (CPU, memory, temperature)
 * - Recording status and controls
 * - Storage usage and statistics
 * - Cloud sync status
 * - Network information
 * - GPIO status
 * - Event logs
 */
class DashboardRenderer {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class DashboardMode {
        FULL,           // Full dashboard with all sections
        MINIMAL,        // Minimal view with only essential info
        RECORDING,      // Focus on recording controls
        MONITORING,     // Focus on system monitoring
        STORAGE,        // Focus on storage management
        NETWORK         // Focus on network status
    };
    
    enum class Color {
        RESET,
        BLACK,
        RED,
        GREEN,
        YELLOW,
        BLUE,
        MAGENTA,
        CYAN,
        WHITE,
        BRIGHT_BLACK,
        BRIGHT_RED,
        BRIGHT_GREEN,
        BRIGHT_YELLOW,
        BRIGHT_BLUE,
        BRIGHT_MAGENTA,
        BRIGHT_CYAN,
        BRIGHT_WHITE
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief System metrics
     */
    struct SystemMetrics {
        double cpu_usage = 0;
        double cpu_temp = 0;
        double memory_total = 0;
        double memory_used = 0;
        double memory_free = 0;
        double memory_available = 0;
        double memory_percentage = 0;
        double swap_total = 0;
        double swap_used = 0;
        double swap_free = 0;
        double uptime_seconds = 0;
        int load_avg_1min = 0;
        int load_avg_5min = 0;
        int load_avg_15min = 0;
        int process_count = 0;
        int thread_count = 0;
        std::string kernel_version;
        std::string hostname;
    };
    
    /**
     * @brief Recording status
     */
    struct RecordingStatus {
        bool is_recording = false;
        bool is_paused = false;
        std::string filename;
        int duration_seconds = 0;
        int64_t file_size_bytes = 0;
        int video_frames = 0;
        int audio_samples = 0;
        double fps = 0;
        double bitrate = 0;
        std::string video_codec;
        std::string audio_codec;
        std::chrono::system_clock::time_point start_time;
    };
    
    /**
     * @brief Storage status
     */
    struct StorageStatus {
        uint64_t total_bytes = 0;
        uint64_t used_bytes = 0;
        uint64_t free_bytes = 0;
        uint64_t available_bytes = 0;
        size_t file_count = 0;
        size_t directory_count = 0;
        std::string mount_point;
        std::string filesystem;
        std::map<std::string, size_t> file_type_distribution;
        std::map<std::string, size_t> file_age_distribution;
    };
    
    /**
     * @brief Cloud status
     */
    struct CloudStatus {
        bool is_connected = false;
        bool is_syncing = false;
        std::string provider;
        std::string username;
        uint64_t total_space = 0;
        uint64_t used_space = 0;
        uint64_t free_space = 0;
        size_t queue_size = 0;
        std::string last_sync;
        std::string current_file;
        float sync_progress = 0;
    };
    
    /**
     * @brief Network status
     */
    struct NetworkStatus {
        std::string interface;
        std::string ip_address;
        std::string mac_address;
        std::string gateway;
        std::vector<std::string> dns_servers;
        bool is_connected = false;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t packets_sent = 0;
        uint64_t packets_received = 0;
        double send_rate_mbps = 0;
        double receive_rate_mbps = 0;
        std::string wifi_ssid;
        int wifi_signal = 0;
    };
    
    /**
     * @brief GPIO status
     */
    struct GPIOStatus {
        struct PinInfo {
            int pin = -1;
            std::string name;
            bool value = false;
            std::string direction;
            std::string pull;
            bool interrupt_enabled = false;
            uint64_t change_count = 0;
        };
        std::vector<PinInfo> pins;
        std::map<int, std::string> pin_functions;
    };
    
    /**
     * @brief Log entry
     */
    struct LogEntry {
        std::string timestamp;
        std::string level;
        std::string message;
        std::string source;
        Color color = Color::WHITE;
        bool is_highlighted = false;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using ActionCallback = std::function<void(const std::string&)>;
    using UpdateCallback = std::function<void()>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    DashboardRenderer();
    ~DashboardRenderer();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize dashboard
     * @param title Dashboard title
     * @return true if successful
     */
    bool initialize(const std::string& title = "Recording Station Dashboard");
    
    /**
     * @brief Shutdown dashboard
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // DATA UPDATE
    // ============================================
    
    /**
     * @brief Update system metrics
     */
    void updateSystemMetrics(const SystemMetrics& metrics);
    
    /**
     * @brief Update recording status
     */
    void updateRecordingStatus(const RecordingStatus& status);
    
    /**
     * @brief Update storage status
     */
    void updateStorageStatus(const StorageStatus& status);
    
    /**
     * @brief Update cloud status
     */
    void updateCloudStatus(const CloudStatus& status);
    
    /**
     * @brief Update network status
     */
    void updateNetworkStatus(const NetworkStatus& status);
    
    /**
     * @brief Update GPIO status
     */
    void updateGPIOStatus(const GPIOStatus& status);
    
    /**
     * @brief Add log entry
     */
    void addLogEntry(const LogEntry& entry);
    
    /**
     * @brief Add simple log message
     */
    void addLog(const std::string& message, const std::string& level = "INFO");
    
    /**
     * @brief Clear logs
     */
    void clearLogs();
    
    // ============================================
    // RENDER
    // ============================================
    
    /**
     * @brief Render dashboard
     */
    void render();
    
    /**
     * @brief Render specific section
     */
    void renderSection(const std::string& section);
    
    /**
     * @brief Set dashboard mode
     */
    void setMode(DashboardMode mode);
    
    /**
     * @brief Get current mode
     */
    DashboardMode getMode() const { return mode; }
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Set refresh interval
     */
    void setRefreshInterval(int seconds);
    
    /**
     * @brief Set auto-refresh
     */
    void setAutoRefresh(bool enable);
    
    /**
     * @brief Set max log entries
     */
    void setMaxLogEntries(int max);
    
    /**
     * @brief Set colors enabled
     */
    void setColorsEnabled(bool enable);
    
    /**
     * @brief Set compact mode
     */
    void setCompactMode(bool enable);
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    /**
     * @brief Set action callback (for interactive elements)
     */
    void setActionCallback(ActionCallback callback);
    
    /**
     * @brief Set update callback
     */
    void setUpdateCallback(UpdateCallback callback);
    
    /**
     * @brief Get action callback
     */
    ActionCallback getActionCallback() const { return action_callback; }
    
    // ============================================
    // EXPORT
    // ============================================
    
    /**
     * @brief Export dashboard to string
     */
    std::string exportToString() const;
    
    /**
     * @brief Export dashboard to file
     */
    bool exportToFile(const std::string& filename) const;
    
    /**
     * @brief Export dashboard to HTML
     */
    std::string exportToHTML() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    bool initialized = false;
    bool auto_refresh = false;
    bool colors_enabled = true;
    bool compact_mode = false;
    int refresh_interval = 5;  // seconds
    int max_log_entries = 100;
    DashboardMode mode = DashboardMode::FULL;
    std::string title;
    mutable std::mutex mutex;
    mutable std::mutex log_mutex;
    
    // Data
    SystemMetrics system_metrics;
    RecordingStatus recording_status;
    StorageStatus storage_status;
    CloudStatus cloud_status;
    NetworkStatus network_status;
    GPIOStatus gpio_status;
    std::vector<LogEntry> logs;
    
    // Callbacks
    ActionCallback action_callback;
    UpdateCallback update_callback;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // Rendering functions
    void renderHeader();
    void renderFooter();
    void renderSystemMetrics();
    void renderRecordingStatus();
    void renderStorageStatus();
    void renderCloudStatus();
    void renderNetworkStatus();
    void renderGPIOStatus();
    void renderLogs();
    void renderSeparator(char fill = '-', int width = -1);
    void renderProgressBar(float progress, int width, Color color);
    void renderBar(float value, float max, int width, Color color);
    
    // Formatting functions
    std::string formatSize(uint64_t bytes) const;
    std::string formatDuration(int seconds) const;
    std::string formatPercentage(double value) const;
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) const;
    std::string colorString(const std::string& text, Color color) const;
    std::string boldString(const std::string& text) const;
    std::string padRight(const std::string& text, int width) const;
    std::string padLeft(const std::string& text, int width) const;
    std::string centerString(const std::string& text, int width) const;
    std::string truncateString(const std::string& text, int max_width) const;
    int getTerminalWidth() const;
    
    // Color maps
    static const std::map<Color, std::string> color_codes;
    static const std::map<std::string, Color> log_level_colors;
    
    // Helper
    std::string getStatusIcon(bool status) const;
    std::string getRecordingIcon() const;
    std::string getCloudIcon() const;
    std::string getNetworkIcon() const;
    std::string getMemoryBar(double percentage) const;
    std::string getStorageBar(double percentage) const;
};

#endif // DASHBOARD_RENDERER_HPP
