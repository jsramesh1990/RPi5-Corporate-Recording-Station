#include "CloudUploader.hpp"
#include "providers/NextcloudClient.hpp"
#include "providers/OneDriveClient.hpp"
#include "providers/S3Client.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

CloudUploader::CloudUploader()
    : running(false), paused(false) {
    // Initialize status
    status.is_connected = false;
    status.is_syncing = false;
    status.queue_size = 0;
    status.last_sync = std::chrono::system_clock::now();
}

CloudUploader::~CloudUploader() {
    disconnect();
}

bool CloudUploader::connect(Provider provider,
                           const std::string& url,
                           const std::string& username,
                           const std::string& password) {
    std::lock_guard<std::mutex> lock(status_mutex);
    
    // Disconnect if already connected
    if (status.is_connected) {
        disconnect();
    }
    
    // Create provider
    if (!create_provider(provider)) {
        return false;
    }
    
    // Connect
    if (!provider->connect(url, username, password)) {
        provider.reset();
        status.is_connected = false;
        return false;
    }
    
    // Update status
    status.is_connected = true;
    status.provider = provider;
    status.url = url;
    status.username = username;
    status.last_sync = std::chrono::system_clock::now();
    status.queue_size = 0;
    status.is_syncing = false;
    
    // Start upload thread if not running
    if (!running) {
        running = true;
        paused = false;
        upload_thread = std::thread(&CloudUploader::upload_worker, this);
    }
    
    // Trigger callback
    if (on_connection_changed) {
        on_connection_changed(true);
    }
    
    std::cout << "Connected to " << get_provider_name(provider) << " cloud" << std::endl;
    return true;
}

void CloudUploader::disconnect() {
    if (!status.is_connected) {
        return;
    }
    
    // Stop upload thread
    running = false;
    if (upload_thread.joinable()) {
        upload_thread.join();
    }
    
    // Disconnect provider
    if (provider) {
        provider->disconnect();
        provider.reset();
    }
    
    // Update status
    status.is_connected = false;
    status.is_syncing = false;
    
    // Clear queues
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!upload_queue.empty()) upload_queue.pop();
        active_uploads.clear();
        completed_uploads.clear();
        failed_uploads.clear();
    }
    
    // Trigger callback
    if (on_connection_changed) {
        on_connection_changed(false);
    }
    
    std::cout << "Disconnected from cloud" << std::endl;
}

bool CloudUploader::isConnected() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return status.is_connected && provider && provider->is_connected();
}

CloudAPI::ConnectionStatus CloudUploader::getStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return status;
}

bool CloudUploader::uploadFile(const std::string& local_path,
                              const std::string& remote_path,
                              bool overwrite) {
    if (!isConnected()) {
        std::cerr << "Not connected to cloud" << std::endl;
        return false;
    }
    
    if (!file_exists(local_path)) {
        std::cerr << "File does not exist: " << local_path << std::endl;
        return false;
    }
    
    // Create upload task
    UploadTask task;
    task.local_path = local_path;
    task.remote_path = remote_path.empty() ? get_filename(local_path) : remote_path;
    task.overwrite = overwrite;
    task.retry_count = 0;
    task.max_retries = retry_count;
    task.created = std::chrono::system_clock::now();
    task.completed = false;
    task.failed = false;
    
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        upload_queue.push(task);
        status.queue_size = upload_queue.size() + active_uploads.size();
    }
    
    return true;
}

int CloudUploader::uploadFiles(const std::vector<std::string>& local_paths,
                              const std::string& remote_directory) {
    if (!isConnected()) {
        std::cerr << "Not connected to cloud" << std::endl;
        return 0;
    }
    
    int added = 0;
    for (const auto& path : local_paths) {
        if (file_exists(path)) {
            std::string remote_path = get_remote_path(path, remote_directory);
            if (uploadFile(path, remote_path, false)) {
                added++;
            }
        }
    }
    
    return added;
}

bool CloudUploader::downloadFile(const std::string& remote_path,
                                const std::string& local_path) {
    if (!isConnected()) {
        std::cerr << "Not connected to cloud" << std::endl;
        return false;
    }
    
    return provider->download_file(remote_path, local_path);
}

std::vector<std::string> CloudUploader::listFiles(const std::string& remote_path) const {
    if (!isConnected()) {
        std::cerr << "Not connected to cloud" << std::endl;
        return std::vector<std::string>();
    }
    
    return provider->list_files(remote_path);
}

bool CloudUploader::deleteFile(const std::string& remote_path) {
    if (!isConnected()) {
        std::cerr << "Not connected to cloud" << std::endl;
        return false;
    }
    
    return provider->delete_file(remote_path);
}

