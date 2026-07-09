#include "StorageMonitor.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <chrono>
#include <thread>
#include <regex>
#include <openssl/sha.h>
#include <openssl/md5.h>

StorageMonitor::StorageMonitor(const std::string& root_path)
    : root_path(root_path), monitoring_active(false), scan_interval(5) {
    // Create directories if they don't exist
    if (!fs::exists(root_path)) {
        fs::create_directories(root_path);
    }
    
    // Initialize exclude patterns
    exclude_patterns = {
        ".*",           // Hidden files
        "*.tmp",        // Temporary files
        "*.swp",        // Swap files
        "*.lock",       // Lock files
        ".DS_Store",    // macOS metadata
        "Thumbs.db",    // Windows thumbnails
        "desktop.ini",  // Windows desktop
        "*.log"         // Log files (optional)
    };
}

StorageMonitor::~StorageMonitor() {
    stop_monitoring();
}

void StorageMonitor::scan_directory(bool recursive) {
    std::lock_guard<std::mutex> lock(files_mutex);
    clear_file_list();
    scan_recursive = recursive;
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
            throw std::runtime_error("Root path does not exist or is not a directory");
        }
        
        scan_directory_impl(root_path, recursive);
        
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    stats.scan_duration_seconds = std::chrono::duration<double>(end_time - start_time).count();
    
    update_statistics_impl();
    
    if (on_scan_complete) {
        on_scan_complete(stats);
    }
}

void StorageMonitor::scan_directory_impl(const fs::path& path, bool recursive) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (!should_include_file(entry.path())) {
                continue;
            }
            
            try {
                FileMetadata metadata = collect_file_info(entry.path());
                if (metadata.size_bytes > 0 || metadata.file_type == "directory") {
                    files.push_back(metadata);
                    update_file_type_distribution(metadata);
                }
            } catch (const std::exception& e) {
                // Skip files that can't be read
                continue;
            }
            
            // Recursively scan subdirectories
            if (recursive && fs::is_directory(entry.path())) {
                scan_directory_impl(entry.path(), true);
            }
        }
    } catch (const std::exception& e) {
        // Permission denied or other error
    }
}

FileMetadata StorageMonitor::collect_file_info(const fs::path& file_path) {
    FileMetadata metadata;
    metadata.path = file_path.string();
    metadata.filename = file_path.filename().string();
    
    try {
        auto status = fs::status(file_path);
        
        // Get file type
        if (fs::is_regular_file(status)) {
            metadata.file_type = "file";
            metadata.size_bytes = fs::file_size(file_path);
            
            // Determine file type by extension
            std::string ext = file_path.extension().string();
            if (!ext.empty()) {
                metadata.file_type = ext.substr(1); // Remove leading dot
            }
        } else if (fs::is_directory(status)) {
            metadata.file_type = "directory";
            metadata.size_bytes = 0;
        } else if (fs::is_symlink(status)) {
            metadata.file_type = "symlink";
            metadata.size_bytes = 0;
        } else {
            metadata.file_type = "other";
            metadata.size_bytes = 0;
        }
        
        // Get time credentials
        metadata.time_creds = collect_time_credentials(file_path);
        
        // Get permissions
        metadata.permissions = status.permissions();
        
        // Get ownership
        struct stat attr;
        if (stat(file_path.string().c_str(), &attr) == 0) {
            struct passwd* pwd = getpwuid(attr.st_uid);
            if (pwd) metadata.owner = pwd->pw_name;
            
            struct group* grp = getgrgid(attr.st_gid);
            if (grp) metadata.group = grp->gr_name;
        }
        
        // Calculate checksums for regular files
        if (fs::is_regular_file(status) && metadata.size_bytes < 100 * 1024 * 1024) { // < 100MB
            metadata.checksum_sha256 = calculate_sha256(file_path.string());
            metadata.checksum_md5 = calculate_md5(file_path.string());
        }
        
        // Additional metadata
        metadata.is_compressed = (file_path.extension() == ".gz" || 
                                  file_path.extension() == ".zip" ||
                                  file_path.extension() == ".bz2");
        metadata.is_encrypted = false; // Can't determine easily
        
    } catch (const std::exception& e) {
        // Handle errors gracefully
        metadata.size_bytes = 0;
        metadata.file_type = "error";
    }
    
    return metadata;
}

