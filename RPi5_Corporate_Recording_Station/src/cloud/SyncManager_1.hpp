#ifndef SYNC_MANAGER_HPP
#define SYNC_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <map>

#include "../public/CloudAPI.hpp"
#include "CloudUploader.hpp"

/**
 * @brief Sync Manager - Orchestrates multiple cloud providers
 * 
 * Manages synchronization between local storage and multiple cloud providers.
 * Supports automatic background syncing with configurable intervals.
 */
class SyncManager {
public:
    SyncManager();
    ~SyncManager();
    
    // ============================================
    // Provider Management
    // ============================================
    
    /**
     * @brief Add a cloud provider
     * @param provider Unique pointer to CloudAPI implementation
     */
    void add_provider(std::unique_ptr<CloudAPI> provider);
    
    /**
     * @brief Add and configure a cloud provider
     * @param config_name Name for this provider configuration
     * @param type Provider type (Nextcloud, OneDrive, S3)
     * @param url Provider URL or endpoint
     * @param username Username or access key
     * @param password Password or secret key
     */
    void add_provider(const std::string& config_name,
                     CloudAPI::Provider type,
                     const std::string& url,
                     const std::string& username,
                     const std::string& password);
    
    /**
     * @brief Remove a provider by index
     */
    void remove_provider(size_t index);
    
    /**
     * @brief Remove a provider by name
     */
    void remove_provider(const std::string& name);
    
    /**
     * @brief Connect a provider
     */
    bool connect_provider(size_t index);
    
    /**
     * @brief Connect all providers
     */
    void connect_all();
    
    /**
     * @brief Disconnect a provider
     */
    void disconnect_provider(size_t index);
    
    /**
     * @brief Disconnect all providers
     */
    void disconnect_all();
    
    /**
     * @brief Get number of providers
     */
    size_t get_provider_count() const;
    
    /**
     * @brief Get provider by index
     */
    CloudAPI* get_provider(size_t index);
    
    // ============================================
    // Sync Operations
    // ============================================
    
    /**
     * @brief Upload a file to a specific provider
     * @param local_path Local file path
     * @param remote_path Remote file path (optional)
     * @param provider_index Provider to use (default: 0)
     * @return true if successful
     */
    bool upload_file(const std::string& local_path,
                    const std::string& remote_path = "",
                    size_t provider_index = 0);
    
    /**
     * @brief Upload files to a specific provider
     * @param local_paths Vector of local file paths
     * @param remote_directory Remote directory
     * @param provider_index Provider to use (default: 0)
     * @return Number of successful uploads
     */
    int upload_files(const std::vector<std::string>& local_paths,
                    const std::string& remote_directory = "",
                    size_t provider_index = 0);
    
    /**
     * @brief Upload to all connected providers
     * @param local_path Local file path
     * @param remote_path Remote file path (optional)
     * @return Number of successful uploads
     */
    int upload_to_all(const std::string& local_path,
                     const std::string& remote_path = "");
    
    /**
     * @brief Sync a local directory with cloud
     * @param local_path Local directory path
     * @param remote_path Remote directory path
     * @param direction "upload", "download", or "both"
     * @param provider_index Provider to use (default: 0)
     * @return Number of synced files
     */
    int sync_directory(const std::string& local_path,
                      const std::string& remote_path,
                      const std::string& direction = "upload",
                      size_t provider_index = 0);
    
    /**
     * @brief Sync with all connected providers
     * @param local_path Local directory path
     * @param remote_path Remote directory path
     * @param direction "upload", "download", or "both"
     * @return Total number of synced files across all providers
     */
    int sync_all(const std::string& local_path,
                const std::string& remote_path,
                const std::string& direction = "upload");
    
    // ============================================
    // Auto-Sync
    // ============================================
    
    /**
     * @brief Start automatic background syncing
     * @param interval_seconds Sync interval in seconds (default: 3600)
     */
    void start_auto_sync(int interval_seconds = 3600);
    
    /**
     * @brief Stop automatic background syncing
     */
    void stop_auto_sync();
    
    /**
     * @brief Check if auto-sync is running
     */
    bool is_auto_sync_running() const;
    
    /**
     * @brief Set auto-sync interval
     */
    void set_auto_sync_interval(int interval_seconds);
    
    /**
     * @brief Get auto-sync interval
     */
    int get_auto_sync_interval() const;
    
    /**
     * @brief Check if currently syncing
     */
    bool is_syncing() const;
    
