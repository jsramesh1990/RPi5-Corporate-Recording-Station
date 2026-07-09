#include "SyncManager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

SyncManager::SyncManager() 
    : auto_sync_running(false), sync_in_progress(false), 
      sync_interval(3600), default_provider_index(0),
      logging_enabled(false) {
    
    // Set default paths
    default_local_path = "/var/lib/recording-station/recordings";
    default_remote_path = "/Recordings";
    
    // Initialize status
    current_status.is_syncing = false;
    current_status.last_sync_time = std::chrono::system_clock::now();
    current_status.files_synced = 0;
    current_status.files_failed = 0;
    current_status.progress_percent = 0.0f;
}

SyncManager::~SyncManager() {
    stop_auto_sync();
    disconnect_all();
}

// ============================================
// Provider Management
// ============================================

void SyncManager::add_provider(std::unique_ptr<CloudAPI> provider) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    ProviderEntry entry;
    entry.name = provider->getProviderName() + "_" + std::to_string(providers.size());
    entry.provider = std::move(provider);
    entry.connected = false;
    entry.last_sync = std::chrono::system_clock::now();
    
    providers.push_back(std::move(entry));
    
    log("INFO", "Added provider: " + entry.name);
}

void SyncManager::add_provider(const std::string& config_name,
                              CloudAPI::Provider type,
                              const std::string& url,
                              const std::string& username,
                              const std::string& password) {
    
    auto provider = std::make_unique<CloudUploader>();
    
    // Map provider type to CloudUploader::Provider
    CloudAPI::Provider provider_type = type;
    
    if (provider->connect(provider_type, url, username, password)) {
        std::lock_guard<std::mutex> lock(providers_mutex);
        
        ProviderEntry entry;
        entry.name = config_name;
        entry.provider = std::move(provider);
        entry.connected = true;
        entry.last_sync = std::chrono::system_clock::now();
        
        providers.push_back(std::move(entry));
        
        log("INFO", "Added provider: " + config_name + " (connected)");
        
        // Trigger connection callback
        if (on_connection_status) {
            on_connection_status(true);
        }
    } else {
        log("ERROR", "Failed to add provider: " + config_name);
    }
}

void SyncManager::remove_provider(size_t index) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    if (index < providers.size()) {
        std::string name = providers[index].name;
        providers[index].provider->disconnect();
        providers.erase(providers.begin() + index);
        log("INFO", "Removed provider: " + name);
    }
}

void SyncManager::remove_provider(const std::string& name) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    for (auto it = providers.begin(); it != providers.end(); ++it) {
        if (it->name == name) {
            it->provider->disconnect();
            providers.erase(it);
            log("INFO", "Removed provider: " + name);
            return;
        }
    }
}

bool SyncManager::connect_provider(size_t index) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    if (index >= providers.size()) {
        return false;
    }
    
    auto& entry = providers[index];
    
    if (entry.provider->isConnected()) {
        entry.connected = true;
        return true;
    }
    
    // Try to reconnect
    // Note: This assumes the provider was already configured
    // In practice, you'd need to store credentials
    entry.connected = entry.provider->isConnected();
    
    if (entry.connected) {
        log("INFO", "Connected provider: " + entry.name);
        if (on_connection_status) {
            on_connection_status(true);
        }
    } else {
        log("WARN", "Failed to connect provider: " + entry.name);
    }
    
    return entry.connected;
}

void SyncManager::connect_all() {
    for (size_t i = 0; i < get_provider_count(); i++) {
        connect_provider(i);
    }
}

void SyncManager::disconnect_provider(size_t index) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    if (index < providers.size()) {
        providers[index].provider->disconnect();
        providers[index].connected = false;
        log("INFO", "Disconnected provider: " + providers[index].name);
    }
}

void SyncManager::disconnect_all() {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    for (auto& entry : providers) {
        entry.provider->disconnect();
        entry.connected = false;
    }
    log("INFO", "Disconnected all providers");
}

size_t SyncManager::get_provider_count() const {
    std::lock_guard<std::mutex> lock(providers_mutex);
    return providers.size();
}

CloudAPI* SyncManager::get_provider(size_t index) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    if (index < providers.size()) {
        return providers[index].provider.get();
    }
    return nullptr;
}

// ============================================
// Sync Operations
// ============================================

