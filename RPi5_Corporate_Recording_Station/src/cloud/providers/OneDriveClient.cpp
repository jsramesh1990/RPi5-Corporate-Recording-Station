#include "OneDriveClient.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

OneDriveClient::OneDriveClient() 
    : connected(false) {
    curl_global_init(CURL_GLOBAL_ALL);
}

OneDriveClient::~OneDriveClient() {
    disconnect();
    curl_global_cleanup();
}

bool OneDriveClient::connect(const std::string& client_id,
                            const std::string& client_secret,
                            const std::string& refresh_token) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    this->client_id = client_id;
    this->client_secret = client_secret;
    this->refresh_token = refresh_token;
    
    // Refresh token to get access token
    if (!refresh_access_token()) {
        connected = false;
        return false;
    }
    
    // Get user drive info
    if (!get_user_drive()) {
        connected = false;
        return false;
    }
    
    connected = true;
    std::cout << "Connected to Microsoft OneDrive" << std::endl;
    return true;
}

bool OneDriveClient::connect(const std::string& url,
                            const std::string& username,
                            const std::string& password) {
    std::cerr << "Username/password authentication not supported for OneDrive. ";
    std::cerr << "Use OAuth2 with client ID and refresh token." << std::endl;
    return false;
}

void OneDriveClient::disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex);
    connected = false;
    access_token.clear();
    refresh_token.clear();
    client_id.clear();
    client_secret.clear();
    drive_id.clear();
    user_id.clear();
}

bool OneDriveClient::is_connected() const {
    std::lock_guard<std::mutex> lock(connection_mutex);
    return connected && is_token_valid();
}

CloudAPI::ConnectionStatus OneDriveClient::get_status() const {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    CloudAPI::ConnectionStatus status;
    status.is_connected = connected && is_token_valid();
    status.provider = CloudAPI::Provider::PROVIDER_ONEDRIVE;
    status.url = API_BASE_URL;
    status.username = user_id;
    status.last_sync = std::chrono::system_clock::now();
    
    if (connected && is_token_valid()) {
        // Try to get drive quota
        std::string endpoint = "/drives/" + drive_id;
        std::string response;
        
        if (perform_graph_request(endpoint, "GET", "", &response)) {
            auto json = parse_response(response);
            if (json.isObject() && json["quota"].isObject()) {
                status.total_space = json["quota"]["total"].asInt64();
                status.used_space = json["quota"]["used"].asInt64();
                status.free_space = status.total_space - status.used_space;
            }
        }
    }
    
    return status;
}

bool OneDriveClient::upload_file(const std::string& local_path,
                                const std::string& remote_path,
                                bool overwrite) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected || !is_token_valid()) {
        std::cerr << "Not connected to OneDrive" << std::endl;
        return false;
    }
    
    // Check if file exists
    if (!fs::exists(local_path)) {
        std::cerr << "File does not exist: " << local_path << std::endl;
        return false;
    }
    
    std::string relative_path = get_relative_path(remote_path);
    std::string file_name = get_file_name(relative_path);
    std::string parent_path = get_parent_path(relative_path);
    
    // Create parent folder if needed
    if (!parent_path.empty() && parent_path != "/") {
        create_folder(parent_path);
    }
    
    // Get file size
    size_t file_size = fs::file_size(local_path);
    
    // For small files (< 4MB), use simple upload
    if (file_size < 4 * 1024 * 1024) {
        // Build endpoint
        std::string endpoint;
        if (parent_path.empty() || parent_path == "/") {
            endpoint = "/me/drive/root:/" + url_encode(file_name) + ":/content";
        } else {
            endpoint = "/me/drive/root:/" + url_encode(parent_path) + "/" + 
                      url_encode(file_name) + ":/content";
        }
        
        // Read file
        std::ifstream file(local_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << local_path << std::endl;
            return false;
        }
        
        std::string file_data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
        file.close();
        
        // Upload
        std::string response;
        std::map<std::string, std::string> headers;
        if (!overwrite) {
            headers["If-None-Match"] = "*";
        }
        
        if (perform_graph_request(endpoint, "PUT", file_data, &response, headers)) {
            std::cout << "Uploaded: " << local_path << " -> " << remote_path << std::endl;
            return true;
        }
    } else {
        // Large file upload with chunking
        return upload_large_file(local_path, remote_path, overwrite);
    }
    
    return false;
}

