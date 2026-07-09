#include "TimeUtils.hpp"

#include <iostream>
#include <regex>
#include <cmath>

const std::map<TimeUtils::Format, std::string> TimeUtils::format_strings = {
    {Format::ISO8601, "%Y-%m-%dT%H:%M:%S"},
    {Format::RFC3339, "%Y-%m-%dT%H:%M:%SZ"},
    {Format::RFC2822, "%a, %d %b %Y %H:%M:%S %z"},
    {Format::UNIX, "%s"},
    {Format::HUMAN, "%B %d, %Y at %I:%M:%S %p"},
    {Format::COMPACT, "%Y%m%d%H%M%S"},
    {Format::LOG, "%Y-%m-%d %H:%M:%S"},
    {Format::FILENAME, "%Y%m%d_%H%M%S"}
};

const std::map<std::string, int> TimeUtils::duration_units = {
    {"ns", 1},
    {"us", 1000},
    {"ms", 1000000},
    {"s", 1000000000},
    {"m", 60000000000},
    {"h", 3600000000000},
    {"d", 86400000000000}
};

// ============================================
// TIMESTAMP
// ============================================

std::chrono::system_clock::time_point TimeUtils::now() {
    return std::chrono::system_clock::now();
}

std::string TimeUtils::format(const std::chrono::system_clock::time_point& tp, Format fmt) {
    auto it = format_strings.find(fmt);
    if (it != format_strings.end()) {
        return format(tp, it->second);
    }
    return format(tp, format_strings.at(Format::ISO8601));
}

std::string TimeUtils::format(const std::chrono::system_clock::time_point& tp,
                             const std::string& format_str) {
    auto tm_info = toTM(tp);
    char buffer[128];
    strftime(buffer, sizeof(buffer), format_str.c_str(), &tm_info);
    return std::string(buffer);
}

std::chrono::system_clock::time_point TimeUtils::parse(const std::string& str, Format fmt) {
    auto it = format_strings.find(fmt);
    if (it != format_strings.end()) {
        return parse(str, it->second);
    }
    return parse(str, format_strings.at(Format::ISO8601));
}

std::chrono::system_clock::time_point TimeUtils::parse(const std::string& str,
                                                      const std::string& format_str) {
    struct tm tm_info = {};
    strptime(str.c_str(), format_str.c_str(), &tm_info);
    return fromTM(tm_info);
}

int64_t TimeUtils::unixTimestamp() {
    return unixTimestamp(now());
}

int64_t TimeUtils::unixTimestamp(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        tp.time_since_epoch()).count();
}

int64_t TimeUtils::millisecondsSinceEpoch() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now().time_since_epoch()).count();
}

int64_t TimeUtils::microsecondsSinceEpoch() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now().time_since_epoch()).count();
}

// ============================================
// DURATION
// ============================================

TimeUtils::Duration TimeUtils::between(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    
    auto duration = end - start;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    
    Duration result;
    result.nanoseconds = ns;
    result.microseconds = ns / 1000;
    result.milliseconds = ns / 1000000;
    result.seconds = ns / 1000000000;
    result.minutes = result.seconds / 60;
    result.hours = result.minutes / 60;
    result.days = result.hours / 24;
    
    return result;
}

std::string TimeUtils::formatDuration(const Duration& duration) {
    std::stringstream ss;
    
    if (duration.days > 0) {
        ss << duration.days << "d ";
    }
    if (duration.hours > 0 || duration.days > 0) {
        ss << duration.hours << "h ";
    }
    if (duration.minutes > 0 || duration.hours > 0 || duration.days > 0) {
        ss << duration.minutes << "m ";
    }
    ss << duration.seconds << "s";
    
    return ss.str();
}

std::string TimeUtils::formatDurationHuman(const Duration& duration) {
    std::stringstream ss;
    
    if (duration.days > 0) {
        ss << duration.days << " day" << (duration.days > 1 ? "s" : "");
        if (duration.hours > 0 || duration.minutes > 0) {
            ss << ", ";
        }
    }
    if (duration.hours > 0) {
        ss << duration.hours << " hour" << (duration.hours > 1 ? "s" : "");
        if (duration.minutes > 0) {
            ss << ", ";
        }
    }
    if (duration.minutes > 0) {
        ss << duration.minutes << " minute" << (duration.minutes > 1 ? "s" : "");
    }
    if (duration.days == 0 && duration.hours == 0 && duration.minutes == 0) {
        if (duration.seconds > 0) {
            ss << duration.seconds << " second" << (duration.seconds > 1 ? "s" : "");
        } else if (duration.milliseconds > 0) {
            ss << duration.milliseconds << " ms";
        } else if (duration.microseconds > 0) {
            ss << duration.microseconds << " us";
        } else {
            ss << duration.nanoseconds << " ns";
        }
    }
    
    return ss.str();
}

