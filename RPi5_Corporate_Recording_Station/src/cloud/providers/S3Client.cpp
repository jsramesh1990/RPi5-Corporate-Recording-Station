#include "S3Client.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

namespace fs = std::filesystem;

S3Client::S3Client() : connected(false) {
    curl_global_init(CURL_GLOBAL_ALL);
}

S3Client::~S3Client() {
    disconnect();
    curl_global_cleanup();
}

bool S3Client::connect(const std::string& access_key,
                      const std::string& secret_key,
                      const std::string& region,
                      const std::string& bucket) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    this->access_key = access_key;
    this->secret_key = secret_key;
    this->region = region;
    this->bucket = bucket;
    
    // Build endpoint
    this->endpoint = "https://" + bucket + ".s3." + region + ".amazonaws.com";
    
    // Test connection by listing buckets
    std::string response;
    if (perform_s3_request("", "GET", "", &response)) {
        connected = true;
        std::cout << "Connected to AWS S3: " << bucket << " (" << region << ")" << std::endl;
        return true;
    }
    
    connected = false;
    std::cerr << "Failed to connect to AWS S3" << std::endl;
    return false;
}

bool S3Client::connect(const std::string& url,
                      const std::string& username,
                      const std::string& password) {
    // For S3, username is access key, password is secret key
    // URL format: bucket.region
    std::string bucket, region;
    
    // Parse URL (simplified)
    size_t dot_pos = url.find('.');
    if (dot_pos != std::string::npos) {
        bucket = url.substr(0, dot_pos);
        region = url.substr(dot_pos + 1);
    } else {
        bucket = url;
        region = "us-east-1";
    }
    
    return connect(username, password, region, bucket);
}

void S3Client::disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex);
    connected = false;
    access_key.clear();
    secret_key.clear();
    region.clear();
    bucket.clear();
    endpoint.clear();
}

bool S3Client::is_connected() const {
    std::lock_guard<std::mutex> lock(connection_mutex);
    return connected;
}

CloudAPI::ConnectionStatus S3Client::get_status() const {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    CloudAPI::ConnectionStatus status;
    status.is_connected = connected;
    status.provider = CloudAPI::Provider::PROVIDER_S3;
    status.url = endpoint;
    status.username = access_key;
    status.last_sync = std::chrono::system_clock::now();
    
    if (connected) {
        // Try to get bucket info
        std::string response;
        if (perform_s3_request("", "HEAD", "", &response)) {
            // Parse headers for quota info (simplified)
        }
    }
    
    return status;
}

bool S3Client::upload_file(const std::string& local_path,
                          const std::string& remote_path,
                          bool overwrite) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to S3" << std::endl;
        return false;
    }
    
    if (!fs::exists(local_path)) {
        std::cerr << "File does not exist: " << local_path << std::endl;
        return false;
    }
    
    // Check if file exists (optional)
    if (!overwrite) {
        std::string response;
        if (perform_s3_request(remote_path, "HEAD", "", &response)) {
            std::cerr << "File already exists: " << remote_path << std::endl;
            return false;
        }
    }
    
    // Open file
    std::ifstream file(local_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << local_path << std::endl;
        return false;
    }
    
    // Read file content
    std::string file_data((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    file.close();
    
    // Upload
    std::string response;
    if (perform_s3_request(remote_path, "PUT", file_data, &response)) {
        std::cout << "Uploaded: " << local_path << " -> s3://" << bucket << "/" << remote_path << std::endl;
        return true;
    }
    
    return false;
}

int S3Client::upload_files(const std::vector<std::string>& local_paths,
                          const std::string& remote_directory) {
    int uploaded = 0;
    for (const auto& path : local_paths) {
        std::string filename = fs::path(path).filename().string();
        std::string remote_path = remote_directory.empty() ? filename : remote_directory + "/" + filename;
        if (upload_file(path, remote_path, false)) {
            uploaded++;
        }
    }
    return uploaded;
}

bool S3Client::download_file(const std::string& remote_path,
                            const std::string& local_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to S3" << std::endl;
        return false;
    }
    
    std::string response;
    if (!perform_s3_request(remote_path, "GET", "", &response)) {
        return false;
    }
    
    // Write to file
    std::ofstream file(local_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << local_path << std::endl;
        return false;
    }
    
    file.write(response.c_str(), response.size());
    file.close();
    
    std::cout << "Downloaded: s3://" << bucket << "/" << remote_path << " -> " << local_path << std::endl;
    return true;
}

