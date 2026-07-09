#include "ConsoleUI.hpp"

#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>

// Color codes
const std::map<ConsoleUI::Color, std::string> ConsoleUI::color_codes = {
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

const std::map<ConsoleUI::Style, std::string> ConsoleUI::style_codes = {
    {Style::NORMAL, "\033[0m"},
    {Style::BOLD, "\033[1m"},
    {Style::DIM, "\033[2m"},
    {Style::ITALIC, "\033[3m"},
    {Style::UNDERLINE, "\033[4m"},
    {Style::BLINK, "\033[5m"},
    {Style::REVERSE, "\033[7m"},
    {Style::HIDDEN, "\033[8m"},
    {Style::STRIKE, "\033[9m"}
};

const std::map<ConsoleUI::LogLevel, ConsoleUI::Color> ConsoleUI::log_colors = {
    {LogLevel::DEBUG, Color::BRIGHT_CYAN},
    {LogLevel::INFO, Color::BRIGHT_WHITE},
    {LogLevel::SUCCESS, Color::BRIGHT_GREEN},
    {LogLevel::WARNING, Color::BRIGHT_YELLOW},
    {LogLevel::ERROR, Color::BRIGHT_RED},
    {LogLevel::CRITICAL, Color::RED}
};

const std::map<ConsoleUI::LogLevel, std::string> ConsoleUI::log_names = {
    {LogLevel::DEBUG, "DEBUG"},
    {LogLevel::INFO, "INFO"},
    {LogLevel::SUCCESS, "SUCCESS"},
    {LogLevel::WARNING, "WARNING"},
    {LogLevel::ERROR, "ERROR"},
    {LogLevel::CRITICAL, "CRITICAL"}
};

// Spinner frames
const std::vector<std::string> ConsoleUI::spinner_frames = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};

const std::vector<std::string> ConsoleUI::indeterminate_frames = {
    "|", "/", "-", "\\"
};

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

ConsoleUI::ConsoleUI() : auto_refresh(false), refresh_interval(1000) {
    color_supported = isColorTerminal();
    getTerminalSizeImpl();
}

ConsoleUI::~ConsoleUI() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool ConsoleUI::initialize(const std::string& app_title) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    title = app_title;
    getTerminalSizeImpl();
    
    // Set up terminal
    if (color_supported) {
        // Enable colors
        write("\033[0m");
    }
    
    initialized = true;
    
    // Clear screen and show header
    clearScreen();
    drawHeader(title);
    
    return true;
}

void ConsoleUI::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    stopAutoRefresh();
    
    // Restore cursor and colors
    showCursor();
    write(getColorCode(Color::RESET));
    writeln();
    
    initialized = false;
}

// ============================================
// OUTPUT
// ============================================

void ConsoleUI::write(const std::string& text) {
    std::cout << text << std::flush;
}

void ConsoleUI::writeln(const std::string& text) {
    write(text + "\n");
}

void ConsoleUI::writef(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    write(std::string(buffer));
}

void ConsoleUI::writeColored(const std::string& text, Color color, Style style) {
    write(getColorCode(color));
    write(getStyleCode(style));
    write(text);
    write(getColorCode(Color::RESET));
}

void ConsoleUI::writelnColored(const std::string& text, Color color, Style style) {
    writeColored(text, color, style);
    writeln();
}

void ConsoleUI::writeStyled(const std::string& text, Style style) {
    write(getStyleCode(style));
    write(text);
    write(getStyleCode(Style::NORMAL));
}

void ConsoleUI::clearScreen() {
    write("\033[2J\033[H");
}

void ConsoleUI::clearLine() {
    write("\033[2K");
}

void ConsoleUI::moveCursor(int row, int col) {
    writef("\033[%d;%dH", row + 1, col + 1);
}

void ConsoleUI::saveCursor() {
    write("\033[s");
}

void ConsoleUI::restoreCursor() {
    write("\033[u");
}

void ConsoleUI::hideCursor() {
    write("\033[?25l");
}

void ConsoleUI::showCursor() {
    write("\033[?25h");
}

// ============================================
// LOGGING
// ============================================

void ConsoleUI::log(LogLevel level, const std::string& message) {
    std::string timestamp = formatTimestamp();
    Color color = getLogLevelColor(level);
    std::string level_name = getLogLevelName(level);
    
    writeColored("[" + timestamp + "] ", Color::BRIGHT_BLACK);
    writeColored("[" + level_name + "] ", color, Style::BOLD);
    writeln(message);
}