int CloudUploader::syncDirectory(const std::string& local_path,
                                const std::string& remote_path,
                                const std::string& direction) {
    if (!isConnected()) {
        std::cerr << "Not connected to cloud" << std::endl;
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(status_mutex);
    status.is_syncing = true;
    
    int uploaded = 0;
    int failed = 0;
    
    try {
        // Walk through local directory
        if (direction == "upload" || direction == "both") {
            for (const auto& entry : fs::recursive_directory_iterator(local_path)) {
                if (!fs::is_regular_file(entry.path())) continue;
                
                std::string rel_path = fs::relative(entry.path(), local_path).string();
                std::string remote_file = remote_path + "/" + rel_path;
                
                if (uploadFile(entry.path().string(), remote_file, true)) {
                    uploaded++;
                } else {
                    failed++;
                }
            }
        }
        
        // Download files from cloud
        if (direction == "download" || direction == "both") {
            auto files = provider->list_files(remote_path);
            for (const auto& file : files) {
                std::string local_file = local_path + "/" + file;
                if (provider->download_file(remote_path + "/" + file, local_file)) {
                    uploaded++;
                } else {
                    failed++;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Sync error: " << e.what() << std::endl;
    }
    
    status.is_syncing = false;
    status.last_sync = std::chrono::system_clock::now();
    
    // Trigger callback
    if (on_sync_complete) {
        on_sync_complete(uploaded, failed);
    }
    
    return uploaded;
}

size_t CloudUploader::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return upload_queue.size() + active_uploads.size();
}

void CloudUploader::clearQueue() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!upload_queue.empty()) upload_queue.pop();
    active_uploads.clear();
    completed_uploads.clear();
    failed_uploads.clear();
    status.queue_size = 0;
}

CloudAPI::UploadProgress CloudUploader::getCurrentUploadProgress() const {
    std::lock_guard<std::mutex> lock(progress_mutex);
    return current_progress;
}

void CloudUploader::setOnUploadProgress(std::function<void(const UploadProgress&)> callback) {
    on_upload_progress = callback;
}

void CloudUploader::setOnUploadComplete(std::function<void(const std::string&, bool)> callback) {
    on_upload_complete = callback;
}

void CloudUploader::setOnSyncComplete(std::function<void(int, int)> callback) {
    on_sync_complete = callback;
}

void CloudUploader::setOnConnectionStatusChanged(std::function<void(bool)> callback) {
    on_connection_changed = callback;
}

std::string CloudUploader::getProviderName() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return get_provider_name(status.provider);
}

CloudAPI::Provider CloudUploader::getProviderType() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return status.provider;
}

void CloudUploader::set_retry_count(int count) {
    retry_count = std::max(0, count);
}

void CloudUploader::set_retry_delay(int seconds) {
    retry_delay = std::max(1, seconds);
}

void CloudUploader::set_max_concurrent_uploads(int max) {
    max_concurrent_uploads = std::max(1, max);
}

void CloudUploader::set_chunk_size(size_t size) {
    chunk_size = std::max(size_t(1024), size);
}

void CloudUploader::set_timeout(int seconds) {
    timeout = std::max(1, seconds);
}

void CloudUploader::pause_uploads() {
    paused = true;
    std::cout << "Uploads paused" << std::endl;
}

void CloudUploader::resume_uploads() {
    paused = false;
    std::cout << "Uploads resumed" << std::endl;
}

void CloudUploader::cancel_upload(const std::string& filename) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    // Remove from active uploads
    active_uploads.erase(
        std::remove_if(active_uploads.begin(), active_uploads.end(),
            [&filename](const UploadTask& task) {
                return task.local_path == filename;
            }),
        active_uploads.end()
    );
    
    // Remove from queue
    std::queue<UploadTask> new_queue;
    while (!upload_queue.empty()) {
        auto task = upload_queue.front();
        upload_queue.pop();
        if (task.local_path != filename) {
            new_queue.push(task);
        }
    }
    upload_queue = new_queue;
    
    status.queue_size = upload_queue.size() + active_uploads.size();
}

void CloudUploader::retry_failed_uploads() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    for (auto& task : failed_uploads) {
        if (task.retry_count < task.max_retries) {
            task.retry_count = 0;
            task.failed = false;
            task.error_message.clear();
            upload_queue.push(task);
        }
    }
    failed_uploads.clear();
    
    status.queue_size = upload_queue.size() + active_uploads.size();
}

