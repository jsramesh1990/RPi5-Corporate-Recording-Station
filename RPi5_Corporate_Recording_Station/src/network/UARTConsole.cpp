#include "UARTConsole.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdarg>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

UARTConsole::UARTConsole() 
    : running(false), processing(false), uart_fd(-1), owns_uart(false) {
    registerBuiltinCommands();
}

UARTConsole::~UARTConsole() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool UARTConsole::initialize(const std::string& device, int baud_rate) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    device_path = device;
    owns_uart = true;
    
    if (!openUART()) {
        return false;
    }
    
    if (!configureUART()) {
        closeUART();
        return false;
    }
    
    initialized = true;
    return true;
}

bool UARTConsole::initialize(int fd) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    uart_fd = fd;
    owns_uart = false;
    initialized = true;
    return true;
}

void UARTConsole::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    disable();
    
    if (owns_uart && uart_fd >= 0) {
        closeUART();
    }
    
    uart_fd = -1;
    initialized = false;
}

// ============================================
// CONSOLE CONTROL
// ============================================

void UARTConsole::enable() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || enabled) {
        return;
    }
    
    enabled = true;
    running = true;
    processing = true;
    
    read_thread = std::thread(&UARTConsole::readLoop, this);
    process_thread = std::thread(&UARTConsole::processLoop, this);
    
    writeln("\033[2J\033[H");  // Clear screen
    writeln("UART Console enabled");
    writeln("Type 'help' for available commands");
    writePrompt();
}

void UARTConsole::disable() {
    if (!enabled) {
        return;
    }
    
    enabled = false;
    running = false;
    processing = false;
    
    if (read_thread.joinable()) {
        read_thread.join();
    }
    if (process_thread.joinable()) {
        process_thread.join();
    }
}

// ============================================
// OUTPUT
// ============================================

void UARTConsole::write(const std::string& str) {
    std::lock_guard<std::mutex> lock(mutex);
    if (uart_fd < 0) {
        return;
    }
    writeData(std::vector<uint8_t>(str.begin(), str.end()));
}

void UARTConsole::writeln(const std::string& line) {
    write(line + "\r\n");
}

void UARTConsole::writef(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    write(std::string(buffer));
}

void UARTConsole::writeColored(const std::string& text, TextColor color) {
    write(getColorCode(color));
    write(text);
    write(getColorCode(TextColor::RESET));
}

void UARTConsole::log(LogLevel level, const std::string& message) {
    if (level < log_level) {
        return;
    }
    
    std::string timestamp = "[" + getLogLevelName(level) + "] ";
    TextColor color = getLogLevelColor(level);
    
    writeColored(timestamp, color);
    writeln(message);
    notifyLog(level, message);
}

void UARTConsole::clearScreen() {
    write("\033[2J\033[H");
}

void UARTConsole::writePrompt() {
    if (prompt_enabled) {
        write(prompt);
    }
}

// ============================================
// CONFIGURATION
// ============================================

void UARTConsole::setPrompt(const std::string& prompt_str) {
    prompt = prompt_str;
}

void UARTConsole::setEcho(bool enable) {
    echo_enabled = enable;
}

void UARTConsole::setPromptEnabled(bool enable) {
    prompt_enabled = enable;
}

void UARTConsole::setLogLevel(LogLevel level) {
    log_level = level;
}

// ============================================
// COMMAND MANAGEMENT
// ============================================

void UARTConsole::registerCommand(const Command& cmd) {
    std::lock_guard<std::mutex> lock(cmd_mutex);
    commands[cmd.name] = cmd;
}

void UARTConsole::registerCommand(const std::string& name, 
                                 const std::string& description,
                                 CommandHandler handler) {
    Command cmd;
    cmd.name = name;
    cmd.description = description;
    cmd.handler = handler;
    registerCommand(cmd);
}

void UARTConsole::unregisterCommand(const std::string& name) {
    std::lock_guard<std::mutex> lock(cmd_mutex);
    commands.erase(name);
}

std::vector<std::string> UARTConsole::getCommands() const {
    std::lock_guard<std::mutex> lock(cmd_mutex);
    std::vector<std::string> names;
    for (const auto& [name, cmd] : commands) {
        names.push_back(name);
    }
    return names;
}