    /**
     * @brief Trigger a manual sync (non-blocking)
     */
    void trigger_sync();
    
    // ============================================
    // Status & Information
    // ============================================
    
    /**
     * @brief Provider information structure
     */
    struct ProviderInfo {
        size_t index;
        std::string name;
        CloudAPI::Provider type;
        bool is_connected;
        CloudAPI::ConnectionStatus status;
        std::chrono::system_clock::time_point last_sync;
        size_t upload_queue_size;
        size_t failed_uploads;
    };
    
    /**
     * @brief Get information about all providers
     */
    std::vector<ProviderInfo> get_provider_info() const;
    
    /**
     * @brief Get provider info by index
     */
    ProviderInfo get_provider_info(size_t index) const;
    
    /**
     * @brief Get provider info by name
     */
    ProviderInfo get_provider_info(const std::string& name) const;
    
    /**
     * @brief Get sync status
     */
    struct SyncStatus {
        bool is_syncing;
        std::chrono::system_clock::time_point last_sync_time;
        int files_synced;
        int files_failed;
        std::string current_file;
        float progress_percent;
    };
    
    SyncStatus get_sync_status() const;
    
    // ============================================
    // Configuration
    // ============================================
    
    /**
     * @brief Configure sync paths
     */
    void set_default_local_path(const std::string& path);
    void set_default_remote_path(const std::string& path);
    void set_default_provider(size_t index);
    
    /**
     * @brief Enable/disable auto-sync
     */
    void enable_auto_sync(bool enable);
    
    /**
     * @brief Set maximum concurrent uploads
     */
    void set_max_concurrent_uploads(int max);
    
    /**
     * @brief Set bandwidth limit (bytes per second)
     */
    void set_bandwidth_limit(size_t bytes_per_second);
    
    // ============================================
    // Callbacks
    // ============================================
    
    using SyncCompleteCallback = std::function<void(int success, int failed)>;
    using UploadProgressCallback = std::function<void(const CloudAPI::UploadProgress&)>;
    using ConnectionStatusCallback = std::function<void(bool connected)>;
    using SyncStartCallback = std::function<void()>;
    using SyncErrorCallback = std::function<void(const std::string& error)>;
    
    void set_on_sync_complete(SyncCompleteCallback callback);
    void set_on_upload_progress(UploadProgressCallback callback);
    void set_on_connection_status(ConnectionStatusCallback callback);
    void set_on_sync_start(SyncStartCallback callback);
    void set_on_sync_error(SyncErrorCallback callback);
    
    // ============================================
    // Logging
    // ============================================
    
    void enable_logging(bool enable);
    void set_log_file(const std::string& path);
    void log_message(const std::string& level, const std::string& message);
    
private:
    // Provider management
    struct ProviderEntry {
        std::string name;
        std::unique_ptr<CloudAPI> provider;
        bool connected;
        std::chrono::system_clock::time_point last_sync;
    };
    
    std::vector<ProviderEntry> providers;
    mutable std::mutex providers_mutex;
    
    // Sync configuration
    std::string default_local_path;
    std::string default_remote_path;
    size_t default_provider_index = 0;
    bool auto_sync_enabled = true;
    int max_concurrent_uploads = 3;
    size_t bandwidth_limit = 0;
    
    // Auto-sync thread
    std::thread sync_thread;
    std::atomic<bool> auto_sync_running;
    std::atomic<bool> sync_in_progress;
    std::atomic<int> sync_interval;
    std::condition_variable sync_cv;
    
    // Sync status
    SyncStatus current_status;
    mutable std::mutex status_mutex;
    
    // Callbacks
    SyncCompleteCallback on_sync_complete;
    UploadProgressCallback on_upload_progress;
    ConnectionStatusCallback on_connection_status;
    SyncStartCallback on_sync_start;
    SyncErrorCallback on_sync_error;
    
    // Logging
    bool logging_enabled = false;
    std::string log_file_path;
    std::mutex log_mutex;
    
    // Internal functions
    void sync_loop();
    void perform_sync();
    void update_status(const std::string& file, float progress);
    void notify_sync_complete(int success, int failed);
    void notify_connection_status(bool connected);
    void log(const std::string& level, const std::string& message);
    
    // Helper functions
    std::string get_timestamp() const;
    bool should_sync_file(const std::string& path) const;
    size_t get_file_size(const std::string& path) const;
    void process_sync_queue(const std::vector<std::string>& files,
                           const std::string& remote_path,
                           CloudAPI* provider);
};

#endif // SYNC_MANAGER_HPP
