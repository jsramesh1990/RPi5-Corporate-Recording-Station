#include "Logger.hpp"

#include <iostream>
#include <cstdarg>
#include <ctime>
#include <thread>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

// Level colors (ANSI)
const std::map<Logger::Level, std::string> Logger::level_colors = {
    {Level::TRACE, "\033[90m"},     // Bright Black
    {Level::DEBUG, "\033[36m"},     // Cyan
    {Level::INFO, "\033[37m"},      // White
    {Level::SUCCESS, "\033[32m"},   // Green
    {Level::WARNING, "\033[33m"},   // Yellow
    {Level::ERROR, "\033[31m"},     // Red
    {Level::CRITICAL, "\033[91m"}   // Bright Red
};

const std::map<Logger::Level, std::string> Logger::level_icons = {
    {Level::TRACE, "🔍"},
    {Level::DEBUG, "🐛"},
    {Level::INFO, "ℹ️"},
    {Level::SUCCESS, "✅"},
    {Level::WARNING, "⚠️"},
    {Level::ERROR, "❌"},
    {Level::CRITICAL, "💀"}
};

const std::map<Logger::Level, std::string> Logger::level_names = {
    {Level::TRACE, "TRACE"},
    {Level::DEBUG, "DEBUG"},
    {Level::INFO, "INFO"},
    {Level::SUCCESS, "SUCCESS"},
    {Level::WARNING, "WARNING"},
    {Level::ERROR, "ERROR"},
    {Level::CRITICAL, "CRITICAL"},
    {Level::OFF, "OFF"}
};

// ============================================
// SINGLETON
// ============================================

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

Logger::Logger() {
    // Initialize default config
    config.level = Level::INFO;
    config.output = Output::BOTH;
    config.format = Format::TEXT;
    config.log_file = "/var/log/recording-station/recording-station.log";
    config.error_file = "/var/log/recording-station/error.log";
    config.max_file_size = 10 * 1024 * 1024;
    config.max_files = 5;
    config.color_enabled = true;
    config.console_enabled = true;
    config.file_enabled = true;
    config.timestamp_enabled = true;
    config.component_enabled = true;
    config.thread_id_enabled = false;
    config.json_pretty = false;
    config.time_format = "%Y-%m-%d %H:%M:%S";
    config.component = "main";
}

Logger::~Logger() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool Logger::initialize(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    config = cfg;
    
    // Open log file
    if (config.file_enabled && !config.log_file.empty()) {
        log_stream.open(config.log_file, std::ios::app);
        if (!log_stream.is_open()) {
            std::cerr << "Failed to open log file: " << config.log_file << std::endl;
        }
        
        // Open error file
        if (!config.error_file.empty() && config.error_file != config.log_file) {
            error_stream.open(config.error_file, std::ios::app);
        }
    }
    
    initialized = true;
    
    // Log startup
    info("Logger initialized");
    return true;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    if (log_stream.is_open()) {
        log_stream.close();
    }
    if (error_stream.is_open()) {
        error_stream.close();
    }
    
    initialized = false;
}

// ============================================
// LOGGING
// ============================================

void Logger::log(Level level, const std::string& message,
                const std::string& file, int line, const std::string& function) {
    Entry entry;
    entry.level = level;
    entry.component = config.component;
    entry.message = message;
    entry.file = file;
    entry.line = line;
    entry.function = function;
    entry.timestamp = std::chrono::system_clock::now();
    entry.thread_id = getThreadId();
    
    logInternal(entry);
}

void Logger::log(Level level, const std::string& component,
                const std::string& message, const std::string& file,
                int line, const std::string& function) {
    Entry entry;
    entry.level = level;
    entry.component = component;
    entry.message = message;
    entry.file = file;
    entry.line = line;
    entry.function = function;
    entry.timestamp = std::chrono::system_clock::now();
    entry.thread_id = getThreadId();
    
    logInternal(entry);
}

void Logger::trace(const std::string& message, const std::string& file,
                  int line, const std::string& function) {
    log(Level::TRACE, message, file, line, function);
}

void Logger::debug(const std::string& message, const std::string& file,
                  int line, const std::string& function) {
    log(Level::DEBUG, message, file, line, function);
}

void Logger::info(const std::string& message, const std::string& file,
                 int line, const std::string& function) {
    log(Level::INFO, message, file, line, function);
}

void Logger::success(const std::string& message, const std::string& file,
                    int line, const std::string& function) {
    log(Level::SUCCESS, message, file, line, function);
}

void Logger::warning(const std::string& message, const std::string& file,
                    int line, const std::string& function) {
    log(Level::WARNING, message, file, line, function);
}

void Logger::error(const std::string& message, const std::string& file,
                  int line, const std::string& function) {
    log(Level::ERROR, message, file, line, function);
}

void Logger::critical(const std::string& message, const std::string& file,
                     int line, const std::string& function) {
    log(Level::CRITICAL, message, file, line, function);
}