bool OneDriveClient::upload_large_file(const std::string& local_path,
                                      const std::string& remote_path,
                                      bool overwrite) {
    // Create upload session
    std::string relative_path = get_relative_path(remote_path);
    std::string file_name = get_file_name(relative_path);
    std::string parent_path = get_parent_path(relative_path);
    
    std::string endpoint;
    if (parent_path.empty() || parent_path == "/") {
        endpoint = "/me/drive/root:/" + url_encode(file_name) + ":/createUploadSession";
    } else {
        endpoint = "/me/drive/root:/" + url_encode(parent_path) + "/" + 
                  url_encode(file_name) + ":/createUploadSession";
    }
    
    std::string response;
    if (!perform_graph_request(endpoint, "POST", "{}", &response)) {
        return false;
    }
    
    auto json = parse_response(response);
    if (!json.isObject() || !json["uploadUrl"].isString()) {
        return false;
    }
    
    std::string upload_url = json["uploadUrl"].asString();
    
    // Upload in chunks
    const size_t chunk_size = 320 * 1024; // 320KB chunks
    std::ifstream file(local_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    size_t file_size = fs::file_size(local_path);
    size_t bytes_uploaded = 0;
    bool success = true;
    
    while (bytes_uploaded < file_size) {
        size_t remaining = file_size - bytes_uploaded;
        size_t current_chunk_size = std::min(chunk_size, remaining);
        
        std::string chunk(current_chunk_size, '\0');
        file.read(&chunk[0], current_chunk_size);
        
        // Build range header
        std::string range = "bytes " + std::to_string(bytes_uploaded) + "-" +
                           std::to_string(bytes_uploaded + current_chunk_size - 1) + "/" +
                           std::to_string(file_size);
        
        // Upload chunk
        CURL* curl = curl_easy_init();
        if (!curl) {
            success = false;
            break;
        }
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Content-Range: " + range).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
        
        curl_easy_setopt(curl, CURLOPT_URL, upload_url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, chunk.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, current_chunk_size);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        
        std::string chunk_response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk_response);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            success = false;
            break;
        }
        
        bytes_uploaded += current_chunk_size;
        std::cout << "Upload progress: " << (bytes_uploaded * 100 / file_size) << "%" << std::endl;
    }
    
    file.close();
    return success;
}

bool OneDriveClient::download_file(const std::string& remote_path,
                                  const std::string& local_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected || !is_token_valid()) {
        std::cerr << "Not connected to OneDrive" << std::endl;
        return false;
    }
    
    std::string relative_path = get_relative_path(remote_path);
    std::string file_name = get_file_name(relative_path);
    std::string parent_path = get_parent_path(relative_path);
    
    // Build endpoint
    std::string endpoint;
    if (parent_path.empty() || parent_path == "/") {
        endpoint = "/me/drive/root:/" + url_encode(file_name) + ":/content";
    } else {
        endpoint = "/me/drive/root:/" + url_encode(parent_path) + "/" + 
                  url_encode(file_name) + ":/content";
    }
    
    // Download
    std::string response;
    if (perform_graph_request(endpoint, "GET", "", &response)) {
        // Write to file
        std::ofstream file(local_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << local_path << std::endl;
            return false;
        }
        file.write(response.c_str(), response.size());
        file.close();
        
        std::cout << "Downloaded: " << remote_path << " -> " << local_path << std::endl;
        return true;
    }
    
    return false;
}

std::vector<std::string> OneDriveClient::list_files(const std::string& remote_path) const {
    std::vector<std::string> files;
    
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected || !is_token_valid()) {
        std::cerr << "Not connected to OneDrive" << std::endl;
        return files;
    }
    
    std::string relative_path = get_relative_path(remote_path);
    
    // Build endpoint
    std::string endpoint;
    if (relative_path.empty() || relative_path == "/") {
        endpoint = "/me/drive/root/children";
    } else {
        endpoint = "/me/drive/root:/" + url_encode(relative_path) + ":/children";
    }
    
    std::string response;
    if (!perform_graph_request(endpoint, "GET", "", &response)) {
        return files;
    }
    
    auto json = parse_response(response);
    if (!json.isObject() || !json["value"].isArray()) {
        return files;
    }
    
    for (const auto& item : json["value"]) {
        if (item.isObject() && item["name"].isString()) {
            files.push_back(item["name"].asString());
        }
    }
    
    return files;
}

