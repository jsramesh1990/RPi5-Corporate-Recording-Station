#ifndef CLOUD_UPLOADER_HPP
#define CLOUD_UPLOADER_HPP

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <memory>

#include "../public/CloudAPI.hpp"

/**
 * @brief Cloud provider interface
 */
class CloudProvider {
public:
    virtual ~CloudProvider() = default;
    
    virtual bool connect(const std::string& url,
                        const std::string& username,
                        const std::string& password) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual CloudAPI::ConnectionStatus get_status() const = 0;
    
    virtual bool upload_file(const std::string& local_path,
                            const std::string& remote_path,
                            bool overwrite = false) = 0;
    virtual int upload_files(const std::vector<std::string>& local_paths,
                            const std::string& remote_directory = "") = 0;
    virtual bool download_file(const std::string& remote_path,
                              const std::string& local_path) = 0;
    virtual std::vector<std::string> list_files(const std::string& remote_path = "") const = 0;
    virtual bool delete_file(const std::string& remote_path) = 0;
    
    virtual std::string get_name() const = 0;
    virtual CloudAPI::Provider get_type() const = 0;
};

/**
 * @brief Cloud upload manager with queue and retry logic
 */
class CloudUploader : public CloudAPI {
public:
    CloudUploader();
    ~CloudUploader();
    
    // CloudAPI implementation
    bool connect(Provider provider,
                const std::string& url,
                const std::string& username,
                const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;
    ConnectionStatus getStatus() const override;
    
    bool uploadFile(const std::string& local_path,
                   const std::string& remote_path = "",
                   bool overwrite = false) override;
    int uploadFiles(const std::vector<std::string>& local_paths,
                   const std::string& remote_directory = "") override;
    bool downloadFile(const std::string& remote_path,
                     const std::string& local_path) override;
    std::vector<std::string> listFiles(const std::string& remote_path = "") const override;
    bool deleteFile(const std::string& remote_path) override;
    
    int syncDirectory(const std::string& local_path,
                     const std::string& remote_path,
                     const std::string& direction = "upload") override;
    
    size_t getQueueSize() const override;
    void clearQueue() override;
    UploadProgress getCurrentUploadProgress() const override;
    
    void setOnUploadProgress(std::function<void(const UploadProgress&)> callback) override;
    void setOnUploadComplete(std::function<void(const std::string&, bool)> callback) override;
    void setOnSyncComplete(std::function<void(int, int)> callback) override;
    void setOnConnectionStatusChanged(std::function<void(bool)> callback) override;
    
    std::string getProviderName() const override;
    Provider getProviderType() const override;
    
    // Additional functionality
    void set_retry_count(int count);
    void set_retry_delay(int seconds);
    void set_max_concurrent_uploads(int max);
    void set_chunk_size(size_t size);
    void set_timeout(int seconds);
    void set_bandwidth_limit(size_t bytes_per_second);
    
    void pause_uploads();
    void resume_uploads();
    bool is_paused() const { return paused; }
    void cancel_upload(const std::string& filename);
    void retry_failed_uploads();
    
    // Queue management
    struct UploadTask {
        std::string local_path;
        std::string remote_path;
        bool overwrite;
        int retry_count;
        int max_retries;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point last_attempt;
        std::string error_message;
        bool completed;
        bool failed;
    };
    
    std::vector<UploadTask> get_queue() const;
    std::vector<UploadTask> get_failed_tasks() const;
    void clear_completed_tasks();
    
private:
    std::unique_ptr<CloudProvider> provider;
    ConnectionStatus status;
    mutable std::mutex status_mutex;
    
    // Queue
    std::queue<UploadTask> upload_queue;
    std::vector<UploadTask> active_uploads;
    std::vector<UploadTask> completed_uploads;
    std::vector<UploadTask> failed_uploads;
    mutable std::mutex queue_mutex;
    
    // Threading
    std::thread upload_thread;
    std::atomic<bool> running;
    std::atomic<bool> paused;
    mutable std::mutex thread_mutex;
    
    // Callbacks
    std::function<void(const UploadProgress&)> on_upload_progress;
    std::function<void(const std::string&, bool)> on_upload_complete;
    std::function<void(int, int)> on_sync_complete;
    std::function<void(bool)> on_connection_changed;
    
    // Configuration
    int retry_count = 3;
    int retry_delay = 5; // seconds
    int max_concurrent_uploads = 3;
    size_t chunk_size = 1024 * 1024; // 1MB
    int timeout = 300; // seconds
    size_t bandwidth_limit = 0; // 0 = unlimited
    
    // Current upload progress
    UploadProgress current_progress;
    mutable std::mutex progress_mutex;
    
    // Internal functions
    void upload_worker();
    void process_upload(UploadTask& task);
    void update_progress(const UploadTask& task, float progress);
    void complete_upload(const UploadTask& task, bool success);
    void update_status();
    bool create_provider(Provider provider_type);
    std::string get_provider_name(Provider provider) const;
    
    // Helper functions
    bool file_exists(const std::string& path) const;
    size_t get_file_size(const std::string& path) const;
    std::string get_filename(const std::string& path) const;
    std::string get_remote_path(const std::string& local_path,
                               const std::string& remote_directory) const;
    bool should_upload_file(const std::string& path) const;
};

#endif // CLOUD_UPLOADER_HPP
