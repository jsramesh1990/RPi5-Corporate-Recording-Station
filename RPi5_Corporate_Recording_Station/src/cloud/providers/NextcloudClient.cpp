#include "NextcloudClient.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <chrono>

NextcloudClient::NextcloudClient()
    : connected(false) {
    curl_global_init(CURL_GLOBAL_ALL);
}

NextcloudClient::~NextcloudClient() {
    disconnect();
    curl_global_cleanup();
}

bool NextcloudClient::connect(const std::string& url,
                             const std::string& username,
                             const std::string& password) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    this->base_url = url;
    this->username = username;
    this->password = password;
    
    // Ensure URL ends with /
    if (!this->base_url.empty() && this->base_url.back() != '/') {
        this->base_url += '/';
    }
    
    // Set WebDAV URL
    this->webdav_url = this->base_url + "remote.php/dav/files/" + username + "/";
    
    // Test connection
    std::string test_url = webdav_url;
    std::string response;
    
    if (perform_request(test_url, "PROPFIND", "", &response)) {
        connected = true;
        std::cout << "Connected to Nextcloud: " << url << std::endl;
        return true;
    }
    
    connected = false;
    std::cerr << "Failed to connect to Nextcloud: " << url << std::endl;
    return false;
}

void NextcloudClient::disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex);
    connected = false;
    username.clear();
    password.clear();
    base_url.clear();
    webdav_url.clear();
}

bool NextcloudClient::is_connected() const {
    std::lock_guard<std::mutex> lock(connection_mutex);
    return connected;
}

CloudAPI::ConnectionStatus NextcloudClient::get_status() const {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    CloudAPI::ConnectionStatus status;
    status.is_connected = connected;
    status.provider = CloudAPI::Provider::PROVIDER_NEXTCLOUD;
    status.url = base_url;
    status.username = username;
    status.last_sync = std::chrono::system_clock::now();
    
    if (connected) {
        // Try to get quota
        std::string quota_url = base_url + "remote.php/dav/files/" + username + "/";
        std::string response;
        
        if (perform_request(quota_url, "PROPFIND", "", &response)) {
            // Parse quota from response (simplified)
            // In production, would parse XML response
        }
    }
    
    return status;
}

bool NextcloudClient::upload_file(const std::string& local_path,
                                 const std::string& remote_path,
                                 bool overwrite) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to Nextcloud" << std::endl;
        return false;
    }
    
    return perform_upload(local_path, remote_path, overwrite);
}

int NextcloudClient::upload_files(const std::vector<std::string>& local_paths,
                                 const std::string& remote_directory) {
    if (!connected) {
        std::cerr << "Not connected to Nextcloud" << std::endl;
        return 0;
    }
    
    int uploaded = 0;
    for (const auto& path : local_paths) {
        std::string filename = std::filesystem::path(path).filename().string();
        std::string remote_path = remote_directory.empty() ? filename : remote_directory + "/" + filename;
        
        if (upload_file(path, remote_path, false)) {
            uploaded++;
        }
    }
    
    return uploaded;
}

bool NextcloudClient::download_file(const std::string& remote_path,
                                   const std::string& local_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to Nextcloud" << std::endl;
        return false;
    }
    
    return perform_download(remote_path, local_path);
}

std::vector<std::string> NextcloudClient::list_files(const std::string& remote_path) const {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to Nextcloud" << std::endl;
        return std::vector<std::string>();
    }
    
    return perform_propfind(remote_path);
}

bool NextcloudClient::delete_file(const std::string& remote_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected) {
        std::cerr << "Not connected to Nextcloud" << std::endl;
        return false;
    }
    
    std::string full_url = get_full_url(remote_path);
    std::string response;
    
    return perform_request(full_url, "DELETE", "", &response);
}

bool NextcloudClient::perform_request(const std::string& url,
                                     const std::string& method,
                                     const std::string& data,
                                     std::string* response) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    std::string response_data;
    if (response) {
        response->clear();
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set authentication
    std::string auth = username + ":" + password;
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
    
    // Set method
    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        }
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "PROPFIND") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    } else if (method == "MKCOL") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "MKCOL");
    } else if (method == "MOVE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "MOVE");
    }
    
    // Set headers for WebDAV
    struct curl_slist* headers = nullptr;
    if (method == "PROPFIND") {
        headers = curl_slist_append(headers, "Depth: 1");
        headers = curl_slist_append(headers, "Content-Type: application/xml");
        const char* propfind_xml = R"(<?xml version="1.0"?>
            <d:propfind xmlns:d="DAV:">
                <d:prop>
                    <d:displayname/>
                    <d:getcontentlength/>
                    <d:getlastmodified/>
                    <d:resourcetype/>
                </d:prop>
            </d:propfind>)";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, propfind_xml);
    }
    
    if (!headers) {
        headers = curl_slist_append(headers, "Accept: application/json");
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    if (response) {
        *response = response_data;
    }
    
    return true;
}

