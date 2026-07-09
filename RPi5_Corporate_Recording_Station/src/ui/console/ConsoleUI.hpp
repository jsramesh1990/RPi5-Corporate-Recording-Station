#ifndef CONSOLE_UI_HPP
#define CONSOLE_UI_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * @brief Console UI for Recording Station
 * 
 * Provides a terminal-based user interface with:
 * - Interactive menus
 * - Real-time status display
 * - Command processing
 * - Color output
 * - Progress bars
 * - Tables and formatting
 */
class ConsoleUI {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
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
    
    enum class Style {
        NORMAL,
        BOLD,
        DIM,
        ITALIC,
        UNDERLINE,
        BLINK,
        REVERSE,
        HIDDEN,
        STRIKE
    };
    
    enum class Alignment {
        LEFT,
        CENTER,
        RIGHT
    };
    
    enum class LogLevel {
        DEBUG,
        INFO,
        SUCCESS,
        WARNING,
        ERROR,
        CRITICAL
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief Menu item
     */
    struct MenuItem {
        std::string key;
        std::string label;
        std::string description;
        std::function<void()> action;
        bool enabled = true;
        bool visible = true;
    };
    
    /**
     * @brief Table column
     */
    struct TableColumn {
        std::string header;
        int width = 0;
        Alignment alignment = Alignment::LEFT;
        std::function<std::string(const std::map<std::string, std::string>&)> getter;
    };
    
    /**
     * @brief Progress bar style
     */
    struct ProgressStyle {
        char fill_char = '#';
        char empty_char = ' ';
        char left_bracket = '[';
        char right_bracket = ']';
        int width = 40;
        bool show_percentage = true;
        bool show_value = true;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using CommandCallback = std::function<void(const std::string&)>;
    using LogCallback = std::function<void(LogLevel, const std::string&)>;
    using UpdateCallback = std::function<void()>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    ConsoleUI();
    ~ConsoleUI();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize console UI
     * @param title Application title
     * @return true if successful
     */
    bool initialize(const std::string& title = "Recording Station");
    
    /**
     * @brief Shutdown console UI
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // OUTPUT
    // ============================================
    
    /**
     * @brief Write text to console
     */
    void write(const std::string& text);
    
    /**
     * @brief Write line to console
     */
    void writeln(const std::string& text = "");
    
    /**
     * @brief Write formatted string
     */
    void writef(const char* format, ...);
    
    /**
     * @brief Write colored text
     */
    void writeColored(const std::string& text, Color color, Style style = Style::NORMAL);
    
    /**
     * @brief Write colored line
     */
    void writelnColored(const std::string& text, Color color, Style style = Style::NORMAL);
    
    /**
     * @brief Write with style
     */
    void writeStyled(const std::string& text, Style style);
    
    /**
     * @brief Clear screen
     */
    void clearScreen();
    
    /**
     * @brief Clear line
     */
    void clearLine();
    
    /**
     * @brief Move cursor to position
     */
    void moveCursor(int row, int col);
    
    /**
     * @brief Save cursor position
     */
    void saveCursor();
    
    /**
     * @brief Restore cursor position
     */
    void restoreCursor();
    
    /**
     * @brief Hide cursor
     */
    void hideCursor();
    
    /**
     * @brief Show cursor
     */
    void showCursor();
    
    // ============================================
    // LOGGING
    // ============================================
    
    /**
     * @brief Log message
     */
    void log(LogLevel level, const std::string& message);
    
    /**
     * @brief Log debug message
     */
    void debug(const std::string& message);
    
    /**
     * @brief Log info message
     */
    void info(const std::string& message);
    
    /**
     * @brief Log success message
     */
    void success(const std::string& message);
    
    /**
     * @brief Log warning message
     */
    void warning(const std::string& message);
    
    /**
     * @brief Log error message
     */
    void error(const std::string& message);
    
    /**
     * @brief Log critical message
     */
    void critical(const std::string& message);
    
    /**
     * @brief Get log level name
     */
    std::string getLogLevelName(LogLevel level) const;
    
    /**
     * @brief Get log level color
     */
    Color getLogLevelColor(LogLevel level) const;
    
    // ============================================
    // PROGRESS BARS
    // ============================================
    
    /**
     * @brief Draw progress bar
     */
    void drawProgressBar(float progress, const std::string& label = "",
                        const ProgressStyle& style = ProgressStyle());
    
    /**
     * @brief Draw spinner
     */
    void drawSpinner(const std::string& label = "", int frame = -1);
    
    /**
     * @brief Draw indeterminate progress
     */
    void drawIndeterminate(const std::string& label = "", int frame = -1);
    
    // ============================================
    // TABLES
    // ============================================
    
    /**
     * @brief Draw table
     */
    void drawTable(const std::vector<TableColumn>& columns,
                   const std::vector<std::map<std::string, std::string>>& rows);
    
    /**
     * @brief Draw simple table
     */
    void drawSimpleTable(const std::vector<std::string>& headers,
                        const std::vector<std::vector<std::string>>& rows);
    
