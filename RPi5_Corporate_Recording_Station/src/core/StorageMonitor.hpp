#ifndef STORAGE_MONITOR_HPP
#define STORAGE_MONITOR_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include "FileMetadata.hpp"

namespace fs = std::filesystem;

/**
 * @brief Storage statistics with performance metrics
 */
struct StorageStatistics {
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t available_bytes = 0;
    size_t file_count = 0;
    size_t directory_count = 0;
    size_t symlink_count = 0;
    size_t total_inodes = 0;
    size_t used_inodes = 0;
    float fragmentation_percent = 0.0f;
    TimeCredentials last_scan_time;
    double scan_duration_seconds = 0.0;
    size_t largest_file_size = 0;
    size_t smallest_file_size = 0;
    double average_file_size = 0.0;
    std::map<std::string, size_t> file_type_distribution;
    std::map<std::string, size_t> file_age_distribution;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Main storage monitor class
 * 
 * Implements comprehensive storage monitoring with memory management
 * and time credentials tracking for corporate recording systems.
 */
class StorageMonitor {
public:
    StorageMonitor(const std::string& root_path = "/var/lib/recording-station");
    ~StorageMonitor();
    
    // Core monitoring functions
    void scan_directory(bool recursive = true);
    void scan_file(const std::string& file_path);
    void update_statistics();
    void rescan();
    
    // Time credential functions
    TimeCredentials get_file_timestamps(const std::string& file_path) const;
    std::string format_time(const std::chrono::system_clock::time_point& tp) const;
    std::string format_time(time_t time_val) const;
    std::string get_age_string(const TimeCredentials& creds) const;
    
    // Query functions
    StorageStatistics get_statistics() const;
    std::vector<FileMetadata> get_files() const;
    std::vector<FileMetadata> get_files_by_time_range(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;
    std::vector<FileMetadata> get_files_by_type(const std::string& file_type) const;
    std::vector<FileMetadata> get_files_by_size(uint64_t min_size, uint64_t max_size) const;
    std::vector<FileMetadata> get_oldest_files(size_t count = 10) const;
    std::vector<FileMetadata> get_largest_files(size_t count = 10) const;
    std::vector<FileMetadata> get_files_by_owner(const std::string& owner) const;
    std::vector<FileMetadata> get_files_by_pattern(const std::string& pattern) const;
    std::vector<FileMetadata> get_files_by_extension(const std::string& extension) const;
    
    // File management functions
    bool delete_file(const std::string& file_path);
    bool move_file(const std::string& source, const std::string& destination);
    bool copy_file(const std::string& source, const std::string& destination);
    bool compress_file(const std::string& file_path);
    bool decompress_file(const std::string& file_path);
    void delete_old_files(const std::chrono::system_clock::time_point& cutoff);
    void compress_old_files(const std::chrono::system_clock::time_point& cutoff);
    void move_files_to_archive(const std::chrono::system_clock::time_point& cutoff);
    void update_file_metadata(const std::string& file_path, 
                            const std::map<std::string, std::string>& metadata);
    void rename_file(const std::string& old_path, const std::string& new_path);
    
    // Export functions
    void export_to_json(const std::string& filename) const;
    void export_to_csv(const std::string& filename) const;
    void export_to_sqlite(const std::string& filename) const;
    void export_to_html(const std::string& filename) const;
    
    // Real-time monitoring
    void start_monitoring(int interval_seconds = 5);
    void stop_monitoring();
    bool is_monitoring() const { return monitoring_active; }
    void set_scan_interval(int seconds) { scan_interval = seconds; }
    
    // Event callbacks
    using FileAddedCallback = std::function<void(const FileMetadata&)>;
    using FileModifiedCallback = std::function<void(const FileMetadata&)>;
    using FileDeletedCallback = std::function<void(const std::string&)>;
    using ThresholdExceededCallback = std::function<void(float, float)>;
    using ScanCompleteCallback = std::function<void(const StorageStatistics&)>;
    
    void set_on_file_added(FileAddedCallback callback);
    void set_on_file_modified(FileModifiedCallback callback);
    void set_on_file_deleted(FileDeletedCallback callback);
    void set_on_threshold_exceeded(ThresholdExceededCallback callback);
    void set_on_scan_complete(ScanCompleteCallback callback);
    
    // Configuration
    void set_root_path(const std::string& path);
    std::string get_root_path() const { return root_path.string(); }
    void set_scan_recursive(bool recursive) { scan_recursive = recursive; }
    void set_exclude_patterns(const std::vector<std::string>& patterns);
    void add_exclude_pattern(const std::string& pattern);
    void remove_exclude_pattern(const std::string& pattern);
    void set_threshold_warning(float percentage);
    void set_threshold_critical(float percentage);
    
    // Utility functions
    size_t get_file_count() const;
    uint64_t get_total_size() const;
    uint64_t get_free_space() const;
    bool has_space_for(uint64_t size) const;
    uint64_t get_average_file_size() const;
    std::string get_file_system_type() const;
    bool is_path_accessible(const std::string& path) const;
    
private:
    fs::path root_path;
    std::vector<FileMetadata> files;
    StorageStatistics stats;
    mutable std::mutex files_mutex;
    mutable std::mutex stats_mutex;
    
    // Monitoring thread
    std::thread monitor_thread;
    std::condition_variable cv;
    std::atomic<bool> monitoring_active;
    int scan_interval = 5;
    bool scan_recursive = true;
    
    // Threshold settings
    float warning_threshold = 80.0f;
    float critical_threshold = 90.0f;
    
    // Exclude patterns
    std::vector<std::string> exclude_patterns;
    std::mutex exclude_mutex;
    
    // Callbacks
    FileAddedCallback on_file_added;
    FileModifiedCallback on_file_modified;
    FileDeletedCallback on_file_deleted;
    ThresholdExceededCallback on_threshold_exceeded;
    ScanCompleteCallback on_scan_complete;
    
    // Helper functions
    FileMetadata collect_file_info(const fs::path& file_path);
    TimeCredentials collect_time_credentials(const fs::path& file_path);
    std::string calculate_sha256(const std::string& file_path);
    std::string calculate_md5(const std::string& file_path);
    std::string calculate_file_type(const std::string& file_path);
    void update_file_age_distribution(const FileMetadata& file);
    void check_thresholds();
    void scan_loop();
    void notify_file_change(const FileMetadata& file, const std::string& event);
    bool should_include_file(const fs::path& file_path) const;
    std::vector<fs::path> get_file_changes();
    bool is_excluded(const fs::path& file_path) const;
    uint64_t get_file_size(const fs::path& path) const;
    std::string get_file_permissions(const fs::path& path) const;
    std::string get_file_owner(const fs::path& path) const;
    std::string get_file_group(const fs::path& path) const;
    void update_file_type_distribution(const FileMetadata& file);
    void calculate_average_file_size();
    void scan_directory_impl(const fs::path& path, bool recursive);
    void update_statistics_impl();
    void clear_file_list();
    void rebuild_index();
};

#endif // STORAGE_MONITOR_HPP