void Logger::logf(Level level, const char* format, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(level, std::string(buffer));
}

void Logger::logExtra(Level level, const std::string& message,
                     const std::map<std::string, std::string>& extra,
                     const std::string& file, int line,
                     const std::string& function) {
    Entry entry;
    entry.level = level;
    entry.component = config.component;
    entry.message = message;
    entry.extra = extra;
    entry.file = file;
    entry.line = line;
    entry.function = function;
    entry.timestamp = std::chrono::system_clock::now();
    entry.thread_id = getThreadId();
    
    logInternal(entry);
}

// ============================================
// COMPONENT LOGGING
// ============================================

Logger::ComponentLogger::ComponentLogger(Logger& logger, const std::string& name)
    : logger(logger), name(name) {}

void Logger::ComponentLogger::trace(const std::string& message) {
    logger.log(Level::TRACE, name, message);
}

void Logger::ComponentLogger::debug(const std::string& message) {
    logger.log(Level::DEBUG, name, message);
}

void Logger::ComponentLogger::info(const std::string& message) {
    logger.log(Level::INFO, name, message);
}

void Logger::ComponentLogger::success(const std::string& message) {
    logger.log(Level::SUCCESS, name, message);
}

void Logger::ComponentLogger::warning(const std::string& message) {
    logger.log(Level::WARNING, name, message);
}

void Logger::ComponentLogger::error(const std::string& message) {
    logger.log(Level::ERROR, name, message);
}

void Logger::ComponentLogger::critical(const std::string& message) {
    logger.log(Level::CRITICAL, name, message);
}

Logger::ComponentLogger Logger::getComponent(const std::string& name) {
    return ComponentLogger(*this, name);
}

// ============================================
// CONFIGURATION
// ============================================

void Logger::setLevel(Level level) {
    std::lock_guard<std::mutex> lock(mutex);
    config.level = level;
}

void Logger::setOutput(Output output) {
    std::lock_guard<std::mutex> lock(mutex);
    config.output = output;
    config.console_enabled = (output == Output::CONSOLE || output == Output::BOTH);
    config.file_enabled = (output == Output::FILE || output == Output::BOTH);
}

void Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    config.log_file = path;
    if (log_stream.is_open()) {
        log_stream.close();
    }
    log_stream.open(path, std::ios::app);
}

void Logger::setComponent(const std::string& component) {
    std::lock_guard<std::mutex> lock(mutex);
    config.component = component;
}

void Logger::setColorEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(mutex);
    config.color_enabled = enable;
}

void Logger::setConsoleEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(mutex);
    config.console_enabled = enable;
}

void Logger::setFileEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(mutex);
    config.file_enabled = enable;
}

// ============================================
// CALLBACKS
// ============================================

void Logger::setOnLog(LogCallback callback) {
    on_log = callback;
}

void Logger::setOnError(ErrorCallback callback) {
    on_error = callback;
}

// ============================================
// PRIVATE METHODS
// ============================================

void Logger::logInternal(const Entry& entry) {
    if (!initialized) {
        std::cerr << "Logger not initialized: " << entry.message << std::endl;
        return;
    }
    
    if (!shouldLog(entry)) {
        return;
    }
    
    // Callback
    if (on_log) {
        on_log(entry);
    }
    
    // Error callback for errors
    if (entry.level >= Level::ERROR && on_error) {
        on_error(entry);
    }
    
    // Write to console
    if (config.console_enabled) {
        writeToConsole(entry);
    }
    
    // Write to file
    if (config.file_enabled) {
        writeToFile(entry);
    }
    
    // Write to error file for errors
    if (entry.level >= Level::ERROR && error_stream.is_open()) {
        writeToError(entry);
    }
}

void Logger::writeToConsole(const Entry& entry) {
    std::string formatted = formatEntry(entry);
    
    if (config.color_enabled) {
        std::string color = getLevelColor(entry.level);
        std::string reset = "\033[0m";
        std::cout << color << formatted << reset << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }
}