bool SyncManager::upload_file(const std::string& local_path,
                             const std::string& remote_path,
                             size_t provider_index) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    if (provider_index >= providers.size()) {
        log("ERROR", "Invalid provider index: " + std::to_string(provider_index));
        return false;
    }
    
    auto& entry = providers[provider_index];
    
    if (!entry.connected) {
        log("WARN", "Provider not connected: " + entry.name);
        return false;
    }
    
    std::string remote = remote_path.empty() ? 
                         fs::path(local_path).filename().string() : 
                         remote_path;
    
    bool result = entry.provider->uploadFile(local_path, remote);
    
    if (result) {
        log("INFO", "Uploaded: " + local_path + " -> " + entry.name + ":" + remote);
    } else {
        log("ERROR", "Failed to upload: " + local_path);
    }
    
    return result;
}

int SyncManager::upload_files(const std::vector<std::string>& local_paths,
                             const std::string& remote_directory,
                             size_t provider_index) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    if (provider_index >= providers.size()) {
        log("ERROR", "Invalid provider index: " + std::to_string(provider_index));
        return 0;
    }
    
    auto& entry = providers[provider_index];
    
    if (!entry.connected) {
        log("WARN", "Provider not connected: " + entry.name);
        return 0;
    }
    
    int uploaded = entry.provider->uploadFiles(local_paths, remote_directory);
    log("INFO", "Uploaded " + std::to_string(uploaded) + " files to " + entry.name);
    
    return uploaded;
}

int SyncManager::upload_to_all(const std::string& local_path,
                              const std::string& remote_path) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    int total_uploaded = 0;
    std::string remote = remote_path.empty() ? 
                         fs::path(local_path).filename().string() : 
                         remote_path;
    
    for (auto& entry : providers) {
        if (entry.connected) {
            if (entry.provider->uploadFile(local_path, remote)) {
                total_uploaded++;
                log("INFO", "Uploaded to " + entry.name + ": " + local_path);
            }
        }
    }
    
    return total_uploaded;
}

int SyncManager::sync_directory(const std::string& local_path,
                               const std::string& remote_path,
                               const std::string& direction,
                               size_t provider_index) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    if (provider_index >= providers.size()) {
        log("ERROR", "Invalid provider index: " + std::to_string(provider_index));
        return 0;
    }
    
    auto& entry = providers[provider_index];
    
    if (!entry.connected) {
        log("WARN", "Provider not connected: " + entry.name);
        return 0;
    }
    
    int synced = entry.provider->syncDirectory(local_path, remote_path, direction);
    entry.last_sync = std::chrono::system_clock::now();
    
    log("INFO", "Synced " + std::to_string(synced) + " files with " + entry.name);
    
    return synced;
}

int SyncManager::sync_all(const std::string& local_path,
                         const std::string& remote_path,
                         const std::string& direction) {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    int total_synced = 0;
    
    for (auto& entry : providers) {
        if (entry.connected) {
            int synced = entry.provider->syncDirectory(local_path, remote_path, direction);
            total_synced += synced;
            entry.last_sync = std::chrono::system_clock::now();
            log("INFO", "Synced " + std::to_string(synced) + " files with " + entry.name);
        }
    }
    
    return total_synced;
}

// ============================================
// Auto-Sync
// ============================================

void SyncManager::start_auto_sync(int interval_seconds) {
    if (auto_sync_running) {
        return;
    }
    
    sync_interval = interval_seconds;
    auto_sync_running = true;
    sync_in_progress = false;
    
    sync_thread = std::thread(&SyncManager::sync_loop, this);
    
    log("INFO", "Auto-sync started (interval: " + std::to_string(interval_seconds) + "s)");
}

void SyncManager::stop_auto_sync() {
    if (!auto_sync_running) {
        return;
    }
    
    auto_sync_running = false;
    sync_cv.notify_all();
    
    if (sync_thread.joinable()) {
        sync_thread.join();
    }
    
    log("INFO", "Auto-sync stopped");
}

bool SyncManager::is_auto_sync_running() const {
    return auto_sync_running;
}

void SyncManager::set_auto_sync_interval(int interval_seconds) {
    sync_interval = interval_seconds;
    log("INFO", "Auto-sync interval set to " + std::to_string(interval_seconds) + "s");
}

int SyncManager::get_auto_sync_interval() const {
    return sync_interval;
}

bool SyncManager::is_syncing() const {
    return sync_in_progress;
}

void SyncManager::trigger_sync() {
    if (!sync_in_progress) {
        sync_cv.notify_one();
        log("INFO", "Manual sync triggered");
    } else {
        log("WARN", "Sync already in progress");
    }
}

void SyncManager::sync_loop() {
    while (auto_sync_running) {
        // Wait for next sync or trigger
        std::unique_lock<std::mutex> lock(providers_mutex);
        sync_cv.wait_for(lock, std::chrono::seconds(sync_interval),
            [this]() { return !auto_sync_running; });
        
        if (!auto_sync_running) {
            break;
        }
        
        // Perform sync
        perform_sync();
    }
}

