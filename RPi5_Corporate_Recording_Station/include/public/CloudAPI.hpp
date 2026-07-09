#ifndef CLOUD_API_HPP
#define CLOUD_API_HPP

#include <string>
#include <vector>
#include <functional>
#include <chrono>

/**
 * @brief Public API for cloud operations
 * 
 * This interface provides a stable API for uploading,
 * syncing, and managing cloud storage.
 */
class CloudAPI {
public:
    virtual ~CloudAPI() = default;
    
    /**
     * @brief Cloud provider enumeration
     */
    enum Provider {
        PROVIDER_NEXTCLOUD,
        PROVIDER_ONEDRIVE,
        PROVIDER_S3,
        PROVIDER_WEBDAV
    };
    
    /**
     * @brief Cloud connection status
     */
    struct ConnectionStatus {
        bool is_connected = false;
        Provider provider = PROVIDER_NEXTCLOUD;
        std::string url;
        std::string username;
        int64_t total_space = 0;
        int64_t used_space = 0;
        int64_t free_space = 0;
        std::chrono::system_clock::time_point last_sync;
        bool is_syncing = false;
        size_t queue_size = 0;
    };
    
    /**
     * @brief Upload progress information
     */
    struct UploadProgress {
        std::string filename;
        std::string remote_path;
        int64_t total_bytes = 0;
        int64_t uploaded_bytes = 0;
        float progress_percent = 0.0f;
        int64_t speed_bytes_per_sec = 0;
        int estimated_seconds_remaining = 0;
        bool is_complete = false;
        bool has_error = false;
        std::string error_message;
    };
    
    /**
     * @brief Connect to cloud provider
     * @param provider Provider type
     * @param url Provider URL
     * @param username Username
     * @param password Password or token
     * @return true if connected successfully
     */
    virtual bool connect(Provider provider,
                         const std::string& url,
                         const std::string& username,
                         const std::string& password) = 0;
    
    /**
     * @brief Disconnect from cloud provider
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief Check if connected
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Get connection status
     */
    virtual ConnectionStatus getStatus() const = 0;
    
    /**
     * @brief Upload a file to cloud
     * @param local_path Local file path
     * @param remote_path Remote path (optional)
     * @param overwrite Overwrite if exists
     * @return true if upload successful
     */
    virtual bool uploadFile(const std::string& local_path,
                            const std::string& remote_path = "",
                            bool overwrite = false) = 0;
    
    /**
     * @brief Upload multiple files
     * @param local_paths Vector of local file paths
     * @param remote_directory Remote directory
     * @return Number of successful uploads
     */
    virtual int uploadFiles(const std::vector<std::string>& local_paths,
                            const std::string& remote_directory = "") = 0;
    
    /**
     * @brief Download a file from cloud
     * @param remote_path Remote file path
     * @param local_path Local file path
     * @return true if download successful
     */
    virtual bool downloadFile(const std::string& remote_path,
                              const std::string& local_path) = 0;
    
    /**
     * @brief List files in remote directory
     * @param remote_path Remote directory path
     * @return Vector of file names
     */
    virtual std::vector<std::string> listFiles(const std::string& remote_path = "") const = 0;
    
    /**
     * @brief Delete file from cloud
     * @param remote_path Remote file path
     * @return true if successful
     */
    virtual bool deleteFile(const std::string& remote_path) = 0;
    
    /**
     * @brief Sync local directory with cloud
     * @param local_path Local directory path
     * @param remote_path Remote directory path
     * @param direction Sync direction (upload/download/both)
     * @return Number of synced files
     */
    virtual int syncDirectory(const std::string& local_path,
                              const std::string& remote_path,
                              const std::string& direction = "upload") = 0;
    
    /**
     * @brief Get upload queue size
     */
    virtual size_t getQueueSize() const = 0;
    
    /**
     * @brief Clear upload queue
     */
    virtual void clearQueue() = 0;
    
    /**
     * @brief Get current upload progress
     */
    virtual UploadProgress getCurrentUploadProgress() const = 0;
    
    /**
     * @brief Set onUploadProgress callback
     */
    virtual void setOnUploadProgress(std::function<void(const UploadProgress&)> callback) = 0;
    
    /**
     * @brief Set onUploadComplete callback
     */
    virtual void setOnUploadComplete(std::function<void(const std::string&, bool)> callback) = 0;
    
    /**
     * @brief Set onSyncComplete callback
     */
    virtual void setOnSyncComplete(std::function<void(int, int)> callback) = 0;
    
    /**
     * @brief Set onConnectionStatusChanged callback
     */
    virtual void setOnConnectionStatusChanged(std::function<void(bool)> callback) = 0;
    
    /**
     * @brief Get provider name
     */
    virtual std::string getProviderName() const = 0;
    
    /**
     * @brief Get provider type
     */
    virtual Provider getProviderType() const = 0;
};

#endif // CLOUD_API_HPP
