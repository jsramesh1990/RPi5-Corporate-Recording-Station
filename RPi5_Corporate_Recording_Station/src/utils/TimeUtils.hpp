#ifndef TIME_UTILS_HPP
#define TIME_UTILS_HPP

#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <map>
#include <functional>

/**
 * @brief Time Utilities for Recording Station
 * 
 * Provides time-related utility functions including:
 * - Timestamp formatting
 * - Duration formatting
 * - Time parsing
 * - Timezone handling
 * - Performance timing
 */
class TimeUtils {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Format {
        ISO8601,
        RFC3339,
        RFC2822,
        UNIX,
        HUMAN,
        COMPACT,
        LOG,
        FILENAME
    };
    
    enum class Unit {
        NANOSECONDS,
        MICROSECONDS,
        MILLISECONDS,
        SECONDS,
        MINUTES,
        HOURS,
        DAYS
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    struct Duration {
        int64_t nanoseconds = 0;
        int64_t microseconds = 0;
        int64_t milliseconds = 0;
        int64_t seconds = 0;
        int64_t minutes = 0;
        int64_t hours = 0;
        int64_t days = 0;
        
        std::string toString() const;
        std::string toHumanString() const;
        double toUnit(Unit unit) const;
    };
    
    // ============================================
    // TIMESTAMP
    // ============================================
    
    /**
     * @brief Get current timestamp
     */
    static std::chrono::system_clock::time_point now();
    
    /**
     * @brief Format timestamp
     */
    static std::string format(const std::chrono::system_clock::time_point& tp,
                             Format fmt = Format::ISO8601);
    
    /**
     * @brief Format timestamp with custom format
     */
    static std::string format(const std::chrono::system_clock::time_point& tp,
                             const std::string& format_str);
    
    /**
     * @brief Parse timestamp from string
     */
    static std::chrono::system_clock::time_point parse(const std::string& str,
                                                       Format fmt = Format::ISO8601);
    
    /**
     * @brief Parse timestamp with custom format
     */
    static std::chrono::system_clock::time_point parse(const std::string& str,
                                                       const std::string& format_str);
    
    /**
     * @brief Get Unix timestamp (seconds since epoch)
     */
    static int64_t unixTimestamp();
    
    /**
     * @brief Get Unix timestamp from time_point
     */
    static int64_t unixTimestamp(const std::chrono::system_clock::time_point& tp);
    
    /**
     * @brief Get milliseconds since epoch
     */
    static int64_t millisecondsSinceEpoch();
    
    /**
     * @brief Get microseconds since epoch
     */
    static int64_t microsecondsSinceEpoch();
    
    // ============================================
    // DURATION
    // ============================================
    
    /**
     * @brief Calculate duration between two time points
     */
    static Duration between(const std::chrono::system_clock::time_point& start,
                           const std::chrono::system_clock::time_point& end);
    
    /**
     * @brief Format duration
     */
    static std::string formatDuration(const Duration& duration);
    
    /**
     * @brief Format duration in human-readable form
     */
    static std::string formatDurationHuman(const Duration& duration);
    
    /**
     * @brief Parse duration from string (e.g., "1h30m", "2d", "3.5s")
     */
    static Duration parseDuration(const std::string& str);
    
    // ============================================
    // TIMEZONE
    // ============================================
    
    /**
     * @brief Get current timezone name
     */
    static std::string getTimezoneName();
    
    /**
     * @brief Get timezone offset in seconds
     */
    static int getTimezoneOffset();
    
    /**
     * @brief Convert time between timezones
     */
    static std::chrono::system_clock::time_point convertTimezone(
        const std::chrono::system_clock::time_point& tp,
        const std::string& from_tz,
        const std::string& to_tz);
    
    // ============================================
    // PERFORMANCE TIMING
    // ============================================
    
    /**
     * @brief Performance timer
     */
    class Timer {
    public:
        Timer();
        void start();
        void stop();
        void reset();
        Duration elapsed() const;
        double elapsedSeconds() const;
        double elapsedMilliseconds() const;
        
    private:
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point stop_time;
        bool running = false;
    };
    
    /**
     * @brief Scoped timer for RAII timing
     */
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& name = "", 
                   std::function<void(const std::string&, double)> callback = nullptr);
        ~ScopedTimer();
        
    private:
        std::string name;
        Timer timer;
        std::function<void(const std::string&, double)> callback;
    };
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get weekday name
     */
    static std::string getWeekdayName(int day);
    
    /**
     * @brief Get month name
     */
    static std::string getMonthName(int month);
    
    /**
     * @brief Check if year is leap year
     */
    static bool isLeapYear(int year);
    
    /**
     * @brief Get days in month
     */
    static int getDaysInMonth(int year, int month);
    
    /**
     * @brief Get day of year
     */
    static int getDayOfYear(int year, int month, int day);
    
    /**
     * @brief Get week number
     */
    static int getWeekNumber(int year, int month, int day);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    TimeUtils() = delete;
    ~TimeUtils() = delete;
    
    // Format strings
    static const std::map<Format, std::string> format_strings;
    static const std::map<std::string, int> duration_units;
    
    static struct tm toTM(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point fromTM(const struct tm& tm_info);
    static std::string padNumber(int number, int width = 2);
};

#endif // TIME_UTILS_HPP