void SyncManager::perform_sync() {
    if (sync_in_progress) {
        return;
    }
    
    sync_in_progress = true;
    
    // Update status
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        current_status.is_syncing = true;
        current_status.files_synced = 0;
        current_status.files_failed = 0;
        current_status.progress_percent = 0.0f;
    }
    
    // Notify sync start
    if (on_sync_start) {
        on_sync_start();
    }
    
    log("INFO", "Starting sync...");
    
    // Sync with all providers
    int total_synced = 0;
    int total_failed = 0;
    
    {
        std::lock_guard<std::mutex> lock(providers_mutex);
        
        for (auto& entry : providers) {
            if (!entry.connected) {
                continue;
            }
            
            try {
                int synced = entry.provider->syncDirectory(
                    default_local_path, 
                    default_remote_path, 
                    "upload"
                );
                
                total_synced += synced;
                entry.last_sync = std::chrono::system_clock::now();
                
                log("INFO", "Provider " + entry.name + " synced " + 
                    std::to_string(synced) + " files");
                
            } catch (const std::exception& e) {
                total_failed++;
                log("ERROR", "Sync failed for " + entry.name + ": " + e.what());
                
                if (on_sync_error) {
                    on_sync_error("Provider " + entry.name + ": " + e.what());
                }
            }
        }
    }
    
    // Update status
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        current_status.is_syncing = false;
        current_status.last_sync_time = std::chrono::system_clock::now();
        current_status.files_synced = total_synced;
        current_status.files_failed = total_failed;
        current_status.progress_percent = 100.0f;
    }
    
    // Notify completion
    if (on_sync_complete) {
        on_sync_complete(total_synced, total_failed);
    }
    
    log("INFO", "Sync complete: " + std::to_string(total_synced) + 
        " uploaded, " + std::to_string(total_failed) + " failed");
    
    sync_in_progress = false;
}

// ============================================
// Status & Information
// ============================================

std::vector<SyncManager::ProviderInfo> SyncManager::get_provider_info() const {
    std::lock_guard<std::mutex> lock(providers_mutex);
    
    std::vector<ProviderInfo> info_list;
    
    for (size_t i = 0; i < providers.size(); i++) {
        const auto& entry = providers[i];
        
        ProviderInfo info;
        info.index = i;
        info.name = entry.name;
        info.type = entry.provider->getProviderType();
        info.is_connected = entry.connected;
        info.status = entry.provider->getStatus();
        info.last_sync = entry.last_sync;
        
        // Try to get queue info from CloudUploader
        auto* uploader = dynamic_cast<CloudUploader*>(entry.provider.get());
        if (uploader) {
            info.upload_queue_size = uploader->getQueueSize();
            // Failed uploads count from status
            info.failed_uploads = 0;
        } else {
            info.upload_queue_size = 0;
            info.failed_uploads = 0;
        }
        
        info_list.push_back(info);
    }
    
    return info_list;
}

SyncManager::ProviderInfo SyncManager::get_provider_info(size_t index) const {
    auto info_list = get_provider_info();
    if (index < info_list.size()) {
        return info_list[index];
    }
    return ProviderInfo{};
}

SyncManager::ProviderInfo SyncManager::get_provider_info(const std::string& name) const {
    auto info_list = get_provider_info();
    for (const auto& info : info_list) {
        if (info.name == name) {
            return info;
        }
    }
    return ProviderInfo{};
}

SyncManager::SyncStatus SyncManager::get_sync_status() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return current_status;
}

// ============================================
// Configuration
// ============================================

void SyncManager::set_default_local_path(const std::string& path) {
    default_local_path = path;
    log("INFO", "Default local path set to: " + path);
}

void SyncManager::set_default_remote_path(const std::string& path) {
    default_remote_path = path;
    log("INFO", "Default remote path set to: " + path);
}

void SyncManager::set_default_provider(size_t index) {
    if (index < providers.size()) {
        default_provider_index = index;
        log("INFO", "Default provider set to index: " + std::to_string(index));
    }
}

void SyncManager::enable_auto_sync(bool enable) {
    if (enable && !auto_sync_running) {
        start_auto_sync(sync_interval);
    } else if (!enable && auto_sync_running) {
        stop_auto_sync();
    }
    auto_sync_enabled = enable;
}

void SyncManager::set_max_concurrent_uploads(int max) {
    max_concurrent_uploads = std::max(1, max);
    log("INFO", "Max concurrent uploads set to: " + std::to_string(max));
    
    // Apply to all providers
    std::lock_guard<std::mutex> lock(providers_mutex);
    for (auto& entry : providers) {
        auto* uploader = dynamic_cast<CloudUploader*>(entry.provider.get());
        if (uploader) {
            uploader->set_max_concurrent_uploads(max);
        }
    }
}