    /**
     * @brief Draw key-value table
     */
    void drawKeyValueTable(const std::map<std::string, std::string>& data,
                          const std::string& title = "");
    
    // ============================================
    // MENUS
    // ============================================
    
    /**
     * @brief Show menu and get selection
     */
    std::string showMenu(const std::vector<MenuItem>& items, const std::string& title = "");
    
    /**
     * @brief Show menu with callback
     */
    void showMenuInteractive(const std::vector<MenuItem>& items, const std::string& title = "");
    
    /**
     * @brief Show confirmation dialog
     */
    bool confirm(const std::string& message, bool default_yes = true);
    
    /**
     * @brief Show input prompt
     */
    std::string prompt(const std::string& message, const std::string& default_value = "");
    
    /**
     * @brief Show selection prompt
     */
    int select(const std::string& message, const std::vector<std::string>& options);
    
    // ============================================
    // STATUS DISPLAY
    // ============================================
    
    /**
     * @brief Display system status
     */
    void displayStatus(const std::map<std::string, std::string>& status);
    
    /**
     * @brief Display recording status
     */
    void displayRecordingStatus(bool is_recording, const std::string& filename = "",
                               int duration = 0, int64_t size = 0);
    
    /**
     * @brief Display storage status
     */
    void displayStorageStatus(uint64_t total, uint64_t used, uint64_t free);
    
    /**
     * @brief Display memory status
     */
    void displayMemoryStatus(size_t total, size_t used, size_t free);
    
    // ============================================
    // HEADER / FOOTER
    // ============================================
    
    /**
     * @brief Draw header
     */
    void drawHeader(const std::string& title);
    
    /**
     * @brief Draw footer
     */
    void drawFooter(const std::string& text = "");
    
    /**
     * @brief Draw separator
     */
    void drawSeparator(char fill = '=', int width = -1);
    
    /**
     * @brief Draw box
     */
    void drawBox(const std::string& title, const std::string& content, int width = -1);
    
    // ============================================
    // SCREEN MANAGEMENT
    // ============================================
    
    /**
     * @brief Start fullscreen mode
     */
    void enterFullscreen();
    
    /**
     * @brief Exit fullscreen mode
     */
    void exitFullscreen();
    
    /**
     * @brief Check if in fullscreen mode
     */
    bool isFullscreen() const { return fullscreen; }
    
    /**
     * @brief Refresh display
     */
    void refresh();
    
    /**
     * @brief Set refresh interval
     */
    void setRefreshInterval(int ms);
    
    /**
     * @brief Start auto-refresh
     */
    void startAutoRefresh(UpdateCallback callback);
    
    /**
     * @brief Stop auto-refresh
     */
    void stopAutoRefresh();
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get terminal size
     */
    std::pair<int, int> getTerminalSize() const;
    
    /**
     * @brief Get terminal width
     */
    int getTerminalWidth() const;
    
    /**
     * @brief Get terminal height
     */
    int getTerminalHeight() const;
    
    /**
     * @brief Center text
     */
    std::string centerText(const std::string& text, int width = -1);
    
    /**
     * @brief Format size (bytes to human readable)
     */
    std::string formatSize(uint64_t bytes) const;
    
    /**
     * @brief Format duration (seconds to human readable)
     */
    std::string formatDuration(int seconds) const;
    
    /**
     * @brief Format timestamp
     */
    std::string formatTimestamp() const;
    
    /**
     * @brief Color code getter
     */
    std::string getColorCode(Color color) const;
    
    /**
     * @brief Style code getter
     */
    std::string getStyleCode(Style style) const;
    
    /**
     * @brief Reset style
     */
    std::string getResetCode() const;
    
    /**
     * @brief Check if terminal supports color
     */
    bool supportsColor() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    bool initialized = false;
    bool fullscreen = false;
    bool color_supported = false;
    std::string title;
    int terminal_width = 80;
    int terminal_height = 24;
    
    // Refresh thread
    std::thread refresh_thread;
    std::atomic<bool> auto_refresh;
    int refresh_interval = 1000;  // ms
    UpdateCallback refresh_callback;
    mutable std::mutex mutex;
    
    // Color maps
    static const std::map<Color, std::string> color_codes;
    static const std::map<Style, std::string> style_codes;
    static const std::map<LogLevel, Color> log_colors;
    static const std::map<LogLevel, std::string> log_names;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    void getTerminalSizeImpl();
    void refreshLoop();
    void updateTerminalSize();
    bool isColorTerminal() const;
    std::string getFormattedSize(uint64_t bytes) const;
    std::string getFormattedDuration(int seconds) const;
    std::string formatTableRow(const std::vector<std::string>& columns,
                              const std::vector<int>& widths,
                              const std::vector<Alignment>& alignments);
    std::vector<int> calculateColumnWidths(const std::vector<TableColumn>& columns,
                                          const std::vector<std::map<std::string, std::string>>& rows);
    std::string repeatChar(char c, int count);
    std::string truncateText(const std::string& text, int max_width);
    
    // Spinner frames
    static const std::vector<std::string> spinner_frames;
    static const std::vector<std::string> indeterminate_frames;
};

#endif // CONSOLE_UI_HPP