void ConsoleUI::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void ConsoleUI::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void ConsoleUI::success(const std::string& message) {
    log(LogLevel::SUCCESS, message);
}

void ConsoleUI::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void ConsoleUI::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void ConsoleUI::critical(const std::string& message) {
    log(LogLevel::CRITICAL, message);
}

std::string ConsoleUI::getLogLevelName(LogLevel level) const {
    auto it = log_names.find(level);
    return it != log_names.end() ? it->second : "UNKNOWN";
}

ConsoleUI::Color ConsoleUI::getLogLevelColor(LogLevel level) const {
    auto it = log_colors.find(level);
    return it != log_colors.end() ? it->second : Color::WHITE;
}

// ============================================
// PROGRESS BARS
// ============================================

void ConsoleUI::drawProgressBar(float progress, const std::string& label,
                               const ProgressStyle& style) {
    progress = std::max(0.0f, std::min(1.0f, progress));
    int filled = static_cast<int>(progress * style.width);
    int empty = style.width - filled;
    
    std::string bar;
    bar += style.left_bracket;
    bar += std::string(filled, style.fill_char);
    bar += std::string(empty, style.empty_char);
    bar += style.right_bracket;
    
    if (style.show_percentage) {
        int percent = static_cast<int>(progress * 100);
        bar += " " + std::to_string(percent) + "%";
    }
    
    if (style.show_value) {
        bar += " " + std::to_string(static_cast<int>(progress * 100)) + "%";
    }
    
    if (!label.empty()) {
        bar = label + " " + bar;
    }
    
    // Clear line and write
    clearLine();
    write("\r");
    write(bar);
    write("\r");
}

void ConsoleUI::drawSpinner(const std::string& label, int frame) {
    if (frame < 0) {
        frame = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 100 % spinner_frames.size();
    }
    int idx = frame % spinner_frames.size();
    
    clearLine();
    write("\r");
    write(spinner_frames[idx]);
    if (!label.empty()) {
        write(" " + label);
    }
    write("\r");
}

void ConsoleUI::drawIndeterminate(const std::string& label, int frame) {
    if (frame < 0) {
        frame = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 100 % indeterminate_frames.size();
    }
    int idx = frame % indeterminate_frames.size();
    
    clearLine();
    write("\r");
    write(indeterminate_frames[idx]);
    if (!label.empty()) {
        write(" " + label);
    }
    write("\r");
}

// ============================================
// TABLES
// ============================================

void ConsoleUI::drawTable(const std::vector<TableColumn>& columns,
                         const std::vector<std::map<std::string, std::string>>& rows) {
    if (columns.empty() || rows.empty()) {
        return;
    }
    
    auto widths = calculateColumnWidths(columns, rows);
    std::vector<Alignment> alignments;
    for (const auto& col : columns) {
        alignments.push_back(col.alignment);
    }
    
    // Draw header
    std::vector<std::string> header;
    for (const auto& col : columns) {
        header.push_back(col.header);
    }
    std::string header_line = formatTableRow(header, widths, alignments);
    writelnColored(header_line, Color::BRIGHT_CYAN, Style::BOLD);
    
    // Draw separator
    std::string sep;
    for (int w : widths) {
        sep += std::string(w, '─');
        sep += " ";
    }
    writelnColored(sep, Color::BRIGHT_BLACK);
    
    // Draw rows
    for (const auto& row : rows) {
        std::vector<std::string> cells;
        for (const auto& col : columns) {
            if (col.getter) {
                cells.push_back(col.getter(row));
            } else {
                auto it = row.find(col.header);
                if (it != row.end()) {
                    cells.push_back(it->second);
                } else {
                    cells.push_back("");
                }
            }
        }
        writeln(formatTableRow(cells, widths, alignments));
    }
}

void ConsoleUI::drawSimpleTable(const std::vector<std::string>& headers,
                               const std::vector<std::vector<std::string>>& rows) {
    std::vector<TableColumn> columns;
    for (const auto& header : headers) {
        TableColumn col;
        col.header = header;
        col.alignment = Alignment::LEFT;
        columns.push_back(col);
    }
    
    std::vector<std::map<std::string, std::string>> table_rows;
    for (const auto& row : rows) {
        std::map<std::string, std::string> data;
        for (size_t i = 0; i < headers.size() && i < row.size(); i++) {
            data[headers[i]] = row[i];
        }
        table_rows.push_back(data);
    }
    
    drawTable(columns, table_rows);
}