TimeCredentials StorageMonitor::collect_time_credentials(const fs::path& file_path) {
    TimeCredentials creds;
    
    try {
        struct stat attr;
        if (stat(file_path.string().c_str(), &attr) == 0) {
            // Set time points
            creds.creation_time = std::chrono::system_clock::from_time_t(attr.st_ctime);
            creds.modification_time = std::chrono::system_clock::from_time_t(attr.st_mtime);
            creds.access_time = std::chrono::system_clock::from_time_t(attr.st_atime);
            creds.status_change_time = std::chrono::system_clock::from_time_t(attr.st_ctime);
            
            // Set epoch timestamps
            creds.creation_epoch = attr.st_ctime;
            creds.modification_epoch = attr.st_mtime;
            creds.access_epoch = attr.st_atime;
            creds.status_change_epoch = attr.st_ctime;
            
            // Set formatted strings
            creds.creation_formatted = format_time(creds.creation_time);
            creds.modification_formatted = format_time(creds.modification_time);
            creds.access_formatted = format_time(creds.access_time);
            creds.status_change_formatted = format_time(creds.status_change_time);
            
            // Calculate age
            time_t now = time(nullptr);
            creds.age_seconds = now - attr.st_ctime;
            creds.age_days = creds.age_seconds / 86400;
            creds.age_months = creds.age_days / 30;
            
            // Get timezone
            tzset();
            creds.timezone = std::string(tzname[0]);
            creds.timezone_offset = -timezone;
        }
    } catch (const std::exception& e) {
        // Handle errors
    }
    
    return creds;
}

std::string StorageMonitor::calculate_sha256(const std::string& file_path) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) return "";
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string StorageMonitor::calculate_md5(const std::string& file_path) {
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_CTX md5;
    MD5_Init(&md5);
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) return "";
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        MD5_Update(&md5, buffer, file.gcount());
    }
    MD5_Final(hash, &md5);
    
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

void StorageMonitor::update_statistics() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    update_statistics_impl();
}

void StorageMonitor::update_statistics_impl() {
    stats.total_bytes = 0;
    stats.used_bytes = 0;
    stats.free_bytes = 0;
    stats.available_bytes = 0;
    stats.file_count = 0;
    stats.directory_count = 0;
    stats.symlink_count = 0;
    stats.largest_file_size = 0;
    stats.smallest_file_size = SIZE_MAX;
    
    // Get filesystem stats
    struct statvfs stat;
    if (statvfs(root_path.string().c_str(), &stat) == 0) {
        stats.total_bytes = stat.f_blocks * stat.f_frsize;
        stats.free_bytes = stat.f_bfree * stat.f_frsize;
        stats.available_bytes = stat.f_bavail * stat.f_frsize;
        stats.used_bytes = stats.total_bytes - stats.free_bytes;
        stats.total_inodes = stat.f_files;
        stats.used_inodes = stat.f_files - stat.f_ffree;
    }
    
    // Count files and directories
    for (const auto& file : files) {
        if (file.file_type == "directory") {
            stats.directory_count++;
        } else if (file.file_type == "symlink") {
            stats.symlink_count++;
        } else {
            stats.file_count++;
            stats.total_bytes += file.size_bytes;
            
            if (file.size_bytes > stats.largest_file_size) {
                stats.largest_file_size = file.size_bytes;
            }
            if (file.size_bytes > 0 && file.size_bytes < stats.smallest_file_size) {
                stats.smallest_file_size = file.size_bytes;
            }
        }
    }
    
    if (stats.smallest_file_size == SIZE_MAX) {
        stats.smallest_file_size = 0;
    }
    
    calculate_average_file_size();
    stats.timestamp = std::chrono::system_clock::now();
    
    // Set last scan time
    stats.last_scan_time = collect_time_credentials(root_path);
}