std::string UARTConsole::getCommandHelp(const std::string& name) const {
    std::lock_guard<std::mutex> lock(cmd_mutex);
    auto it = commands.find(name);
    if (it != commands.end()) {
        std::string help = it->second.name;
        if (!it->second.usage.empty()) {
            help += " " + it->second.usage;
        }
        help += " - " + it->second.description;
        return help;
    }
    return "Command not found: " + name;
}

std::string UARTConsole::getAllHelp() const {
    std::lock_guard<std::mutex> lock(cmd_mutex);
    std::string help = "Available commands:\r\n";
    for (const auto& [name, cmd] : commands) {
        help += "  " + name;
        if (!cmd.usage.empty()) {
            help += " " + cmd.usage;
        }
        help += " - " + cmd.description + "\r\n";
    }
    return help;
}

// ============================================
// PRIVATE METHODS
// ============================================

bool UARTConsole::openUART() {
    uart_fd = open(device_path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (uart_fd < 0) {
        std::cerr << "Failed to open UART device: " << device_path << std::endl;
        return false;
    }
    return true;
}

void UARTConsole::closeUART() {
    if (uart_fd >= 0) {
        close(uart_fd);
        uart_fd = -1;
    }
}

bool UARTConsole::configureUART() {
    struct termios options;
    
    if (tcgetattr(uart_fd, &options) < 0) {
        std::cerr << "Failed to get UART attributes" << std::endl;
        return false;
    }
    
    // Set baud rate (115200)
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    
    // 8N1 configuration
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag |= CREAD | CLOCAL;
    
    // Disable flow control
    options.c_cflag &= ~CRTSCTS;
    
    // Raw input
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    options.c_oflag &= ~OPOST;
    
    // Set read timeout
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;
    
    if (tcsetattr(uart_fd, TCSANOW, &options) < 0) {
        std::cerr << "Failed to set UART attributes" << std::endl;
        return false;
    }
    
    return true;
}

void UARTConsole::writeData(const std::vector<uint8_t>& data) {
    if (uart_fd < 0) {
        return;
    }
    write(uart_fd, data.data(), data.size());
}

std::vector<uint8_t> UARTConsole::readData(size_t length) {
    std::vector<uint8_t> data;
    if (uart_fd < 0) {
        return data;
    }
    
    data.resize(length);
    ssize_t read_bytes = read(uart_fd, data.data(), length);
    if (read_bytes < 0) {
        return std::vector<uint8_t>();
    }
    data.resize(read_bytes);
    return data;
}

bool UARTConsole::dataAvailable() const {
    if (uart_fd < 0) {
        return false;
    }
    int bytes_avail;
    if (ioctl(uart_fd, FIONREAD, &bytes_avail) < 0) {
        return false;
    }
    return bytes_avail > 0;
}

void UARTConsole::readLoop() {
    std::string line;
    
    while (running && enabled) {
        if (dataAvailable()) {
            auto data = readData(1);
            if (data.empty()) {
                continue;
            }
            
            char c = data[0];
            
            // Handle special characters
            if (c == '\r' || c == '\n') {
                if (!line.empty()) {
                    writeln("");
                    if (on_command) {
                        on_command(line);
                    }
                    // Process command in process thread
                    processCommand(line);
                    line.clear();
                    if (prompt_enabled && enabled) {
                        writePrompt();
                    }
                } else {
                    if (prompt_enabled && enabled) {
                        writePrompt();
                    }
                }
            } else if (c == 0x7F || c == 0x08) { // Backspace
                if (!line.empty()) {
                    line.pop_back();
                    if (echo_enabled) {
                        write("\b \b");
                    }
                }
            } else if (c == 0x03) { // Ctrl+C
                line.clear();
                writeln("^C");
                if (prompt_enabled && enabled) {
                    writePrompt();
                }
            } else if (c == 0x0C) { // Ctrl+L (clear screen)
                clearScreen();
                if (prompt_enabled && enabled) {
                    writePrompt();
                }
            } else if (c >= 0x20 && c <= 0x7E) {
                line += c;
                if (echo_enabled) {
                    write(std::string(1, c));
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void UARTConsole::processLoop() {
    while (processing && enabled) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void UARTConsole::processCommand(const std::string& cmd_line) {
    auto args = parseCommand(cmd_line);
    if (args.empty()) {
        return;
    }
    
    std::string cmd_name = args[0];
    std::vector<std::string> cmd_args;
    if (args.size() > 1) {
        cmd_args.assign(args.begin() + 1, args.end());
    }
    
    // Check if it's a built-in command
    if (cmd_name[0] == '#') {
        handleBuiltinCommands(cmd_name.substr(1), cmd_args);
        return;
    }
    
    // Check registered commands
    std::lock_guard<std::mutex> lock(cmd_mutex);
    auto it = commands.find(cmd_name);
    if (it != commands.end()) {
        try {
            std::string result = it->second.handler(cmd_args);
            if (!result.empty()) {
                writeln(result);
            }
        } catch (const std::exception& e) {
            writeln("Error: " + std::string(e.what()));
        }
    } else {
        writeln("Unknown command: " + cmd_name + ". Type 'help' for available commands.");
    }
}

std::vector<std::string> UARTConsole::parseCommand(const std::string& line) {
    std::vector<std::string> args;
    std::string current;
    bool in_quotes = false;
    
    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        args.push_back(current);
    }
    
    return args;
}

std::string UARTConsole::executeCommand(const std::string& name, 
                                       const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(cmd_mutex);
    auto it = commands.find(name);
    if (it != commands.end()) {
        return it->second.handler(args);
    }
    return "Command not found: " + name;
}

void UARTConsole::handleBuiltinCommands(const std::string& cmd, 
                                       const std::vector<std::string>& args) {
    if (cmd == "help") {
        writeln(getAllHelp());
    } else if (cmd == "clear") {
        clearScreen();
    } else if (cmd == "echo") {
        std::string result;
        for (const auto& arg : args) {
            if (!result.empty()) result += " ";
            result += arg;
        }
        writeln(result);
    } else if (cmd == "prompt") {
        if (!args.empty()) {
            setPrompt(args[0]);
        }
        writeln("Prompt: " + prompt);
    } else if (cmd == "loglevel") {
        if (!args.empty()) {
            std::string level = args[0];
            if (level == "debug") setLogLevel(LogLevel::DEBUG);
            else if (level == "info") setLogLevel(LogLevel::INFO);
            else if (level == "warning") setLogLevel(LogLevel::WARNING);
            else if (level == "error") setLogLevel(LogLevel::ERROR);
            else if (level == "critical") setLogLevel(LogLevel::CRITICAL);
            else writeln("Invalid log level: " + level);
        }
        writeln("Log level: " + getLogLevelName(log_level));
    } else if (cmd == "status") {
        writeln("UART Console Status:");
        writeln("  Enabled: " + std::string(enabled ? "Yes" : "No"));
        writeln("  Echo: " + std::string(echo_enabled ? "On" : "Off"));
        writeln("  Prompt: " + prompt);
        writeln("  Log Level: " + getLogLevelName(log_level));
        writeln("  Commands Registered: " + std::to_string(commands.size()));
    } else {
        writeln("Unknown built-in command: " + cmd);
    }
}

// ============================================
// BUILT-IN COMMANDS
// ============================================

void UARTConsole::registerBuiltinCommands() {
    // Built-in commands are handled by handleBuiltinCommands
    // But we register them for help display
    
    Command help_cmd;
    help_cmd.name = "help";
    help_cmd.description = "Show available commands";
    help_cmd.handler = [this](const std::vector<std::string>&) -> std::string {
        return getAllHelp();
    };
    registerCommand(help_cmd);
    
    Command clear_cmd;
    clear_cmd.name = "clear";
    clear_cmd.description = "Clear screen";
    clear_cmd.handler = [this](const std::vector<std::string>&) -> std::string {
        clearScreen();
        return "";
    };
    registerCommand(clear_cmd);
    
    Command echo_cmd;
    echo_cmd.name = "echo";
    echo_cmd.usage = "<text>";
    echo_cmd.description = "Echo text";
    echo_cmd.handler = [this](const std::vector<std::string>& args) -> std::string {
        std::string result;
        for (const auto& arg : args) {
            if (!result.empty()) result += " ";
            result += arg;
        }
        return result;
    };
    registerCommand(echo_cmd);
    
    Command prompt_cmd;
    prompt_cmd.name = "prompt";
    prompt_cmd.usage = "[new_prompt]";
    prompt_cmd.description = "Set or show prompt";
    prompt_cmd.handler = [this](const std::vector<std::string>& args) -> std::string {
        if (!args.empty()) {
            setPrompt(args[0]);
        }
        return "Prompt: " + prompt;
    };
    registerCommand(prompt_cmd);
    
    Command loglevel_cmd;
    loglevel_cmd.name = "loglevel";
    loglevel_cmd.usage = "[debug|info|warning|error|critical]";
    loglevel_cmd.description = "Set or show log level";
    loglevel_cmd.handler = [this](const std::vector<std::string>& args) -> std::string {
        if (!args.empty()) {
            std::string level = args[0];
            if (level == "debug") setLogLevel(LogLevel::DEBUG);
            else if (level == "info") setLogLevel(LogLevel::INFO);
            else if (level == "warning") setLogLevel(LogLevel::WARNING);
            else if (level == "error") setLogLevel(LogLevel::ERROR);
            else if (level == "critical") setLogLevel(LogLevel::CRITICAL);
            else return "Invalid log level: " + level;
        }
        return "Log level: " + getLogLevelName(log_level);
    };
    registerCommand(loglevel_cmd);
    
    Command status_cmd;
    status_cmd.name = "status";
    status_cmd.description = "Show console status";
    status_cmd.handler = [this](const std::vector<std::string>&) -> std::string {
        std::stringstream ss;
        ss << "UART Console Status:\n";
        ss << "  Enabled: " << (enabled ? "Yes" : "No") << "\n";
        ss << "  Echo: " << (echo_enabled ? "On" : "Off") << "\n";
        ss << "  Prompt: " << prompt << "\n";
        ss << "  Log Level: " << getLogLevelName(log_level) << "\n";
        ss << "  Commands Registered: " << commands.size();
        return ss.str();
    };
    registerCommand(status_cmd);
}

// ============================================
// CALLBACKS
// ============================================

void UARTConsole::setOnLog(LogCallback callback) {
    on_log = callback;
}

void UARTConsole::setOnCommand(std::function<void(const std::string&)> callback) {
    on_command = callback;
}

void UARTConsole::notifyLog(LogLevel level, const std::string& message) {
    if (on_log) {
        on_log(level, message);
    }
}

// ============================================
// UTILITY
// ============================================

std::string UARTConsole::getColorCode(TextColor color) const {
    switch (color) {
        case TextColor::RESET: return "\033[0m";
        case TextColor::BLACK: return "\033[30m";
        case TextColor::RED: return "\033[31m";
        case TextColor::GREEN: return "\033[32m";
        case TextColor::YELLOW: return "\033[33m";
        case TextColor::BLUE: return "\033[34m";
        case TextColor::MAGENTA: return "\033[35m";
        case TextColor::CYAN: return "\033[36m";
        case TextColor::WHITE: return "\033[37m";
        case TextColor::BRIGHT_BLACK: return "\033[90m";
        case TextColor::BRIGHT_RED: return "\033[91m";
        case TextColor::BRIGHT_GREEN: return "\033[92m";
        case TextColor::BRIGHT_YELLOW: return "\033[93m";
        case TextColor::BRIGHT_BLUE: return "\033[94m";
        case TextColor::BRIGHT_MAGENTA: return "\033[95m";
        case TextColor::BRIGHT_CYAN: return "\033[96m";
        case TextColor::BRIGHT_WHITE: return "\033[97m";
        default: return "";
    }
}

std::string UARTConsole::getLogLevelName(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

UARTConsole::TextColor UARTConsole::getLogLevelColor(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return TextColor::BRIGHT_CYAN;
        case LogLevel::INFO: return TextColor::BRIGHT_GREEN;
        case LogLevel::WARNING: return TextColor::BRIGHT_YELLOW;
        case LogLevel::ERROR: return TextColor::BRIGHT_RED;
        case LogLevel::CRITICAL: return TextColor::RED;
        default: return TextColor::RESET;
    }
}
