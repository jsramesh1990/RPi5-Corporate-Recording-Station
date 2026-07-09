#ifndef UART_CONSOLE_HPP
#define UART_CONSOLE_HPP

#include "PL011UART.hpp"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>

/**
 * @brief UART Console Interface
 * 
 * Provides a command-line interface over UART with
 * command handling, echo, and prompt support.
 */
class UARTConsole {
public:
    using CommandHandler = std::function<std::string(const std::string&)>;
    
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
    
    UARTConsole();
    ~UARTConsole();
    
    bool initialize(const std::string& device, int baud_rate = 115200);
    bool initializeDirect(uint32_t base_addr, int baud_rate = 115200);
    
    void enable();
    void disable();
    bool isEnabled() const;
    
    void writeString(const std::string& str);
    void writeLine(const std::string& line);
    void writePrompt();
    
    void setCommandHandler(CommandHandler handler);
    void setPrompt(const std::string& prompt);
    void enableEcho(bool enable);
    void enablePrompt(bool enable);
    
    void clearScreen();
    void setColor(TextColor color);
    
    PL011UART& getUART();
    
private:
    PL011UART uart;
    std::string prompt = "> ";
    std::string current_command;
    bool echo_enabled = true;
    bool prompt_enabled = true;
    
    std::thread read_thread;
    std::thread process_thread;
    std::atomic<bool> running;
    std::atomic<bool> command_processing;
    
    CommandHandler command_handler;
    
    void readLoop();
    void processLoop();
    void processCommand(const std::string& command);
};

#endif // UART_CONSOLE_HPP