TimeUtils::Duration TimeUtils::parseDuration(const std::string& str) {
    Duration result;
    
    std::regex duration_regex(R"((\d+\.?\d*)([a-zA-Z]+))");
    std::smatch match;
    std::string remaining = str;
    
    while (std::regex_search(remaining, match, duration_regex)) {
        double value = std::stod(match[1].str());
        std::string unit = match[2].str();
        
        auto it = duration_units.find(unit);
        if (it != duration_units.end()) {
            result.nanoseconds += static_cast<int64_t>(value * it->second);
        }
        
        remaining = match.suffix().str();
    }
    
    // Recalculate all fields
    result.microseconds = result.nanoseconds / 1000;
    result.milliseconds = result.nanoseconds / 1000000;
    result.seconds = result.nanoseconds / 1000000000;
    result.minutes = result.seconds / 60;
    result.hours = result.minutes / 60;
    result.days = result.hours / 24;
    
    return result;
}

// ============================================
// TIMEZONE
// ============================================

std::string TimeUtils::getTimezoneName() {
    std::string tz;
    char* tz_env = getenv("TZ");
    if (tz_env) {
        tz = tz_env;
    } else {
        tz = "UTC";
    }
    return tz;
}

int TimeUtils::getTimezoneOffset() {
    time_t t = time(nullptr);
    struct tm* tm_local = localtime(&t);
    struct tm* tm_utc = gmtime(&t);
    return static_cast<int>(mktime(tm_local) - mktime(tm_utc));
}

std::chrono::system_clock::time_point TimeUtils::convertTimezone(
    const std::chrono::system_clock::time_point& tp,
    const std::string& from_tz,
    const std::string& to_tz) {
    // This would require a timezone database
    // Simplified implementation
    return tp;
}

// ============================================
// PERFORMANCE TIMING
// ============================================

TimeUtils::Timer::Timer() : running(false) {}

void TimeUtils::Timer::start() {
    start_time = std::chrono::steady_clock::now();
    running = true;
}

void TimeUtils::Timer::stop() {
    if (running) {
        stop_time = std::chrono::steady_clock::now();
        running = false;
    }
}

void TimeUtils::Timer::reset() {
    running = false;
    start_time = std::chrono::steady_clock::time_point();
    stop_time = std::chrono::steady_clock::time_point();
}

TimeUtils::Duration TimeUtils::Timer::elapsed() const {
    auto end = running ? std::chrono::steady_clock::now() : stop_time;
    auto duration = end - start_time;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    
    Duration result;
    result.nanoseconds = ns;
    result.microseconds = ns / 1000;
    result.milliseconds = ns / 1000000;
    result.seconds = ns / 1000000000;
    result.minutes = result.seconds / 60;
    result.hours = result.minutes / 60;
    result.days = result.hours / 24;
    
    return result;
}

double TimeUtils::Timer::elapsedSeconds() const {
    return elapsed().seconds + elapsed().milliseconds / 1000.0;
}

double TimeUtils::Timer::elapsedMilliseconds() const {
    return elapsed().milliseconds + elapsed().microseconds / 1000.0;
}

TimeUtils::ScopedTimer::ScopedTimer(const std::string& n,
                                   std::function<void(const std::string&, double)> cb)
    : name(n), callback(cb) {
    timer.start();
}

TimeUtils::ScopedTimer::~ScopedTimer() {
    timer.stop();
    auto elapsed = timer.elapsedMilliseconds();
    if (callback) {
        callback(name, elapsed);
    }
}

// ============================================
// UTILITY
// ============================================

std::string TimeUtils::getWeekdayName(int day) {
    static const std::vector<std::string> weekdays = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    if (day >= 0 && day < 7) {
        return weekdays[day];
    }
    return "";
}

std::string TimeUtils::getMonthName(int month) {
    static const std::vector<std::string> months = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    if (month >= 1 && month <= 12) {
        return months[month - 1];
    }
    return "";
}

bool TimeUtils::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int TimeUtils::getDaysInMonth(int year, int month) {
    static const std::vector<int> days = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    if (month >= 1 && month <= 12) {
        return days[month - 1];
    }
    return 0;
}

int TimeUtils::getDayOfYear(int year, int month, int day) {
    int day_of_year = 0;
    for (int m = 1; m < month; m++) {
        day_of_year += getDaysInMonth(year, m);
    }
    day_of_year += day;
    return day_of_year;
}

int TimeUtils::getWeekNumber(int year, int month, int day) {
    struct tm tm_info = {};
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = month - 1;
    tm_info.tm_mday = day;
    mktime(&tm_info);
    return tm_info.tm_wday / 7 + 1;
}

// ============================================
// PRIVATE HELPERS
// ============================================

struct tm TimeUtils::toTM(const std::chrono::system_clock::time_point& tp) {
    time_t time_t_value = std::chrono::system_clock::to_time_t(tp);
    struct tm tm_info;
    localtime_r(&time_t_value, &tm_info);
    return tm_info;
}

std::chrono::system_clock::time_point TimeUtils::fromTM(const struct tm& tm_info) {
    struct tm tm_copy = tm_info;
    time_t time_t_value = mktime(&tm_copy);
    return std::chrono::system_clock::from_time_t(time_t_value);
}

std::string TimeUtils::padNumber(int number, int width) {
    std::stringstream ss;
    ss << std::setw(width) << std::setfill('0') << number;
    return ss.str();
}
