#ifndef SYNC_MANAGER_HPP
#define SYNC_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include "../public/CloudAPI.hpp"

/**
 * @brief Sync Manager - Orchestrates multiple cloud providers
 */
class SyncManager {
public:
    SyncManager();
    ~SyncManager();
    
    // Provider management
    void add_provider(std::unique_ptr<CloudAPI> provider);
    void add_provider(const std::string& config_name,
                     CloudAPI::Provider type,
                     const std::string& url,
                     const std::string& username,
                     const std::string& password);
    bool connect_provider(size_t index);
    void disconnect_all();
    size_t get_provider_count() const { return providers.size(); }
    
    // Sync operations
    bool upload_file(const std::string& local_path,
                    const std::string& remote_path = "",
                    size_t provider_index = 0);
    int upload_files(const std::vector<std::string>& local_paths,
                    const std::string& remote_directory = "",
                    size_t provider_index = 0);
    int sync_all(const std::string& local_path,
                const std::string& remote_path);
    
    // Auto-sync
    void start_sync(int interval_seconds = 3600);
    void stop_sync();
    bool is_syncing() const;
    
    // Status
    struct ProviderInfo {
        size_t index;
        std::string name;
        CloudAPI::Provider type;
        bool is_connected;
        CloudAPI::ConnectionStatus status;
    };
    
    std::vector<ProviderInfo> get_provider_info() const;
    
    // Callbacks
    void set_on_sync_complete(std::function<void(int, int)> callback);
    void set_on_upload_progress(std::function<void(const CloudAPI::UploadProgress&)> callback);
    
private:
    std::vector<std::unique_ptr<CloudAPI>> providers;
    mutable std::mutex providers_mutex;
    
    // Sync thread
    std::thread sync_thread;
    std::atomic<bool> running;
    int sync_interval;
    
    // Callbacks
    std::function<void(int, int)> on_sync_complete;
    std::function<void(const CloudAPI::UploadProgress&)> on_upload_progress;
    
    // Internal functions
    void sync_loop();
};

#endif // SYNC_MANAGER_HPP
