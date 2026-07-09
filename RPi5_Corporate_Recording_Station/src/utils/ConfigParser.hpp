#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <algorithm>

/**
 * @brief Configuration Parser for Recording Station
 * 
 * Provides configuration file parsing with:
 * - INI file format support
 * - Environment variable substitution
 * - Section-based organization
 * - Comments support
 * - Type conversion
 * - Include support
 */
class ConfigParser {
public:
    // ============================================
    // STRUCTURES
    // ============================================
    
    struct Value {
        std::string raw;
        bool is_env = false;
        bool is_quoted = false;
        
        std::string as_string() const;
        int as_int(int default_value = 0) const;
        float as_float(float default_value = 0.0f) const;
        bool as_bool(bool default_value = false) const;
        std::vector<std::string> as_list(char delimiter = ',') const;
        std::map<std::string, std::string> as_map(char delimiter = ',') const;
    };
    
    struct Section {
        std::string name;
        std::map<std::string, Value> values;
        std::vector<std::string> comments;
    };
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    ConfigParser();
    ~ConfigParser();
    
    // ============================================
    // PARSING
    // ============================================
    
    /**
     * @brief Load configuration from file
     * @param filename Path to configuration file
     * @return true if successful
     */
    bool loadFile(const std::string& filename);
    
    /**
     * @brief Load configuration from string
     * @param content Configuration content
     * @return true if successful
     */
    bool loadString(const std::string& content);
    
    /**
     * @brief Clear configuration
     */
    void clear();
    
    // ============================================
    // SAVE
    // ============================================
    
    /**
     * @brief Save configuration to file
     * @param filename Path to configuration file
     * @return true if successful
     */
    bool saveFile(const std::string& filename) const;
    
    /**
     * @brief Save configuration to string
     * @return Configuration content
     */
    std::string saveString() const;
    
    // ============================================
    // GET
    // ============================================
    
    /**
     * @brief Get value from section
     * @param section Section name
     * @param key Key name
     * @param default_value Default value
     * @return Value
     */
    Value get(const std::string& section, const std::string& key,
              const std::string& default_value = "") const;
    
    /**
     * @brief Get string value
     */
    std::string getString(const std::string& section, const std::string& key,
                         const std::string& default_value = "") const;
    
    /**
     * @brief Get integer value
     */
    int getInt(const std::string& section, const std::string& key,
               int default_value = 0) const;
    
    /**
     * @brief Get float value
     */
    float getFloat(const std::string& section, const std::string& key,
                   float default_value = 0.0f) const;
    
    /**
     * @brief Get boolean value
     */
    bool getBool(const std::string& section, const std::string& key,
                 bool default_value = false) const;
    
    /**
     * @brief Get list value
     */
    std::vector<std::string> getList(const std::string& section,
                                    const std::string& key,
                                    char delimiter = ',') const;
    
    /**
     * @brief Get map value
     */
    std::map<std::string, std::string> getMap(const std::string& section,
                                              const std::string& key,
                                              char delimiter = ',') const;
    
    /**
     * @brief Get all sections
     */
    std::vector<std::string> getSections() const;
    
    /**
     * @brief Get all keys in section
     */
    std::vector<std::string> getKeys(const std::string& section) const;
    
    /**
     * @brief Check if section exists
     */
    bool hasSection(const std::string& section) const;
    
    /**
     * @brief Check if key exists
     */
    bool hasKey(const std::string& section, const std::string& key) const;
    
    // ============================================
    // SET
    // ============================================
    
    /**
     * @brief Set value
     * @param section Section name
     * @param key Key name
     * @param value Value
     */
    void set(const std::string& section, const std::string& key,
             const std::string& value);
    
    /**
     * @brief Set value with type
     */
    void setInt(const std::string& section, const std::string& key, int value);
    void setFloat(const std::string& section, const std::string& key, float value);
    void setBool(const std::string& section, const std::string& key, bool value);
    void setList(const std::string& section, const std::string& key,
                 const std::vector<std::string>& values, char delimiter = ',');
    
    /**
     * @brief Remove key
     */
    void removeKey(const std::string& section, const std::string& key);
    
    /**
     * @brief Remove section
     */
    void removeSection(const std::string& section);
    
    // ============================================
    // COMMENTS
    // ============================================
    
    /**
     * @brief Add comment to section
     */
    void addSectionComment(const std::string& section, const std::string& comment);
    
    /**
     * @brief Add comment to key
     */
    void addKeyComment(const std::string& section, const std::string& key,
                       const std::string& comment);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Merge configuration
     */
    void merge(const ConfigParser& other, bool override = true);
    
    /**
     * @brief Get configuration as map
     */
    std::map<std::string, std::map<std::string, std::string>> asMap() const;
    
    /**
     * @brief Dump configuration
     */
    void dump() const;
    
    /**
     * @brief Get last error
     */
    std::string getLastError() const { return last_error; }
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    std::map<std::string, Section> config;
    mutable std::mutex mutex;
    std::string last_error;
    std::vector<std::string> include_paths;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    bool parseLine(const std::string& line, std::string& section,
                   std::string& key, std::string& value);
    bool parseSection(const std::string& line, std::string& section);
    bool parseKeyValue(const std::string& line, std::string& key,
                       std::string& value);
    std::string trim(const std::string& str);
    std::string resolveEnv(const std::string& str);
    std::string escapeQuotes(const std::string& str);
    std::string unescapeQuotes(const std::string& str);
    bool isComment(const std::string& line);
    bool isSection(const std::string& line);
    bool isKeyValue(const std::string& line);
    std::string getSectionName(const std::string& line);
    void processInclude(const std::string& line, const std::string& base_path);
    
    // Static helpers
    static const std::vector<std::string> truth_values;
    static const std::vector<std::string> false_values;
};

#endif // CONFIG_PARSER_HPP