void SyncManager::set_bandwidth_limit(size_t bytes_per_second) {
    bandwidth_limit = bytes_per_second;
    log("INFO", "Bandwidth limit set to: " + std::to_string(bytes_per_second) + " B/s");
    
    // Apply to all providers
    std::lock_guard<std::mutex> lock(providers_mutex);
    for (auto& entry : providers) {
        auto* uploader = dynamic_cast<CloudUploader*>(entry.provider.get());
        if (uploader) {
            uploader->set_bandwidth_limit(bytes_per_second);
        }
    }
}

// ============================================
// Callbacks
// ============================================

void SyncManager::set_on_sync_complete(SyncCompleteCallback callback) {
    on_sync_complete = callback;
}

void SyncManager::set_on_upload_progress(UploadProgressCallback callback) {
    on_upload_progress = callback;
    
    // Forward to all providers
    std::lock_guard<std::mutex> lock(providers_mutex);
    for (auto& entry : providers) {
        auto* uploader = dynamic_cast<CloudUploader*>(entry.provider.get());
        if (uploader) {
            uploader->setOnUploadProgress(callback);
        }
    }
}

void SyncManager::set_on_connection_status(ConnectionStatusCallback callback) {
    on_connection_status = callback;
    
    // Check current status
    std::lock_guard<std::mutex> lock(providers_mutex);
    bool any_connected = false;
    for (const auto& entry : providers) {
        if (entry.connected) {
            any_connected = true;
            break;
        }
    }
    if (callback) {
        callback(any_connected);
    }
}

void SyncManager::set_on_sync_start(SyncStartCallback callback) {
    on_sync_start = callback;
}

void SyncManager::set_on_sync_error(SyncErrorCallback callback) {
    on_sync_error = callback;
}

// ============================================
// Logging
// ============================================

void SyncManager::enable_logging(bool enable) {
    logging_enabled = enable;
}

void SyncManager::set_log_file(const std::string& path) {
    log_file_path = path;
}

void SyncManager::log_message(const std::string& level, const std::string& message) {
    if (!logging_enabled) {
        return;
    }
    
    std::string timestamp = get_timestamp();
    std::string log_line = "[" + timestamp + "] [" + level + "] " + message + "\n";
    
    // Write to file if configured
    if (!log_file_path.empty()) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::ofstream file(log_file_path, std::ios::app);
        if (file.is_open()) {
            file << log_line;
            file.close();
        }
    }
    
    // Also print to console
    std::cout << log_line;
}

void SyncManager::log(const std::string& level, const std::string& message) {
    log_message(level, message);
}

// ============================================
// Helper Functions
// ============================================

std::string SyncManager::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}

bool SyncManager::should_sync_file(const std::string& path) const {
    // Skip hidden files
    if (fs::path(path).filename().string().front() == '.') {
        return false;
    }
    
    // Skip temporary files
    std::string ext = fs::path(path).extension().string();
    std::vector<std::string> skip_ext = {".tmp", ".swp", ".lock", ".log", ".part"};
    for (const auto& skip : skip_ext) {
        if (ext == skip) {
            return false;
        }
    }
    
    return true;
}

size_t SyncManager::get_file_size(const std::string& path) const {
    try {
        return fs::file_size(path);
    } catch (...) {
        return 0;
    }
}

void SyncManager::process_sync_queue(const std::vector<std::string>& files,
                                    const std::string& remote_path,
                                    CloudAPI* provider) {
    // Process files in chunks
    size_t chunk_size = max_concurrent_uploads;
    
    for (size_t i = 0; i < files.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, files.size());
        std::vector<std::string> chunk(files.begin() + i, files.begin() + end);
        
        // Upload chunk
        int uploaded = provider->uploadFiles(chunk, remote_path);
        
        // Update status
        {
            std::lock_guard<std::mutex> lock(status_mutex);
            current_status.files_synced += uploaded;
            current_status.progress_percent = 
                static_cast<float>(i + uploaded) / files.size() * 100.0f;
        }
        
        // Sleep if bandwidth limit is set
        if (bandwidth_limit > 0) {
            // Rough bandwidth limiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void SyncManager::update_status(const std::string& file, float progress) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.current_file = file;
    current_status.progress_percent = progress;
}

void SyncManager::notify_sync_complete(int success, int failed) {
    if (on_sync_complete) {
        on_sync_complete(success, failed);
    }
}

void SyncManager::notify_connection_status(bool connected) {
    if (on_connection_status) {
        on_connection_status(connected);
    }
}