void CloudUploader::upload_worker() {
    while (running) {
        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Process uploads
        UploadTask task;
        bool has_task = false;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!upload_queue.empty()) {
                task = upload_queue.front();
                upload_queue.pop();
                has_task = true;
                active_uploads.push_back(task);
                status.queue_size = upload_queue.size() + active_uploads.size();
            }
        }
        
        if (has_task) {
            process_upload(task);
        } else {
            // No tasks, sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void CloudUploader::process_upload(UploadTask& task) {
    if (!isConnected()) {
        complete_upload(task, false);
        return;
    }
    
    // Update progress
    update_progress(task, 0.0f);
    
    // Get file size
    size_t file_size = get_file_size(task.local_path);
    
    // Upload file
    bool success = provider->upload_file(task.local_path, task.remote_path, task.overwrite);
    
    if (success) {
        // Upload successful
        task.completed = true;
        complete_upload(task, true);
    } else {
        // Upload failed
        task.retry_count++;
        task.last_attempt = std::chrono::system_clock::now();
        
        if (task.retry_count < task.max_retries) {
            // Retry later
            std::lock_guard<std::mutex> lock(queue_mutex);
            upload_queue.push(task);
            active_uploads.erase(
                std::remove_if(active_uploads.begin(), active_uploads.end(),
                    [&task](const UploadTask& t) {
                        return t.local_path == task.local_path;
                    }),
                active_uploads.end()
            );
            status.queue_size = upload_queue.size() + active_uploads.size();
        } else {
            // Failed permanently
            task.failed = true;
            task.error_message = "Max retries exceeded";
            complete_upload(task, false);
        }
    }
}

void CloudUploader::update_progress(const UploadTask& task, float progress) {
    std::lock_guard<std::mutex> lock(progress_mutex);
    
    current_progress.filename = task.local_path;
    current_progress.remote_path = task.remote_path;
    current_progress.total_bytes = get_file_size(task.local_path);
    current_progress.uploaded_bytes = current_progress.total_bytes * progress;
    current_progress.progress_percent = progress * 100;
    current_progress.is_complete = false;
    current_progress.has_error = false;
    
    if (on_upload_progress) {
        on_upload_progress(current_progress);
    }
}

void CloudUploader::complete_upload(const UploadTask& task, bool success) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    // Remove from active uploads
    active_uploads.erase(
        std::remove_if(active_uploads.begin(), active_uploads.end(),
            [&task](const UploadTask& t) {
                return t.local_path == task.local_path;
            }),
        active_uploads.end()
    );
    
    // Add to completed or failed
    if (success) {
        completed_uploads.push_back(task);
    } else {
        failed_uploads.push_back(task);
    }
    
    status.queue_size = upload_queue.size() + active_uploads.size();
    
    // Update progress
    {
        std::lock_guard<std::mutex> lock(progress_mutex);
        current_progress.filename = task.local_path;
        current_progress.is_complete = success;
        current_progress.has_error = !success;
        current_progress.progress_percent = success ? 100.0f : 0.0f;
        if (!success) {
            current_progress.error_message = task.error_message;
        }
    }
    
    // Trigger callback
    if (on_upload_complete) {
        on_upload_complete(task.local_path, success);
    }
    
    if (success) {
        std::cout << "Uploaded: " << task.local_path << " -> " << task.remote_path << std::endl;
    } else {
        std::cerr << "Failed to upload: " << task.local_path << " - " << task.error_message << std::endl;
    }
}

bool CloudUploader::create_provider(Provider provider_type) {
    switch (provider_type) {
        case Provider::PROVIDER_NEXTCLOUD:
            provider = std::make_unique<NextcloudClient>();
            break;
        case Provider::PROVIDER_ONEDRIVE:
            provider = std::make_unique<OneDriveClient>();
            break;
        case Provider::PROVIDER_S3:
            provider = std::make_unique<S3Client>();
            break;
        default:
            return false;
    }
    return true;
}

std::string CloudUploader::get_provider_name(Provider provider) const {
    switch (provider) {
        case Provider::PROVIDER_NEXTCLOUD: return "Nextcloud";
        case Provider::PROVIDER_ONEDRIVE: return "OneDrive";
        case Provider::PROVIDER_S3: return "AWS S3";
        case Provider::PROVIDER_WEBDAV: return "WebDAV";
        default: return "Unknown";
    }
}

bool CloudUploader::file_exists(const std::string& path) const {
    return fs::exists(path) && fs::is_regular_file(path);
}

size_t CloudUploader::get_file_size(const std::string& path) const {
    try {
        return fs::file_size(path);
    } catch (...) {
        return 0;
    }
}

std::string CloudUploader::get_filename(const std::string& path) const {
    return fs::path(path).filename().string();
}

std::string CloudUploader::get_remote_path(const std::string& local_path,
                                          const std::string& remote_directory) const {
    std::string filename = get_filename(local_path);
    if (remote_directory.empty()) {
        return filename;
    }
    return remote_directory + "/" + filename;
}

bool CloudUploader::should_upload_file(const std::string& path) const {
    // Skip hidden files
    if (fs::path(path).filename().string().front() == '.') {
        return false;
    }
    
    // Skip temporary files
    std::string ext = fs::path(path).extension().string();
    std::vector<std::string> skip_ext = {".tmp", ".swp", ".lock", ".log"};
    for (const auto& skip : skip_ext) {
        if (ext == skip) {
            return false;
        }
    }
    
    return true;
}