void StorageMonitor::calculate_average_file_size() {
    if (stats.file_count > 0) {
        stats.average_file_size = static_cast<double>(stats.total_bytes) / stats.file_count;
    } else {
        stats.average_file_size = 0.0;
    }
}

StorageStatistics StorageMonitor::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

std::vector<FileMetadata> StorageMonitor::get_files() const {
    std::lock_guard<std::mutex> lock(files_mutex);
    return files;
}

std::vector<FileMetadata> StorageMonitor::get_files_by_type(const std::string& file_type) const {
    std::lock_guard<std::mutex> lock(files_mutex);
    std::vector<FileMetadata> result;
    for (const auto& file : files) {
        if (file.file_type == file_type) {
            result.push_back(file);
        }
    }
    return result;
}

std::vector<FileMetadata> StorageMonitor::get_largest_files(size_t count) const {
    std::lock_guard<std::mutex> lock(files_mutex);
    auto sorted = files;
    std::sort(sorted.begin(), sorted.end(),
        [](const FileMetadata& a, const FileMetadata& b) {
            return a.size_bytes > b.size_bytes;
        });
    
    if (sorted.size() > count) {
        sorted.resize(count);
    }
    return sorted;
}

std::vector<FileMetadata> StorageMonitor::get_oldest_files(size_t count) const {
    std::lock_guard<std::mutex> lock(files_mutex);
    auto sorted = files;
    std::sort(sorted.begin(), sorted.end(),
        [](const FileMetadata& a, const FileMetadata& b) {
            return a.time_creds.creation_epoch < b.time_creds.creation_epoch;
        });
    
    if (sorted.size() > count) {
        sorted.resize(count);
    }
    return sorted;
}

std::string StorageMonitor::format_time(const std::chrono::system_clock::time_point& tp) const {
    time_t time_t_value = std::chrono::system_clock::to_time_t(tp);
    return format_time(time_t_value);
}

