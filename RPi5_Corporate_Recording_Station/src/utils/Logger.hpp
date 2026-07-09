#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <functional>
#include <memory>

/**
 * @brief Logger for Recording Station
 * 
 * Provides comprehensive logging with:
 * - Multiple log levels
 * - File and console output
 * - Log rotation
 * - Component-specific logging
 * - Thread-safe operations
 * - Color output
 * - JSON format support
 */
class Logger {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        SUCCESS = 3,
        WARNING = 4,
        ERROR = 5,
        CRITICAL = 6,
        OFF = 7
    };
    
    enum class Output {
        CONSOLE,
        FILE,
        BOTH
    };
    
    enum class Format {
        TEXT,
        JSON,
        SYSLOG
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    struct Config {
        Level level = Level::INFO;
        Output output = Output::BOTH;
        Format format = Format::TEXT;
        std::string log_file = "/var/log/recording-station/recording-station.log";
        std::string error_file = "/var/log/recording-station/error.log";
        size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
        int max_files = 5;
        bool color_enabled = true;
        bool console_enabled = true;
        bool file_enabled = true;
        bool timestamp_enabled = true;
        bool component_enabled = true;
        bool thread_id_enabled = false;
        bool json_pretty = false;
        std::string time_format = "%Y-%m-%d %H:%M:%S";
        std::string component = "main";
        std::vector<std::string> components;
    };
    
    /**
     * @brief Log entry
     */
    struct Entry {
        Level level = Level::INFO;
        std::string component;
        std::string message;
        std::string file;
        int line = 0;
        std::string function;
        std::chrono::system_clock::time_point timestamp;
        std::string thread_id;
        std::map<std::string, std::string> extra;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using LogCallback = std::function<void(const Entry&)>;
    using ErrorCallback = std::function<void(const Entry&)>;
    
    // ============================================
    // SINGLETON
    // ============================================
    
    static Logger& getInstance();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize logger
     * @param config Logger configuration
     * @return true if successful
     */
    bool initialize(const Config& config = Config());
    
    /**
     * @brief Shutdown logger
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // LOGGING
    // ============================================
    
    /**
     * @brief Log a message
     */
    void log(Level level, const std::string& message, 
             const std::string& file = "", int line = 0,
             const std::string& function = "");
    
    /**
     * @brief Log with component
     */
    void log(Level level, const std::string& component,
             const std::string& message, const std::string& file = "",
             int line = 0, const std::string& function = "");
    
    // Convenience methods
    void trace(const std::string& message, const std::string& file = "", int line = 0,
               const std::string& function = "");
    void debug(const std::string& message, const std::string& file = "", int line = 0,
               const std::string& function = "");
    void info(const std::string& message, const std::string& file = "", int line = 0,
              const std::string& function = "");
    void success(const std::string& message, const std::string& file = "", int line = 0,
                 const std::string& function = "");
    void warning(const std::string& message, const std::string& file = "", int line = 0,
                 const std::string& function = "");
    void error(const std::string& message, const std::string& file = "", int line = 0,
               const std::string& function = "");
    void critical(const std::string& message, const std::string& file = "", int line = 0,
                  const std::string& function = "");
    
    /**
     * @brief Log formatted message
     */
    void logf(Level level, const char* format, ...);
    
    /**
     * @brief Log with extra fields
     */
    void logExtra(Level level, const std::string& message,
                  const std::map<std::string, std::string>& extra,
                  const std::string& file = "", int line = 0,
                  const std::string& function = "");
    
    // ============================================
    // COMPONENT LOGGING
    // ============================================
    
    /**
     * @brief Get component logger
     */
    class ComponentLogger {
    public:
        ComponentLogger(Logger& logger, const std::string& name);
        
        void trace(const std::string& message);
        void debug(const std::string& message);
        void info(const std::string& message);
        void success(const std::string& message);
        void warning(const std::string& message);
        void error(const std::string& message);
        void critical(const std::string& message);
        
    private:
        Logger& logger;
        std::string name;
    };
    
    /**
     * @brief Get component logger
     */
    ComponentLogger getComponent(const std::string& name);
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Set log level
     */
    void setLevel(Level level);
    
    /**
     * @brief Get log level
     */
    Level getLevel() const { return config.level; }
    
    /**
     * @brief Set output
     */
    void setOutput(Output output);
    
    /**
     * @brief Set log file
     */
    void setLogFile(const std::string& path);
    
    /**
     * @brief Set component
     */
    void setComponent(const std::string& component);
    
    /**
     * @brief Enable/disable color
     */
    void setColorEnabled(bool enable);
    
    /**
     * @brief Enable/disable console
     */
    void setConsoleEnabled(bool enable);
    
    /**
     * @brief Enable/disable file
     */
    void setFileEnabled(bool enable);
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnLog(LogCallback callback);
    void setOnError(ErrorCallback callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get level name
     */
    static std::string levelToString(Level level);
    
    /**
     * @brief Get level from string
     */
    static Level stringToLevel(const std::string& str);
    
    /**
     * @brief Get level color
     */
    static std::string getLevelColor(Level level);
    
    /**
     * @brief Get level icon
     */
    static std::string getLevelIcon(Level level);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    bool initialized = false;
    Config config;
    mutable std::mutex mutex;
    mutable std::mutex file_mutex;
    
    std::ofstream log_stream;
    std::ofstream error_stream;
    size_t current_file_size = 0;
    
    LogCallback on_log;
    ErrorCallback on_error;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    void logInternal(const Entry& entry);
    void writeToConsole(const Entry& entry);
    void writeToFile(const Entry& entry);
    void writeToError(const Entry& entry);
    void rotateLogFile();
    std::string formatEntry(const Entry& entry) const;
    std::string formatJSON(const Entry& entry) const;
    std::string formatText(const Entry& entry) const;
    std::string formatSyslog(const Entry& entry) const;
    std::string getTimestamp(const std::chrono::system_clock::time_point& tp) const;
    std::string getThreadId() const;
    bool shouldLog(const Entry& entry) const;
    std::string escapeJSON(const std::string& str) const;
    void checkAndRotate();
    
    // Color codes
    static const std::map<Level, std::string> level_colors;
    static const std::map<Level, std::string> level_icons;
    static const std::map<Level, std::string> level_names;
};

#endif // LOGGER_HPP