void Logger::writeToFile(const Entry& entry) {
    if (!log_stream.is_open()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(file_mutex);
    std::string formatted = formatEntry(entry);
    log_stream << formatted << std::endl;
    current_file_size += formatted.length() + 1;
    
    checkAndRotate();
}

void Logger::writeToError(const Entry& entry) {
    if (!error_stream.is_open()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(file_mutex);
    std::string formatted = formatEntry(entry);
    error_stream << formatted << std::endl;
}

std::string Logger::formatEntry(const Entry& entry) const {
    switch (config.format) {
        case Format::JSON:
            return formatJSON(entry);
        case Format::SYSLOG:
            return formatSyslog(entry);
        case Format::TEXT:
        default:
            return formatText(entry);
    }
}

std::string Logger::formatText(const Entry& entry) const {
    std::stringstream ss;
    
    // Timestamp
    if (config.timestamp_enabled) {
        ss << "[" << getTimestamp(entry.timestamp) << "] ";
    }
    
    // Level
    ss << "[" << levelToString(entry.level) << "] ";
    
    // Component
    if (config.component_enabled && !entry.component.empty()) {
        ss << "[" << entry.component << "] ";
    }
    
    // Thread ID
    if (config.thread_id_enabled && !entry.thread_id.empty()) {
        ss << "[" << entry.thread_id << "] ";
    }
    
    // File and line
    if (!entry.file.empty()) {
        ss << entry.file;
        if (entry.line > 0) {
            ss << ":" << entry.line;
        }
        if (!entry.function.empty()) {
            ss << " in " << entry.function;
        }
        ss << " - ";
    }
    
    // Message
    ss << entry.message;
    
    // Extra fields
    if (!entry.extra.empty()) {
        ss << " {";
        bool first = true;
        for (const auto& [key, value] : entry.extra) {
            if (!first) ss << ", ";
            ss << key << "=" << value;
            first = false;
        }
        ss << "}";
    }
    
    return ss.str();
}

std::string Logger::formatJSON(const Entry& entry) const {
    std::stringstream ss;
    ss << "{";
    ss << "\"timestamp\":\"" << getTimestamp(entry.timestamp) << "\",";
    ss << "\"level\":\"" << levelToString(entry.level) << "\",";
    ss << "\"component\":\"" << escapeJSON(entry.component) << "\",";
    ss << "\"message\":\"" << escapeJSON(entry.message) << "\"";
    
    if (!entry.file.empty()) {
        ss << ",\"file\":\"" << escapeJSON(entry.file) << "\"";
    }
    if (entry.line > 0) {
        ss << ",\"line\":" << entry.line;
    }
    if (!entry.function.empty()) {
        ss << ",\"function\":\"" << escapeJSON(entry.function) << "\"";
    }
    if (!entry.thread_id.empty()) {
        ss << ",\"thread_id\":\"" << entry.thread_id << "\"";
    }
    if (!entry.extra.empty()) {
        ss << ",\"extra\":{";
        bool first = true;
        for (const auto& [key, value] : entry.extra) {
            if (!first) ss << ",";
            ss << "\"" << escapeJSON(key) << "\":\"" << escapeJSON(value) << "\"";
            first = false;
        }
        ss << "}";
    }
    ss << "}";
    return ss.str();
}

std::string Logger::formatSyslog(const Entry& entry) const {
    // RFC 5424 format
    std::stringstream ss;
    ss << "<" << (static_cast<int>(entry.level) * 8 + 3) << ">";
    ss << "1 ";
    ss << getTimestamp(entry.timestamp) << " ";
    ss << "recording-station ";
    ss << entry.component << " ";
    ss << "- - ";
    ss << escapeJSON(entry.message);
    return ss.str();
}

std::string Logger::getTimestamp(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()).count() % 1000;
    
    char buffer[64];
    struct tm* tm_info = localtime(&time_t);
    strftime(buffer, sizeof(buffer), config.time_format.c_str(), tm_info);
    
    std::stringstream ss;
    ss << buffer << "." << std::setw(3) << std::setfill('0') << ms;
    return ss.str();
}

std::string Logger::getThreadId() const {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

bool Logger::shouldLog(const Entry& entry) const {
    return static_cast<int>(entry.level) >= static_cast<int>(config.level);
}

std::string Logger::escapeJSON(const std::string& str) const {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                    result += buffer;
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

void Logger::checkAndRotate() {
    if (current_file_size >= config.max_file_size) {
        rotateLogFile();
    }
}

void Logger::rotateLogFile() {
    log_stream.close();
    
    // Rename existing log files
    for (int i = config.max_files - 1; i >= 1; i--) {
        std::string old_name = config.log_file + "." + std::to_string(i);
        std::string new_name = config.log_file + "." + std::to_string(i + 1);
        rename(old_name.c_str(), new_name.c_str());
    }
    
    std::string new_name = config.log_file + ".1";
    rename(config.log_file.c_str(), new_name.c_str());
    
    // Open new log file
    log_stream.open(config.log_file, std::ios::app);
    current_file_size = 0;
}

// ============================================
// UTILITY
// ============================================

std::string Logger::levelToString(Level level) {
    auto it = level_names.find(level);
    return it != level_names.end() ? it->second : "UNKNOWN";
}

Logger::Level Logger::stringToLevel(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    for (const auto& [level, name] : level_names) {
        if (name == upper) {
            return level;
        }
    }
    return Level::INFO;
}

std::string Logger::getLevelColor(Level level) {
    auto it = level_colors.find(level);
    return it != level_colors.end() ? it->second : "\033[0m";
}

std::string Logger::getLevelIcon(Level level) {
    auto it = level_icons.find(level);
    return it != level_icons.end() ? it->second : "";
}