std::string StorageMonitor::format_time(time_t time_val) const {
    char buffer[64];
    struct tm* timeinfo = localtime(&time_val);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

std::string StorageMonitor::get_age_string(const TimeCredentials& creds) const {
    if (creds.age_days < 1) {
        return "Today";
    } else if (creds.age_days < 2) {
        return "Yesterday";
    } else if (creds.age_days < 7) {
        return std::to_string(creds.age_days) + " days ago";
    } else if (creds.age_days < 30) {
        return std::to_string(creds.age_days / 7) + " weeks ago";
    } else if (creds.age_days < 365) {
        return std::to_string(creds.age_months) + " months ago";
    } else {
        return std::to_string(creds.age_days / 365) + " years ago";
    }
}

void StorageMonitor::start_monitoring(int interval_seconds) {
    if (monitoring_active) return;
    
    scan_interval = interval_seconds;
    monitoring_active = true;
    
    // Initial scan
    scan_directory(scan_recursive);
    
    // Start monitoring thread
    monitor_thread = std::thread(&StorageMonitor::scan_loop, this);
}

void StorageMonitor::stop_monitoring() {
    if (!monitoring_active) return;
    
    monitoring_active = false;
    cv.notify_all();
    
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
}

void StorageMonitor::scan_loop() {
    while (monitoring_active) {
        std::unique_lock<std::mutex> lock(files_mutex);
        cv.wait_for(lock, std::chrono::seconds(scan_interval),
            [this]() { return !monitoring_active; });
        
        if (!monitoring_active) break;
        
        // Perform rescan
        rescan();
    }
}

void StorageMonitor::rescan() {
    // This would compare current files with previous state
    // and trigger callbacks for changes
    // Simplified version - just rescan
    scan_directory(scan_recursive);
}

bool StorageMonitor::should_include_file(const fs::path& file_path) const {
    // Skip hidden files (starting with .)
    if (file_path.filename().string().front() == '.') {
        return false;
    }
    
    return !is_excluded(file_path);
}

bool StorageMonitor::is_excluded(const fs::path& file_path) const {
    std::lock_guard<std::mutex> lock(exclude_mutex);
    std::string filename = file_path.filename().string();
    
    for (const auto& pattern : exclude_patterns) {
        try {
            std::regex regex_pattern(pattern, std::regex::icase);
            if (std::regex_match(filename, regex_pattern)) {
                return true;
            }
        } catch (const std::exception&) {
            // If regex fails, try simple string matching
            if (filename.find(pattern) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

void StorageMonitor::set_exclude_patterns(const std::vector<std::string>& patterns) {
    std::lock_guard<std::mutex> lock(exclude_mutex);
    exclude_patterns = patterns;
}

void StorageMonitor::add_exclude_pattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(exclude_mutex);
    exclude_patterns.push_back(pattern);
}

void StorageMonitor::remove_exclude_pattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(exclude_mutex);
    exclude_patterns.erase(
        std::remove(exclude_patterns.begin(), exclude_patterns.end(), pattern),
        exclude_patterns.end()
    );
}

void StorageMonitor::export_to_json(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(files_mutex);
    std::ofstream out(filename);
    
    out << "{\n";
    out << "  \"timestamp\": \"" << format_time(std::chrono::system_clock::now()) << "\",\n";
    out << "  \"statistics\": {\n";
    out << "    \"total_bytes\": " << stats.total_bytes << ",\n";
    out << "    \"used_bytes\": " << stats.used_bytes << ",\n";
    out << "    \"free_bytes\": " << stats.free_bytes << ",\n";
    out << "    \"file_count\": " << stats.file_count << ",\n";
    out << "    \"directory_count\": " << stats.directory_count << "\n";
    out << "  },\n";
    out << "  \"files\": [\n";
    
    for (size_t i = 0; i < files.size(); i++) {
        const auto& f = files[i];
        out << "    {\n";
        out << "      \"filename\": \"" << f.filename << "\",\n";
        out << "      \"path\": \"" << f.path << "\",\n";
        out << "      \"size_bytes\": " << f.size_bytes << ",\n";
        out << "      \"type\": \"" << f.file_type << "\",\n";
        out << "      \"created\": \"" << f.time_creds.creation_formatted << "\",\n";
        out << "      \"modified\": \"" << f.time_creds.modification_formatted << "\"\n";
        out << "    }";
        if (i < files.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    out.close();
}

void StorageMonitor::set_on_file_added(FileAddedCallback callback) {
    on_file_added = callback;
}

void StorageMonitor::set_on_file_modified(FileModifiedCallback callback) {
    on_file_modified = callback;
}

void StorageMonitor::set_on_file_deleted(FileDeletedCallback callback) {
    on_file_deleted = callback;
}

void StorageMonitor::set_on_threshold_exceeded(ThresholdExceededCallback callback) {
    on_threshold_exceeded = callback;
}

void StorageMonitor::set_on_scan_complete(ScanCompleteCallback callback) {
    on_scan_complete = callback;
}

void StorageMonitor::set_root_path(const std::string& path) {
    stop_monitoring();
    root_path = path;
    clear_file_list();
    scan_directory(scan_recursive);
}

void StorageMonitor::clear_file_list() {
    files.clear();
    stats.file_type_distribution.clear();
    stats.file_age_distribution.clear();
}

size_t StorageMonitor::get_file_count() const {
    std::lock_guard<std::mutex> lock(files_mutex);
    return files.size();
}

uint64_t StorageMonitor::get_total_size() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats.total_bytes;
}

uint64_t StorageMonitor::get_free_space() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats.free_bytes;
}

bool StorageMonitor::has_space_for(uint64_t size) const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats.free_bytes >= size;
}

void StorageMonitor::update_file_type_distribution(const FileMetadata& file) {
    stats.file_type_distribution[file.file_type]++;
}

void StorageMonitor::check_thresholds() {
    if (stats.total_bytes > 0) {
        float used_percent = (float)stats.used_bytes / stats.total_bytes * 100;
        if (used_percent >= critical_threshold && on_threshold_exceeded) {
            on_threshold_exceeded(used_percent, critical_threshold);
        } else if (used_percent >= warning_threshold && on_threshold_exceeded) {
            on_threshold_exceeded(used_percent, warning_threshold);
        }
    }
}
