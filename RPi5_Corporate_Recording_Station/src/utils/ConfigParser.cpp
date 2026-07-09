#include "ConfigParser.hpp"

#include <iostream>
#include <filesystem>
#include <cctype>

const std::vector<std::string> ConfigParser::truth_values = {"true", "yes", "1", "on", "enabled"};
const std::vector<std::string> ConfigParser::false_values = {"false", "no", "0", "off", "disabled"};

// ============================================
// VALUE IMPLEMENTATION
// ============================================

std::string ConfigParser::Value::as_string() const {
    if (is_env) {
        const char* env_value = std::getenv(raw.c_str());
        if (env_value) {
            return std::string(env_value);
        }
        return raw;
    }
    return raw;
}

int ConfigParser::Value::as_int(int default_value) const {
    try {
        return std::stoi(as_string());
    } catch (...) {
        return default_value;
    }
}

float ConfigParser::Value::as_float(float default_value) const {
    try {
        return std::stof(as_string());
    } catch (...) {
        return default_value;
    }
}

bool ConfigParser::Value::as_bool(bool default_value) const {
    std::string str = as_string();
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    
    for (const auto& truth : truth_values) {
        if (str == truth) return true;
    }
    for (const auto& fals : false_values) {
        if (str == fals) return false;
    }
    return default_value;
}

std::vector<std::string> ConfigParser::Value::as_list(char delimiter) const {
    std::vector<std::string> result;
    std::string str = as_string();
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        std::string trimmed = trim(item);
        if (!trimmed.empty()) {
            result.push_back(trimmed);
        }
    }
    return result;
}

std::map<std::string, std::string> ConfigParser::Value::as_map(char delimiter) const {
    std::map<std::string, std::string> result;
    auto items = as_list(delimiter);
    for (const auto& item : items) {
        size_t eq_pos = item.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(item.substr(0, eq_pos));
            std::string value = trim(item.substr(eq_pos + 1));
            if (!key.empty()) {
                result[key] = value;
            }
        }
    }
    return result;
}

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

ConfigParser::ConfigParser() {
    // Default include paths
    include_paths.push_back("/etc/recording-station/");
    include_paths.push_back("./config/");
}

ConfigParser::~ConfigParser() {
    clear();
}

// ============================================
// PARSING
// ============================================

bool ConfigParser::loadFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        last_error = "Failed to open file: " + filename;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return loadString(buffer.str());
}

bool ConfigParser::loadString(const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex);
    
    clear();
    
    std::stringstream ss(content);
    std::string line;
    std::string current_section = "";
    std::vector<std::string> comments;
    
    while (std::getline(ss, line)) {
        // Trim line
        line = trim(line);
        
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        
        // Handle includes
        if (line.find("@include") == 0 || line.find("include") == 0) {
            std::string include_file = trim(line.substr(line.find(' ') + 1));
            if (!include_file.empty()) {
                processInclude(include_file, "");
            }
            continue;
        }
        
        // Handle comments
        if (isComment(line)) {
            comments.push_back(line);
            continue;
        }
        
        // Handle section
        if (isSection(line)) {
            current_section = getSectionName(line);
            if (!current_section.empty()) {
                config[current_section] = Section{};
                config[current_section].name = current_section;
                config[current_section].comments = comments;
                comments.clear();
            }
            continue;
        }
        
        // Handle key-value
        if (isKeyValue(line)) {
            std::string key, value;
            if (parseKeyValue(line, key, value)) {
                if (!current_section.empty()) {
                    Value val;
                    val.raw = value;
                    // Check for environment variable
                    if (value.size() > 2 && value.front() == '$' && value.back() == '$') {
                        val.is_env = true;
                        val.raw = value.substr(1, value.size() - 2);
                    }
                    // Check for quotes
                    if (value.size() > 1 && 
                        ((value.front() == '"' && value.back() == '"') ||
                         (value.front() == '\'' && value.back() == '\''))) {
                        val.is_quoted = true;
                        val.raw = value.substr(1, value.size() - 2);
                    }
                    config[current_section].values[key] = val;
                    
                    // Add comments to last value
                    if (!comments.empty()) {
                        // Comments are stored separately
                        comments.clear();
                    }
                }
            }
            continue;
        }
    }
    
    return true;
}

