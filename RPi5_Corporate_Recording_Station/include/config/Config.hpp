#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>

/**
 * @brief Configuration Manager for Recording Station
 * 
 * Handles loading, parsing, and accessing configuration settings
 * from various sources (files, environment variables, defaults).
 */
class Config {
public:
    /**
     * @brief Get singleton instance
     */
    static Config& getInstance() {
        static Config instance;
        return instance;
    }
    
    /**
     * @brief Load configuration from file
     * @param filename Path to configuration file
     * @return true if successful
     */
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open config file: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            
            // Parse key=value
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) {
                continue;
            }
            
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // Remove quotes if present
            if (value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            config_map[key] = value;
        }
        
        return true;
    }
    
    /**
     * @brief Load configuration from environment variables
     */
    void loadFromEnvironment() {
        // Check for common environment variables
        const char* env_vars[] = {
            "DATA_DIR", "LOG_LEVEL", "CLOUD_PROVIDER",
            "NEXTCLOUD_URL", "NEXTCLOUD_USERNAME", "NEXTCLOUD_PASSWORD",
            "VIDEO_WIDTH", "VIDEO_HEIGHT", "VIDEO_FRAMERATE",
            "AUDIO_SAMPLE_RATE", "AUDIO_CHANNELS"
        };
        
        for (const char* var : env_vars) {
            const char* value = std::getenv(var);
            if (value != nullptr) {
                config_map[var] = std::string(value);
            }
        }
    }
    
    /**
     * @brief Get string value
     */
    std::string getString(const std::string& key, const std::string& default_value = "") const {
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            return it->second;
        }
        return default_value;
    }
    
    /**
     * @brief Get integer value
     */
    int getInt(const std::string& key, int default_value = 0) const {
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return default_value;
            }
        }
        return default_value;
    }
    
    /**
     * @brief Get float value
     */
    float getFloat(const std::string& key, float default_value = 0.0f) const {
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            try {
                return std::stof(it->second);
            } catch (...) {
                return default_value;
            }
        }
        return default_value;
    }
    
    /**
     * @brief Get boolean value
     */
    bool getBool(const std::string& key, bool default_value = false) const {
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            std::string val = it->second;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            return (val == "true" || val == "1" || val == "yes" || val == "on");
        }
        return default_value;
    }
    
    /**
     * @brief Get vector of strings from comma-separated list
     */
    std::vector<std::string> getStringList(const std::string& key) const {
        std::vector<std::string> result;
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            std::stringstream ss(it->second);
            std::string item;
            while (std::getline(ss, item, ',')) {
                // Trim whitespace
                item.erase(0, item.find_first_not_of(" \t"));
                item.erase(item.find_last_not_of(" \t") + 1);
                if (!item.empty()) {
                    result.push_back(item);
                }
            }
        }
        return result;
    }
    
    /**
     * @brief Set configuration value
     */
    void setValue(const std::string& key, const std::string& value) {
        config_map[key] = value;
    }
    
    /**
     * @brief Save configuration to file
     */
    bool save(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        for (const auto& pair : config_map) {
            file << pair.first << "=" << pair.second << "\n";
        }
        
        return true;
    }
    
    /**
     * @brief Print all configuration values
     */
    void dump() const {
        std::cout << "=== Configuration ===" << std::endl;
        for (const auto& pair : config_map) {
            // Don't print passwords
            if (pair.first.find("PASSWORD") != std::string::npos ||
                pair.first.find("SECRET") != std::string::npos ||
                pair.first.find("KEY") != std::string::npos) {
                std::cout << pair.first << "=***" << std::endl;
            } else {
                std::cout << pair.first << "=" << pair.second << std::endl;
            }
        }
        std::cout << "=====================" << std::endl;
    }
    
    /**
     * @brief Get all keys
     */
    std::vector<std::string> getKeys() const {
        std::vector<std::string> keys;
        for (const auto& pair : config_map) {
            keys.push_back(pair.first);
        }
        return keys;
    }
    
private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    std::map<std::string, std::string> config_map;
};

#endif // CONFIG_HPP
