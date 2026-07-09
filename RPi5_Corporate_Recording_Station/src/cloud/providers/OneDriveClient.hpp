#ifndef ONEDRIVE_CLIENT_HPP
#define ONEDRIVE_CLIENT_HPP

#include <string>
#include <vector>
#include <map>
#include <curl/curl.h>
#include <json/json.h>

#include "../CloudUploader.hpp"

/**
 * @brief Microsoft OneDrive Graph API client
 * 
 * Implements OneDrive integration using Microsoft Graph API v1.0
 * Supports OAuth 2.0 authentication with refresh tokens
 */
class OneDriveClient : public CloudProvider {
public:
    OneDriveClient();
    ~OneDriveClient();
    
    // Connect with OAuth credentials (recommended)
    bool connect(const std::string& client_id,
                const std::string& client_secret,
                const std::string& refresh_token);
    
    // Connect with username/password (legacy, less secure)
    bool connect(const std::string& url,
                const std::string& username,
                const std::string& password) override;
    
    void disconnect() override;
    bool is_connected() const override;
    CloudAPI::ConnectionStatus get_status() const override;
    
    // File operations
    bool upload_file(const std::string& local_path,
                    const std::string& remote_path,
                    bool overwrite = false) override;
    int upload_files(const std::vector<std::string>& local_paths,
                    const std::string& remote_directory = "") override;
    bool download_file(const std::string& remote_path,
                      const std::string& local_path) override;
    std::vector<std::string> list_files(const std::string& remote_path = "") const override;
    bool delete_file(const std::string& remote_path) override;
    
    std::string get_name() const override { return "Microsoft OneDrive"; }
    CloudAPI::Provider get_type() const override {
        return CloudAPI::Provider::PROVIDER_ONEDRIVE;
    }
    
    // Additional OneDrive features
    bool create_folder(const std::string& remote_path);
    bool rename_file(const std::string& old_path, const std::string& new_path);
    bool move_file(const std::string& source_path, const std::string& dest_path);
    std::string get_drive_id() const;
    std::string get_user_info() const;
    
private:
    std::string access_token;
    std::string refresh_token;
    std::string client_id;
    std::string client_secret;
    std::string drive_id;
    std::string user_id;
    bool connected;
    std::chrono::system_clock::time_point token_expiry;
    
    static constexpr const char* API_BASE_URL = "https://graph.microsoft.com/v1.0";
    static constexpr const char* AUTH_BASE_URL = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
    static constexpr const char* UPLOAD_CHUNK_SIZE = "327680"; // 320KB chunks
    
    mutable std::mutex connection_mutex;
    mutable std::mutex token_mutex;
    
    // Authentication
    bool refresh_access_token();
    bool authenticate_with_device_code();
    bool get_user_drive();
    bool is_token_valid() const;
    
    // HTTP requests
    bool perform_graph_request(const std::string& endpoint,
                              const std::string& method = "GET",
                              const std::string& data = "",
                              std::string* response = nullptr,
                              const std::map<std::string, std::string>& headers = {});
    
    // Large file upload (for files > 4MB)
    bool upload_large_file(const std::string& local_path,
                          const std::string& remote_path,
                          bool overwrite);
    
    // Helper functions
    std::string url_encode(const std::string& str);
    std::string get_relative_path(const std::string& path) const;
    std::string get_parent_path(const std::string& path) const;
    std::string get_file_name(const std::string& path) const;
    Json::Value parse_response(const std::string& response);
    bool is_error_response(const Json::Value& response);
    std::string get_error_message(const Json::Value& response);
    
    // CURL callbacks
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* data);
    static size_t read_callback(void* ptr, size_t size, size_t nmemb, FILE* file);
    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t ultotal, curl_off_t ulnow);
};

#endif // ONEDRIVE_CLIENT_HPP