std::vector<std::string> S3Client::list_files(const std::string& remote_path) const {
    std::vector<std::string> files;
    
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to S3" << std::endl;
        return files;
    }
    
    // Build query string for list objects
    std::string query;
    if (!remote_path.empty()) {
        query = "?prefix=" + remote_path;
        if (remote_path.back() != '/') {
            query += "/";
        }
    }
    
    std::string response;
    if (!perform_s3_request(query, "GET", "", &response)) {
        return files;
    }
    
    // Parse XML response (simplified)
    // In production, would use XML parser
    std::string key_tag = "<Key>";
    size_t pos = 0;
    
    while ((pos = response.find(key_tag, pos)) != std::string::npos) {
        pos += key_tag.length();
        size_t end = response.find("</Key>", pos);
        if (end != std::string::npos) {
            std::string key = response.substr(pos, end - pos);
            // Skip directory markers
            if (!key.empty() && key.back() != '/') {
                // Get filename from key
                size_t last_slash = key.find_last_of('/');
                if (last_slash != std::string::npos && last_slash < key.length() - 1) {
                    files.push_back(key.substr(last_slash + 1));
                } else {
                    files.push_back(key);
                }
            }
            pos = end + 6;
        }
    }
    
    return files;
}

bool S3Client::delete_file(const std::string& remote_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to S3" << std::endl;
        return false;
    }
    
    std::string response;
    return perform_s3_request(remote_path, "DELETE", "", &response);
}

bool S3Client::perform_s3_request(const std::string& key,
                                 const std::string& method,
                                 const std::string& data,
                                 std::string* response) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    std::string url = endpoint;
    if (!key.empty()) {
        if (key.front() == '?') {
            url += key;
        } else {
            url += "/" + key;
        }
    }
    
    std::string response_data;
    long http_code = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Set AWS authentication headers
    std::string timestamp = get_timestamp();
    std::string signature = get_aws_signature(method, key, timestamp);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: AWS " + access_key + ":" + signature).c_str());
    headers = curl_slist_append(headers, ("Date: " + timestamp).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    
    // Add MD5 for PUT
    if (method == "PUT" && !data.empty()) {
        unsigned char md5[MD5_DIGEST_LENGTH];
        MD5(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), md5);
        
        std::stringstream md5_ss;
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            md5_ss << std::hex << std::setw(2) << std::setfill('0') << (int)md5[i];
        }
        headers = curl_slist_append(headers, ("Content-MD5: " + md5_ss.str()).c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set method
    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        if (!data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    
    // Set response callback
    if (response) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    }
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "S3 request failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    if (http_code >= 400) {
        std::cerr << "S3 error: HTTP " << http_code << std::endl;
        return false;
    }
    
    if (response) {
        *response = response_data;
    }
    
    return true;
}

std::string S3Client::get_aws_signature(const std::string& method,
                                       const std::string& uri,
                                       const std::string& timestamp) {
    // AWS Signature V1 (simplified)
    std::string string_to_sign = method + "\n" +
                                 "" + "\n" + // Content-MD5 (empty)
                                 "application/octet-stream" + "\n" +
                                 timestamp + "\n" +
                                 "/" + bucket + "/" + uri;
    
    // HMAC-SHA1 signature
    unsigned char hmac_result[SHA_DIGEST_LENGTH];
    HMAC(EVP_sha1(), secret_key.c_str(), secret_key.length(),
         reinterpret_cast<const unsigned char*>(string_to_sign.c_str()),
         string_to_sign.length(), hmac_result, nullptr);
    
    // Base64 encode
    std::string signature = base64_encode(hmac_result, SHA_DIGEST_LENGTH);
    return signature;
}

std::string S3Client::base64_encode(const unsigned char* data, size_t length) {
    const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) {
                result += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; j < i + 1; j++) {
            result += base64_chars[char_array_4[j]];
        }
        
        while (i++ < 3) {
            result += '=';
        }
    }
    
    return result;
}

std::string S3Client::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    char buffer[64];
    struct tm* tm_info = gmtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S +0000", tm_info);
    return std::string(buffer);
}

std::string S3Client::get_s3_url(const std::string& key) const {
    return endpoint + "/" + key;
}

size_t S3Client::write_callback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    if (data) {
        data->append((char*)contents, total_size);
    }
    return total_size;
}