bool OneDriveClient::delete_file(const std::string& remote_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected || !is_token_valid()) {
        std::cerr << "Not connected to OneDrive" << std::endl;
        return false;
    }
    
    std::string relative_path = get_relative_path(remote_path);
    
    // Build endpoint
    std::string endpoint;
    if (relative_path.empty() || relative_path == "/") {
        endpoint = "/me/drive/root:/" + url_encode(relative_path);
    } else {
        endpoint = "/me/drive/root:/" + url_encode(relative_path);
    }
    
    return perform_graph_request(endpoint, "DELETE", "", nullptr);
}

bool OneDriveClient::create_folder(const std::string& remote_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected || !is_token_valid()) {
        std::cerr << "Not connected to OneDrive" << std::endl;
        return false;
    }
    
    std::string relative_path = get_relative_path(remote_path);
    std::string folder_name = get_file_name(relative_path);
    std::string parent_path = get_parent_path(relative_path);
    
    // Check if folder already exists
    auto existing = list_files(parent_path);
    for (const auto& file : existing) {
        if (file == folder_name) {
            return true; // Already exists
        }
    }
    
    // Build endpoint
    std::string endpoint;
    if (parent_path.empty() || parent_path == "/") {
        endpoint = "/me/drive/root/children";
    } else {
        endpoint = "/me/drive/root:/" + url_encode(parent_path) + ":/children";
    }
    
    // Build request body
    Json::Value body;
    body["name"] = folder_name;
    body["folder"] = Json::Value();
    body["@microsoft.graph.conflictBehavior"] = "rename";
    
    Json::StreamWriterBuilder writer;
    std::string request_body = Json::writeString(writer, body);
    
    return perform_graph_request(endpoint, "POST", request_body, nullptr);
}

bool OneDriveClient::rename_file(const std::string& old_path, const std::string& new_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected || !is_token_valid()) {
        std::cerr << "Not connected to OneDrive" << std::endl;
        return false;
    }
    
    std::string relative_old = get_relative_path(old_path);
    std::string new_name = get_file_name(new_path);
    
    // Build endpoint
    std::string endpoint = "/me/drive/root:/" + url_encode(relative_old);
    
    // Build request body
    Json::Value body;
    body["name"] = new_name;
    
    Json::StreamWriterBuilder writer;
    std::string request_body = Json::writeString(writer, body);
    
    return perform_graph_request(endpoint, "PATCH", request_body, nullptr);
}

bool OneDriveClient::move_file(const std::string& source_path, const std::string& dest_path) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    
    if (!connected || !is_token_valid()) {
        std::cerr << "Not connected to OneDrive" << std::endl;
        return false;
    }
    
    std::string relative_source = get_relative_path(source_path);
    std::string relative_dest = get_relative_path(dest_path);
    std::string dest_parent = get_parent_path(relative_dest);
    
    // Build endpoint
    std::string endpoint = "/me/drive/root:/" + url_encode(relative_source);
    
    // Build request body
    Json::Value body;
    if (!dest_parent.empty() && dest_parent != "/") {
        body["parentReference"]["path"] = "/drive/root:/" + url_encode(dest_parent);
    } else {
        body["parentReference"]["path"] = "/drive/root";
    }
    body["name"] = get_file_name(relative_dest);
    
    Json::StreamWriterBuilder writer;
    std::string request_body = Json::writeString(writer, body);
    
    return perform_graph_request(endpoint, "PATCH", request_body, nullptr);
}

