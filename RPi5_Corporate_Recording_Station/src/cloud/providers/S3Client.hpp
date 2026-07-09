#ifndef S3_CLIENT_HPP
#define S3_CLIENT_HPP

#include <string>
#include <vector>
#include <map>
#include <curl/curl.h>

#include "../CloudUploader.hpp"

/**
 * @brief AWS S3 client implementation
 */
class S3Client : public CloudProvider {
public:
    S3Client();
    ~S3Client();
    
    bool connect(const std::string& access_key,
                const std::string& secret_key,
                const std::string& region,
                const std::string& bucket);
    bool connect(const std::string& url,
                const std::string& username,
                const std::string& password) override;
    void disconnect() override;
    bool is_connected() const override;
    CloudAPI::ConnectionStatus get_status() const override;
    
    bool upload_file(const std::string& local_path,
                    const std::string& remote_path,
                    bool overwrite = false) override;
    int upload_files(const std::vector<std::string>& local_paths,
                    const std::string& remote_directory = "") override;
    bool download_file(const std::string& remote_path,
                      const std::string& local_path) override;
    std::vector<std::string> list_files(const std::string& remote_path = "") const override;
    bool delete_file(const std::string& remote_path) override;
    
    std::string get_name() const override { return "AWS S3"; }
    CloudAPI::Provider get_type() const override {
        return CloudAPI::Provider::PROVIDER_S3;
    }
    
private:
    std::string access_key;
    std::string secret_key;
    std::string region;
    std::string bucket;
    std::string endpoint;
    bool connected;
    
    mutable std::mutex connection_mutex;
    
    // HTTP requests
    bool perform_s3_request(const std::string& key,
                           const std::string& method = "GET",
                           const std::string& data = "",
                           std::string* response = nullptr);
    
    // Helper functions
    std::string get_aws_signature(const std::string& method,
                                 const std::string& uri,
                                 const std::string& timestamp);
    std::string get_s3_url(const std::string& key) const;
};

#endif // S3_CLIENT_HPP