void ConfigParser::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    config.clear();
    last_error.clear();
}

// ============================================
// SAVE
// ============================================

bool ConfigParser::saveFile(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << saveString();
    file.close();
    return true;
}

std::string ConfigParser::saveString() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::stringstream ss;
    
    for (const auto& [section_name, section] : config) {
        // Write section comments
        for (const auto& comment : section.comments) {
            ss << comment << "\n";
        }
        
        // Write section header
        ss << "[" << section_name << "]\n";
        
        // Write values
        for (const auto& [key, value] : section.values) {
            std::string val = value.raw;
            if (value.is_env) {
                val = "$" + val + "$";
            } else if (value.is_quoted || val.find(' ') != std::string::npos) {
                val = "\"" + val + "\"";
            }
            ss << key << "=" << val << "\n";
        }
        ss << "\n";
    }
    
    return ss.str();
}

// ============================================
// GET
// ============================================

ConfigParser::Value ConfigParser::get(const std::string& section, const std::string& key,
                                     const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto section_it = config.find(section);
    if (section_it != config.end()) {
        auto key_it = section_it->second.values.find(key);
        if (key_it != section_it->second.values.end()) {
            return key_it->second;
        }
    }
    
    Value val;
    val.raw = default_value;
    return val;
}

std::string ConfigParser::getString(const std::string& section, const std::string& key,
                                   const std::string& default_value) const {
    return get(section, key, default_value).as_string();
}

int ConfigParser::getInt(const std::string& section, const std::string& key,
                        int default_value) const {
    return get(section, key, std::to_string(default_value)).as_int(default_value);
}

float ConfigParser::getFloat(const std::string& section, const std::string& key,
                            float default_value) const {
    return get(section, key, std::to_string(default_value)).as_float(default_value);
}

bool ConfigParser::getBool(const std::string& section, const std::string& key,
                          bool default_value) const {
    return get(section, key, default_value ? "true" : "false").as_bool(default_value);
}

std::vector<std::string> ConfigParser::getList(const std::string& section,
                                              const std::string& key,
                                              char delimiter) const {
    return get(section, key, "").as_list(delimiter);
}

std::map<std::string, std::string> ConfigParser::getMap(const std::string& section,
                                                       const std::string& key,
                                                       char delimiter) const {
    return get(section, key, "").as_map(delimiter);
}

std::vector<std::string> ConfigParser::getSections() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> sections;
    for (const auto& [name, _] : config) {
        sections.push_back(name);
    }
    return sections;
}

std::vector<std::string> ConfigParser::getKeys(const std::string& section) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> keys;
    auto it = config.find(section);
    if (it != config.end()) {
        for (const auto& [key, _] : it->second.values) {
            keys.push_back(key);
        }
    }
    return keys;
}

bool ConfigParser::hasSection(const std::string& section) const {
    std::lock_guard<std::mutex> lock(mutex);
    return config.find(section) != config.end();
}

bool ConfigParser::hasKey(const std::string& section, const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = config.find(section);
    if (it != config.end()) {
        return it->second.values.find(key) != it->second.values.end();
    }
    return false;
}

// ============================================
// SET
// ============================================

void ConfigParser::set(const std::string& section, const std::string& key,
                      const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (config.find(section) == config.end()) {
        config[section] = Section{};
        config[section].name = section;
    }
    
    Value val;
    val.raw = value;
    config[section].values[key] = val;
}

void ConfigParser::setInt(const std::string& section, const std::string& key, int value) {
    set(section, key, std::to_string(value));
}

void ConfigParser::setFloat(const std::string& section, const std::string& key, float value) {
    set(section, key, std::to_string(value));
}

void ConfigParser::setBool(const std::string& section, const std::string& key, bool value) {
    set(section, key, value ? "true" : "false");
}