bool NextcloudClient::perform_upload(const std::string& local_path,
                                    const std::string& remote_path,
                                    bool overwrite) {
    std::string full_url = get_full_url(remote_path);
    
    // Check if file exists
    if (!overwrite) {
        std::string response;
        if (perform_request(full_url, "PROPFIND", "", &response)) {
            // File exists and overwrite is false
            std::cerr << "File already exists: " << remote_path << std::endl;
            return false;
        }
    }
    
    // Open file for reading
    FILE* file = fopen(local_path.c_str(), "rb");
    if (!file) {
        std::cerr << "Failed to open file: " << local_path << std::endl;
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(file);
        return false;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    
    // Set authentication
    std::string auth = username + ":" + password;
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
    
    // Set upload method
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);
    
    // Set file to upload
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_size);
    
    // Set read callback
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    
    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    if (!overwrite) {
        headers = curl_slist_append(headers, "If-None-Match: *");
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    
    // Perform upload
    CURLcode res = curl_easy_perform(curl);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    fclose(file);
    
    return res == CURLE_OK;
}

bool NextcloudClient::perform_download(const std::string& remote_path,
                                      const std::string& local_path) {
    std::string full_url = get_full_url(remote_path);
    
    // Open file for writing
    FILE* file = fopen(local_path.c_str(), "wb");
    if (!file) {
        std::cerr << "Failed to open file for writing: " << local_path << std::endl;
        return false;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(file);
        return false;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    
    // Set authentication
    std::string auth = username + ":" + password;
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
    
    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Perform download
    CURLcode res = curl_easy_perform(curl);
    
    // Cleanup
    curl_easy_cleanup(curl);
    fclose(file);
    
    return res == CURLE_OK;
}

std::vector<std::string> NextcloudClient::perform_propfind(const std::string& path) {
    std::vector<std::string> files;
    std::string full_url = get_full_url(path);
    std::string response;
    
    if (!perform_request(full_url, "PROPFIND", "", &response)) {
        return files;
    }
    
    // Parse XML response (simplified)
    // In production, would use a proper XML parser
    std::string href_tag = "<d:href>";
    size_t pos = 0;
    
    while ((pos = response.find(href_tag, pos)) != std::string::npos) {
        pos += href_tag.length();
        size_t end = response.find("</d:href>", pos);
        if (end != std::string::npos) {
            std::string href = response.substr(pos, end - pos);
            // Decode URL encoding
            href = url_decode(href);
            
            // Remove parent directory
            if (href != "/" && href.find("/") != std::string::npos) {
                size_t last_slash = href.find_last_of('/');
                if (last_slash != std::string::npos && last_slash < href.length() - 1) {
                    files.push_back(href.substr(last_slash + 1));
                }
            }
            pos = end + 9;
        }
    }
    
    return files;
}

std::string NextcloudClient::get_full_url(const std::string& path) const {
    std::string full_url = webdav_url;
    if (!path.empty() && path.front() != '/') {
        full_url += path;
    } else if (!path.empty()) {
        full_url += path.substr(1);
    }
    return full_url;
}

std::string NextcloudClient::url_encode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return str;
    }
    
    char* encoded = curl_easy_escape(curl, str.c_str(), str.length());
    std::string result = encoded;
    curl_free(encoded);
    curl_easy_cleanup(curl);
    
    return result;
}

std::string NextcloudClient::url_decode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return str;
    }
    
    int out_len = 0;
    char* decoded = curl_easy_unescape(curl, str.c_str(), str.length(), &out_len);
    std::string result(decoded, out_len);
    curl_free(decoded);
    curl_easy_cleanup(curl);
    
    return result;
}

size_t NextcloudClient::write_callback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    if (data) {
        data->append((char*)contents, total_size);
    }
    return total_size;
}

size_t NextcloudClient::read_callback(void* ptr, size_t size, size_t nmemb, FILE* file) {
    return fread(ptr, size, nmemb, file);
}
