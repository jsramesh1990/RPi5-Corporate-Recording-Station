#include "UARTConsole.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>

UARTConsole::UARTConsole() : running(false), echo_enabled(true), prompt_enabled(true) {
}

UARTConsole::~UARTConsole() {
    disable();
}

bool UARTConsole::initialize(const std::string& device, int baud_rate) {
    PL011UART::UARTConfig config;
    config.device = device;
    config.baud_rate = baud_rate;
    return uart.initialize(config);
}

bool UARTConsole::initializeDirect(uint32_t base_addr, int baud_rate) {
    return uart.initializeDirect(base_addr, baud_rate);
}

void UARTConsole::enable() {
    if (running) {
        return;
    }
    
    running = true;
    command_processing = true;
    read_thread = std::thread(&UARTConsole::readLoop, this);
    process_thread = std::thread(&UARTConsole::processLoop, this);
    
    // Set callbacks
    uart.setOnError([this](const std::string& error) {
        writeLine("ERROR: " + error);
    });
    
    writeLine("UART Console enabled");
    writeLine("Type 'help' for available commands");
    writePrompt();
}

void UARTConsole::disable() {
    if (!running) {
        return;
    }
    
    running = false;
    command_processing = false;
    
    if (read_thread.joinable()) {
        read_thread.join();
    }
    if (process_thread.joinable()) {
        process_thread.join();
    }
    
    uart.shutdown();
}

void UARTConsole::readLoop() {
    std::string line;
    
    while (running) {
        if (uart.dataAvailable()) {
            uint8_t c = uart.readByte();
            
            // Handle special characters
            if (c == '\r' || c == '\n') {
                if (!line.empty()) {
                    processCommand(line);
                    line.clear();
                    if (prompt_enabled) {
                        writePrompt();
                    }
                } else {
                    if (prompt_enabled) {
                        writePrompt();
                    }
                }
            } else if (c == 0x7F || c == 0x08) { // Backspace
                if (!line.empty()) {
                    line.pop_back();
                    if (echo_enabled) {
                        writeString("\b \b");
                    }
                }
            } else if (c == 0x03) { // Ctrl+C
                line.clear();
                writeLine("\n^C");
                if (prompt_enabled) {
                    writePrompt();
                }
            } else if (isprint(c)) {
                line += c;
                if (echo_enabled) {
                    uart.writeByte(c);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void UARTConsole::processLoop() {
    while (command_processing) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void UARTConsole::processCommand(const std::string& command) {
    if (!command_handler) {
        writeLine("No command handler registered");
        return;
    }
    
    std::string response = command_handler(command);
    if (!response.empty()) {
        writeLine(response);
    }
}

void UARTConsole::writeString(const std::string& str) {
    uart.writeData(str);
}

void UARTConsole::writeLine(const std::string& line) {
    uart.writeLine(line);
}

void UARTConsole::writePrompt() {
    if (prompt_enabled) {
        writeString(prompt);
    }
}

void UARTConsole::setCommandHandler(CommandHandler handler) {
    command_handler = handler;
}

void UARTConsole::setPrompt(const std::string& prompt_str) {
    prompt = prompt_str;
}

void UARTConsole::enableEcho(bool enable) {
    echo_enabled = enable;
}

void UARTConsole::enablePrompt(bool enable) {
    prompt_enabled = enable;
}

bool UARTConsole::isEnabled() const {
    return running;
}

PL011UART& UARTConsole::getUART() {
    return uart;
}

void UARTConsole::clearScreen() {
    writeString("\033[2J\033[H");
}

void UARTConsole::setColor(TextColor color) {
    switch (color) {
        case TextColor::RESET: writeString("\033[0m"); break;
        case TextColor::BLACK: writeString("\033[30m"); break;
        case TextColor::RED: writeString("\033[31m"); break;
        case TextColor::GREEN: writeString("\033[32m"); break;
        case TextColor::YELLOW: writeString("\033[33m"); break;
        case TextColor::BLUE: writeString("\033[34m"); break;
        case TextColor::MAGENTA: writeString("\033[35m"); break;
        case TextColor::CYAN: writeString("\033[36m"); break;
        case TextColor::WHITE: writeString("\033[37m"); break;
        case TextColor::BRIGHT_BLACK: writeString("\033[90m"); break;
        case TextColor::BRIGHT_RED: writeString("\033[91m"); break;
        case TextColor::BRIGHT_GREEN: writeString("\033[92m"); break;
        case TextColor::BRIGHT_YELLOW: writeString("\033[93m"); break;
        case TextColor::BRIGHT_BLUE: writeString("\033[94m"); break;
        case TextColor::BRIGHT_MAGENTA: writeString("\033[95m"); break;
        case TextColor::BRIGHT_CYAN: writeString("\033[96m"); break;
        case TextColor::BRIGHT_WHITE: writeString("\033[97m"); break;
    }
}