void ConsoleUI::drawKeyValueTable(const std::map<std::string, std::string>& data,
                                 const std::string& title) {
    if (!title.empty()) {
        writelnColored(title, Color::BRIGHT_YELLOW, Style::BOLD);
        drawSeparator('-');
    }
    
    int max_key_len = 0;
    for (const auto& [key, _] : data) {
        max_key_len = std::max(max_key_len, (int)key.length());
    }
    max_key_len = std::min(max_key_len + 2, 30);
    
    for (const auto& [key, value] : data) {
        writeColored(key, Color::BRIGHT_CYAN);
        write(":");
        int padding = max_key_len - key.length() + 1;
        write(std::string(padding, ' '));
        writeln(value);
    }
}

// ============================================
// MENUS
// ============================================

std::string ConsoleUI::showMenu(const std::vector<MenuItem>& items, const std::string& title) {
    if (!title.empty()) {
        writelnColored(title, Color::BRIGHT_YELLOW, Style::BOLD);
        drawSeparator('-');
    }
    
    for (const auto& item : items) {
        if (!item.visible) continue;
        writeColored("  [" + item.key + "] ", Color::BRIGHT_CYAN);
        write(item.label);
        if (!item.description.empty()) {
            write(" - ");
            writeColored(item.description, Color::BRIGHT_BLACK);
        }
        writeln();
    }
    writeln();
    writeColored("Selection: ", Color::BRIGHT_GREEN);
    
    std::string choice;
    std::getline(std::cin, choice);
    return choice;
}

void ConsoleUI::showMenuInteractive(const std::vector<MenuItem>& items, const std::string& title) {
    while (true) {
        std::string choice = showMenu(items, title);
        if (choice.empty()) continue;
        
        bool found = false;
        for (const auto& item : items) {
            if (item.key == choice && item.enabled && item.visible) {
                if (item.action) {
                    item.action();
                }
                found = true;
                break;
            }
        }
        
        if (!found) {
            writelnColored("Invalid selection. Please try again.", Color::RED);
        }
    }
}

bool ConsoleUI::confirm(const std::string& message, bool default_yes) {
    std::string prompt = message + " (" + (default_yes ? "Y/n" : "y/N") + "): ";
    write(prompt);
    
    std::string response;
    std::getline(std::cin, response);
    
    if (response.empty()) {
        return default_yes;
    }
    
    char first = tolower(response[0]);
    return first == 'y';
}

std::string ConsoleUI::prompt(const std::string& message, const std::string& default_value) {
    std::string prompt = message;
    if (!default_value.empty()) {
        prompt += " [" + default_value + "]";
    }
    prompt += ": ";
    write(prompt);
    
    std::string response;
    std::getline(std::cin, response);
    
    return response.empty() ? default_value : response;
}

int ConsoleUI::select(const std::string& message, const std::vector<std::string>& options) {
    writelnColored(message, Color::BRIGHT_YELLOW);
    for (size_t i = 0; i < options.size(); i++) {
        writeColored("  [" + std::to_string(i + 1) + "] ", Color::BRIGHT_CYAN);
        writeln(options[i]);
    }
    writeln();
    
    while (true) {
        std::string input = prompt("Select option");
        try {
            int choice = std::stoi(input);
            if (choice >= 1 && choice <= (int)options.size()) {
                return choice - 1;
            }
        } catch (...) {
            // Invalid input
        }
        writelnColored("Invalid selection. Please try again.", Color::RED);
    }
}

// ============================================
// STATUS DISPLAY
// ============================================

void ConsoleUI::displayStatus(const std::map<std::string, std::string>& status) {
    clearScreen();
    drawHeader(title);
    
    drawKeyValueTable(status, "System Status");
    drawFooter();
}

void ConsoleUI::displayRecordingStatus(bool is_recording, const std::string& filename,
                                      int duration, int64_t size) {
    clearScreen();
    drawHeader(title);
    
    std::map<std::string, std::string> status;
    status["Status"] = is_recording ? "🔴 RECORDING" : "⏹ IDLE";
    if (is_recording) {
        status["File"] = filename;
        status["Duration"] = formatDuration(duration);
        status["Size"] = formatSize(size);
    }
    
    drawKeyValueTable(status, "Recording Status");
    drawFooter();
}