bool OneDriveClient::refresh_access_token() {
    std::lock_guard<std::mutex> lock(token_mutex);
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    // Build request body
    std::string post_fields = 
        "client_id=" + url_encode(client_id) +
        "&client_secret=" + url_encode(client_secret) +
        "&refresh_token=" + url_encode(refresh_token) +
        "&grant_type=refresh_token";
    
    curl_easy_setopt(curl, CURLOPT_URL, AUTH_BASE_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "Token refresh failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    // Parse response
    auto json = parse_response(response);
    if (!json.isObject() || !json["access_token"].isString()) {
        std::cerr << "Invalid token response" << std::endl;
        return false;
    }
    
    access_token = json["access_token"].asString();
    if (json["refresh_token"].isString()) {
        refresh_token = json["refresh_token"].asString();
    }
    
    // Set expiry (default to 1 hour from now)
    token_expiry = std::chrono::system_clock::now() + std::chrono::hours(1);
    
    return true;
}

bool OneDriveClient::get_user_drive() {
    std::string response;
    if (!perform_graph_request("/me/drive", "GET", "", &response)) {
        return false;
    }
    
    auto json = parse_response(response);
    if (!json.isObject() || !json["id"].isString()) {
        return false;
    }
    
    drive_id = json["id"].asString();
    if (json["owner"].isObject() && json["owner"]["user"].isObject()) {
        user_id = json["owner"]["user"]["id"].asString();
    }
    
    return true;
}

bool OneDriveClient::is_token_valid() const {
    auto now = std::chrono::system_clock::now();
    // Check if token is still valid (with 5 minute buffer)
    auto buffer = std::chrono::minutes(5);
    return now + buffer < token_expiry;
}

bool OneDriveClient::perform_graph_request(const std::string& endpoint,
                                          const std::string& method,
                                          const std::string& data,
                                          std::string* response,
                                          const std::map<std::string, std::string>& headers) {
    // Check if token needs refresh
    if (!is_token_valid()) {
        if (!refresh_access_token()) {
            return false;
        }
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    std::string url = std::string(API_BASE_URL) + endpoint;
    std::string response_data;
    long http_code = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Set headers
    struct curl_slist* curl_headers = nullptr;
    curl_headers = curl_slist_append(curl_headers, ("Authorization: Bearer " + access_token).c_str());
    curl_headers = curl_slist_append(curl_headers, "Content-Type: application/json");
    curl_headers = curl_slist_append(curl_headers, "Accept: application/json");
    
    for (const auto& [key, value] : headers) {
        curl_headers = curl_slist_append(curl_headers, (key + ": " + value).c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    
    // Set method
    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        }
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (!data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (!data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        }
    }
    
    // Set response callback
    if (response) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    }
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(curl_headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "Graph API request failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    if (http_code >= 400) {
        std::cerr << "Graph API error: HTTP " << http_code << std::endl;
        if (!response_data.empty()) {
            auto json = parse_response(response_data);
            if (json.isObject()) {
                std::cerr << "Error: " << get_error_message(json) << std::endl;
            }
        }
        return false;
    }
    
    if (response) {
        *response = response_data;
    }
    
    return true;
}

std::string OneDriveClient::url_encode(const std::string& str) {
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

std::string OneDriveClient::get_relative_path(const std::string& path) const {
    // Remove leading slash if present
    std::string result = path;
    if (!result.empty() && result.front() == '/') {
        result = result.substr(1);
    }
    return result;
}

std::string OneDriveClient::get_parent_path(const std::string& path) const {
    std::string relative = get_relative_path(path);
    size_t pos = relative.find_last_of('/');
    if (pos == std::string::npos) {
        return "/";
    }
    return relative.substr(0, pos);
}

std::string OneDriveClient::get_file_name(const std::string& path) const {
    std::string relative = get_relative_path(path);
    size_t pos = relative.find_last_of('/');
    if (pos == std::string::npos) {
        return relative;
    }
    return relative.substr(pos + 1);
}

Json::Value OneDriveClient::parse_response(const std::string& response) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream stream(response);
    
    if (!Json::parseFromStream(reader, stream, &root, &errs)) {
        std::cerr << "Failed to parse JSON response: " << errs << std::endl;
        return Json::Value();
    }
    
    return root;
}

bool OneDriveClient::is_error_response(const Json::Value& response) {
    return response.isObject() && response["error"].isObject();
}

std::string OneDriveClient::get_error_message(const Json::Value& response) {
    if (response.isObject() && response["error"].isObject()) {
        if (response["error"]["message"].isString()) {
            return response["error"]["message"].asString();
        }
    }
    return "Unknown error";
}

int OneDriveClient::upload_files(const std::vector<std::string>& local_paths,
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

size_t OneDriveClient::write_callback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    if (data) {
        data->append((char*)contents, total_size);
    }
    return total_size;
}

size_t OneDriveClient::read_callback(void* ptr, size_t size, size_t nmemb, FILE* file) {
    return fread(ptr, size, nmemb, file);
}

int OneDriveClient::progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                     curl_off_t ultotal, curl_off_t ulnow) {
    if (ultotal > 0) {
        int percent = (ulnow * 100) / ultotal;
        std::cout << "\rUpload progress: " << percent << "%" << std::flush;
    }
    return 0;
}

std::string OneDriveClient::get_drive_id() const {
    return drive_id;
}

std::string OneDriveClient::get_user_info() const {
    return user_id;
}
