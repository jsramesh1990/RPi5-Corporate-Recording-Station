#ifndef NETWORK_UART_CONSOLE_HPP
#define NETWORK_UART_CONSOLE_HPP

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>
#include <vector>
#include <mutex>

/**
 * @brief UART Console Interface for Network Operations
 * 
 * Provides a command-line interface over UART with
 * command handling, echo, and prompt support.
 * This version is integrated with NetworkManager for
 * network-related commands.
 */
class UARTConsole {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class TextColor {
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
    
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    struct Command {
        std::string name;
        std::string description;
        std::string usage;
        std::function<std::string(const std::vector<std::string>&)> handler;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using CommandHandler = std::function<std::string(const std::vector<std::string>&)>;
    using LogCallback = std::function<void(LogLevel, const std::string&)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    UARTConsole();
    ~UARTConsole();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize UART console
     * @param device UART device path
     * @param baud_rate Baud rate
     * @return true if successful
     */
    bool initialize(const std::string& device, int baud_rate = 115200);
    
    /**
     * @brief Initialize UART console with custom UART
     * @param uart_fd UART file descriptor
     * @return true if successful
     */
    bool initialize(int uart_fd);
    
    /**
     * @brief Shutdown UART console
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // CONSOLE CONTROL
    // ============================================
    
    /**
     * @brief Enable console
     */
    void enable();
    
    /**
     * @brief Disable console
     */
    void disable();
    
    /**
     * @brief Check if enabled
     */
    bool isEnabled() const { return enabled; }
    
    // ============================================
    // OUTPUT
    // ============================================
    
    /**
     * @brief Write string to console
     */
    void write(const std::string& str);
    
    /**
     * @brief Write line to console
     */
    void writeln(const std::string& line);
    
    /**
     * @brief Write formatted string
     */
    void writef(const char* format, ...);
    
    /**
     * @brief Write colored text
     */
    void writeColored(const std::string& text, TextColor color);
    
    /**
     * @brief Write log message
     */
    void log(LogLevel level, const std::string& message);
    
    /**
     * @brief Clear screen
     */
    void clearScreen();
    
    /**
     * @brief Write prompt
     */
    void writePrompt();
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Set prompt string
     */
    void setPrompt(const std::string& prompt);
    
    /**
     * @brief Enable/disable echo
     */
    void setEcho(bool enable);
    
    /**
     * @brief Enable/disable prompt
     */
    void setPromptEnabled(bool enable);
    
    /**
     * @brief Set log level
     */
    void setLogLevel(LogLevel level);
    
    // ============================================
    // COMMAND MANAGEMENT
    // ============================================
    
    /**
     * @brief Register a command
     */
    void registerCommand(const Command& cmd);
    
    /**
     * @brief Register a command with simple handler
     */
    void registerCommand(const std::string& name, 
                        const std::string& description,
                        CommandHandler handler);
    
    /**
     * @brief Unregister a command
     */
    void unregisterCommand(const std::string& name);
    
    /**
     * @brief Get list of registered commands
     */
    std::vector<std::string> getCommands() const;
    
    /**
     * @brief Get command help
     */
    std::string getCommandHelp(const std::string& name) const;
    
    /**
     * @brief Get all command help
     */
    std::string getAllHelp() const;
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnLog(LogCallback callback);
    void setOnCommand(std::function<void(const std::string&)> callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get color code
     */
    std::string getColorCode(TextColor color) const;
    
    /**
     * @brief Get log level name
     */
    std::string getLogLevelName(LogLevel level) const;
    
    /**
     * @brief Get log level color
     */
    TextColor getLogLevelColor(LogLevel level) const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    bool initialized = false;
    bool enabled = false;
    bool echo_enabled = true;
    bool prompt_enabled = true;
    LogLevel log_level = LogLevel::INFO;
    std::string prompt = "> ";
    
    int uart_fd = -1;
    std::string device_path;
    bool owns_uart = false;
    mutable std::mutex mutex;
    
    // Command registry
    std::map<std::string, Command> commands;
    mutable std::mutex cmd_mutex;
    
    // Threading
    std::thread read_thread;
    std::thread process_thread;
    std::atomic<bool> running;
    std::atomic<bool> processing;
    
    // Callbacks
    LogCallback on_log;
    std::function<void(const std::string&)> on_command;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    bool openUART();
    void closeUART();
    bool configureUART();
    void writeData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> readData(size_t length);
    bool dataAvailable() const;
    
    void readLoop();
    void processLoop();
    void processCommand(const std::string& cmd_line);
    std::string executeCommand(const std::string& name, 
                              const std::vector<std::string>& args);
    
    std::vector<std::string> parseCommand(const std::string& line);
    void handleBuiltinCommands(const std::string& cmd, 
                              const std::vector<std::string>& args);
    
    void notifyLog(LogLevel level, const std::string& message);
    
    // Built-in commands
    void registerBuiltinCommands();
    std::string cmdHelp(const std::vector<std::string>& args);
    std::string cmdClear(const std::vector<std::string>& args);
    std::string cmdEcho(const std::vector<std::string>& args);
    std::string cmdPrompt(const std::vector<std::string>& args);
    std::string cmdLogLevel(const std::vector<std::string>& args);
    std::string cmdStatus(const std::vector<std::string>& args);
};

#endif // NETWORK_UART_CONSOLE_HPP