void ConsoleUI::displayStorageStatus(uint64_t total, uint64_t used, uint64_t free) {
    clearScreen();
    drawHeader(title);
    
    std::map<std::string, std::string> status;
    status["Total"] = formatSize(total);
    status["Used"] = formatSize(used);
    status["Free"] = formatSize(free);
    status["Usage"] = std::to_string((int)((float)used / total * 100)) + "%";
    
    drawKeyValueTable(status, "Storage Status");
    
    // Draw progress bar
    float progress = (float)used / total;
    drawProgressBar(progress, "Storage Usage:");
    writeln();
    
    drawFooter();
}

void ConsoleUI::displayMemoryStatus(size_t total, size_t used, size_t free) {
    clearScreen();
    drawHeader(title);
    
    std::map<std::string, std::string> status;
    status["Total"] = formatSize(total);
    status["Used"] = formatSize(used);
    status["Free"] = formatSize(free);
    status["Usage"] = std::to_string((int)((float)used / total * 100)) + "%";
    
    drawKeyValueTable(status, "Memory Status");
    
    // Draw progress bar
    float progress = (float)used / total;
    drawProgressBar(progress, "Memory Usage:");
    writeln();
    
    drawFooter();
}

// ============================================
// HEADER / FOOTER
// ============================================

void ConsoleUI::drawHeader(const std::string& header_title) {
    int width = getTerminalWidth();
    std::string title_line = " " + header_title + " ";
    int padding = (width - title_line.length()) / 2;
    
    writelnColored(std::string(width, '='), Color::BRIGHT_BLACK);
    if (padding > 0) {
        write(std::string(padding, ' '));
    }
    writelnColored(title_line, Color::BRIGHT_CYAN, Style::BOLD);
    writelnColored(std::string(width, '='), Color::BRIGHT_BLACK);
    writeln();
}

void ConsoleUI::drawFooter(const std::string& text) {
    int width = getTerminalWidth();
    if (!text.empty()) {
        write(std::string(width - text.length() - 1, ' '));
        writeColored(text, Color::BRIGHT_BLACK);
        writeln();
    }
    writelnColored(std::string(width, '='), Color::BRIGHT_BLACK);
}

void ConsoleUI::drawSeparator(char fill, int width) {
    if (width < 0) {
        width = getTerminalWidth();
    }
    writelnColored(std::string(width, fill), Color::BRIGHT_BLACK);
}

void ConsoleUI::drawBox(const std::string& title, const std::string& content, int width) {
    if (width < 0) {
        width = getTerminalWidth() - 4;
    }
    width = std::max(20, width);
    
    // Top border
    writelnColored("┌" + std::string(width - 2, '─') + "┐", Color::BRIGHT_BLACK);
    
    // Title
    std::string title_line = "│ " + title;
    title_line += std::string(width - title_line.length() - 1, ' ');
    title_line += "│";
    writelnColored(title_line, Color::BRIGHT_CYAN, Style::BOLD);
    
    // Separator
    writelnColored("├" + std::string(width - 2, '─') + "┤", Color::BRIGHT_BLACK);
    
    // Content lines
    std::stringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        std::string content_line = "│ " + line;
        content_line += std::string(width - content_line.length() - 1, ' ');
        content_line += "│";
        writelnColored(content_line, Color::WHITE);
    }
    
    // Bottom border
    writelnColored("└" + std::string(width - 2, '─') + "┘", Color::BRIGHT_BLACK);
}

// ============================================
// SCREEN MANAGEMENT
// ============================================

void ConsoleUI::enterFullscreen() {
    if (!fullscreen) {
        fullscreen = true;
        clearScreen();
        hideCursor();
    }
}

void ConsoleUI::exitFullscreen() {
    if (fullscreen) {
        fullscreen = false;
        showCursor();
        clearScreen();
    }
}

void ConsoleUI::refresh() {
    if (refresh_callback) {
        refresh_callback();
    }
}

void ConsoleUI::setRefreshInterval(int ms) {
    refresh_interval = std::max(100, ms);
}

void ConsoleUI::startAutoRefresh(UpdateCallback callback) {
    if (auto_refresh) {
        stopAutoRefresh();
    }
    
    refresh_callback = callback;
    auto_refresh = true;
    refresh_thread = std::thread(&ConsoleUI::refreshLoop, this);
}

void ConsoleUI::stopAutoRefresh() {
    if (!auto_refresh) {
        return;
    }
    
    auto_refresh = false;
    if (refresh_thread.joinable()) {
        refresh_thread.join();
    }
}

void ConsoleUI::refreshLoop() {
    while (auto_refresh) {
        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_interval));
        if (fullscreen && refresh_callback) {
            refresh_callback();
        }
    }
}