void ConfigParser::setList(const std::string& section, const std::string& key,
                          const std::vector<std::string>& values, char delimiter) {
    std::string result;
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) result += delimiter;
        result += values[i];
    }
    set(section, key, result);
}

void ConfigParser::removeKey(const std::string& section, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = config.find(section);
    if (it != config.end()) {
        it->second.values.erase(key);
    }
}

void ConfigParser::removeSection(const std::string& section) {
    std::lock_guard<std::mutex> lock(mutex);
    config.erase(section);
}

// ============================================
// COMMENTS
// ============================================

void ConfigParser::addSectionComment(const std::string& section, const std::string& comment) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = config.find(section);
    if (it != config.end()) {
        it->second.comments.push_back(comment);
    }
}

void ConfigParser::addKeyComment(const std::string& section, const std::string& key,
                                const std::string& comment) {
    // Comments for keys are not directly stored in this implementation
    // They would be added during saving
}

// ============================================
// UTILITY
// ============================================

void ConfigParser::merge(const ConfigParser& other, bool override) {
    std::lock_guard<std::mutex> lock(mutex);
    
    for (const auto& [section_name, section] : other.config) {
        if (config.find(section_name) == config.end()) {
            config[section_name] = section;
        } else {
            for (const auto& [key, value] : section.values) {
                if (override || config[section_name].values.find(key) == 
                    config[section_name].values.end()) {
                    config[section_name].values[key] = value;
                }
            }
        }
    }
}

std::map<std::string, std::map<std::string, std::string>> ConfigParser::asMap() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::map<std::string, std::map<std::string, std::string>> result;
    
    for (const auto& [section_name, section] : config) {
        for (const auto& [key, value] : section.values) {
            result[section_name][key] = value.as_string();
        }
    }
    
    return result;
}

void ConfigParser::dump() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::cout << "=== Configuration ===" << std::endl;
    for (const auto& [section_name, section] : config) {
        std::cout << "[" << section_name << "]" << std::endl;
        for (const auto& [key, value] : section.values) {
            std::cout << key << "=" << value.as_string() << std::endl;
        }
        std::cout << std::endl;
    }
    std::cout << "=====================" << std::endl;
}

// ============================================
// PRIVATE METHODS
// ============================================

std::string ConfigParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

std::string ConfigParser::resolveEnv(const std::string& str) {
    if (str.empty()) return str;
    
    std::string result = str;
    std::regex env_regex(R"(\$\{([^}]+)\})");
    std::smatch match;
    
    while (std::regex_search(result, match, env_regex)) {
        const char* env_value = std::getenv(match[1].str().c_str());
        if (env_value) {
            result = match.prefix().str() + std::string(env_value) + match.suffix().str();
        } else {
            result = match.prefix().str() + match.suffix().str();
        }
    }
    
    return result;
}

bool ConfigParser::isComment(const std::string& line) {
    if (line.empty()) return false;
    char first = line[0];
    return first == '#' || first == ';' || (line.size() >= 2 && line[0] == '/' && line[1] == '/');
}

bool ConfigParser::isSection(const std::string& line) {
    if (line.empty()) return false;
    return line.front() == '[' && line.back() == ']';
}

bool ConfigParser::isKeyValue(const std::string& line) {
    return line.find('=') != std::string::npos;
}

std::string ConfigParser::getSectionName(const std::string& line) {
    if (line.size() < 3) return "";
    return line.substr(1, line.size() - 2);
}

void ConfigParser::processInclude(const std::string& line, const std::string& base_path) {
    std::string path = line;
    if (path.front() != '/') {
        // Relative path
        for (const auto& include_path : include_paths) {
            std::string full_path = include_path + path;
            if (std::filesystem::exists(full_path)) {
                loadFile(full_path);
                break;
            }
        }
    } else {
        loadFile(path);
    }
}

bool ConfigParser::parseKeyValue(const std::string& line, std::string& key, std::string& value) {
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
        return false;
    }
    
    key = trim(line.substr(0, eq_pos));
    value = trim(line.substr(eq_pos + 1));
    return !key.empty();
}