// ============================================
// UTILITY
// ============================================

std::pair<int, int> ConsoleUI::getTerminalSize() const {
    return {terminal_width, terminal_height};
}

int ConsoleUI::getTerminalWidth() const {
    return terminal_width;
}

int ConsoleUI::getTerminalHeight() const {
    return terminal_height;
}

void ConsoleUI::getTerminalSizeImpl() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        terminal_width = w.ws_col;
        terminal_height = w.ws_row;
    }
    terminal_width = std::max(20, terminal_width);
    terminal_height = std::max(10, terminal_height);
}

void ConsoleUI::updateTerminalSize() {
    getTerminalSizeImpl();
}

std::string ConsoleUI::centerText(const std::string& text, int width) {
    if (width < 0) {
        width = terminal_width;
    }
    int padding = (width - text.length()) / 2;
    if (padding < 0) padding = 0;
    return std::string(padding, ' ') + text;
}

std::string ConsoleUI::formatSize(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int i = 0;
    double size = bytes;
    while (size >= 1024 && i < 5) {
        size /= 1024;
        i++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[i];
    return ss.str();
}

std::string ConsoleUI::formatDuration(int seconds) const {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    std::stringstream ss;
    if (hours > 0) {
        ss << hours << "h ";
    }
    if (minutes > 0 || hours > 0) {
        ss << minutes << "m ";
    }
    ss << secs << "s";
    return ss.str();
}

std::string ConsoleUI::formatTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
    return std::string(buffer);
}

std::string ConsoleUI::getColorCode(Color color) const {
    auto it = color_codes.find(color);
    return it != color_codes.end() ? it->second : "";
}

std::string ConsoleUI::getStyleCode(Style style) const {
    auto it = style_codes.find(style);
    return it != style_codes.end() ? it->second : "";
}

std::string ConsoleUI::getResetCode() const {
    return getColorCode(Color::RESET);
}

bool ConsoleUI::supportsColor() const {
    return color_supported;
}

bool ConsoleUI::isColorTerminal() const {
    const char* term = getenv("TERM");
    if (!term) return false;
    
    std::string term_str(term);
    return term_str.find("xterm") != std::string::npos ||
           term_str.find("color") != std::string::npos ||
           term_str.find("ansi") != std::string::npos;
}

std::string ConsoleUI::repeatChar(char c, int count) {
    return std::string(std::max(0, count), c);
}

std::string ConsoleUI::truncateText(const std::string& text, int max_width) {
    if ((int)text.length() <= max_width) {
        return text;
    }
    return text.substr(0, max_width - 3) + "...";
}

std::string ConsoleUI::formatTableRow(const std::vector<std::string>& columns,
                                     const std::vector<int>& widths,
                                     const std::vector<Alignment>& alignments) {
    std::string result;
    for (size_t i = 0; i < columns.size(); i++) {
        std::string cell = truncateText(columns[i], widths[i]);
        int padding = widths[i] - cell.length();
        
        switch (alignments[i]) {
            case Alignment::LEFT:
                result += cell;
                result += std::string(padding, ' ');
                break;
            case Alignment::RIGHT:
                result += std::string(padding, ' ');
                result += cell;
                break;
            case Alignment::CENTER:
                result += std::string(padding / 2, ' ');
                result += cell;
                result += std::string(padding - padding / 2, ' ');
                break;
        }
        result += " ";
    }
    return result;
}

std::vector<int> ConsoleUI::calculateColumnWidths(
    const std::vector<TableColumn>& columns,
    const std::vector<std::map<std::string, std::string>>& rows) {
    
    std::vector<int> widths;
    int total_width = getTerminalWidth();
    int used_width = columns.size(); // Account for spaces between columns
    
    // Calculate maximum width for each column
    for (const auto& col : columns) {
        int max_width = col.header.length();
        for (const auto& row : rows) {
            std::string value;
            if (col.getter) {
                value = col.getter(row);
            } else {
                auto it = row.find(col.header);
                if (it != row.end()) {
                    value = it->second;
                }
            }
            max_width = std::max(max_width, (int)value.length());
        }
        widths.push_back(max_width);
        used_width += max_width;
    }
    
    // Distribute remaining width if needed
    if (used_width < total_width && !widths.empty()) {
        int extra = (total_width - used_width) / columns.size();
        for (auto& w : widths) {
            w += extra;
        }
    }
    
    return widths;
}
