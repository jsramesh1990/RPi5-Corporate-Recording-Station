#ifndef STORAGE_API_HPP
#define STORAGE_API_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>

/**
 * @brief Public API for storage operations
 * 
 * This interface provides a stable API for managing
 * storage, files, and metadata.
 */
class StorageAPI {
public:
    virtual ~StorageAPI() = default;
    
    /**
     * @brief File information structure
     */
    struct FileInfo {
        std::string filename;
        std::string path;
        int64_t size_bytes = 0;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point modified;
        std::chrono::system_clock::time_point accessed;
        std::string file_type;
        std::string owner;
        std::string group;
        int permissions = 0;
        std::map<std::string, std::string> metadata;
    };
    
    /**
     * @brief Storage statistics structure
     */
    struct StorageStats {
        int64_t total_bytes = 0;
        int64_t used_bytes = 0;
        int64_t free_bytes = 0;
        int64_t available_bytes = 0;
        size_t file_count = 0;
        size_t directory_count = 0;
        size_t total_inodes = 0;
        size_t used_inodes = 0;
        float fragmentation_percent = 0.0f;
        std::chrono::system_clock::time_point last_scan;
        std::map<std::string, size_t> file_type_distribution;
        std::map<std::string, size_t> file_age_distribution;
    };
    
    /**
     * @brief Get storage statistics
     */
    virtual StorageStats getStats() const = 0;
    
    /**
     * @brief Get list of files in directory
     * @param path Directory path (relative or absolute)
     * @param recursive Include subdirectories
     * @return Vector of file information
     */
    virtual std::vector<FileInfo> listFiles(const std::string& path = "",
                                            bool recursive = false) const = 0;
    
    /**
     * @brief Get file information
     * @param path Path to file
     * @return File information
     */
    virtual FileInfo getFileInfo(const std::string& path) const = 0;
    
    /**
     * @brief Delete a file
     * @param path Path to file
     * @return true if successful
     */
    virtual bool deleteFile(const std::string& path) = 0;
    
    /**
     * @brief Move a file
     * @param source Source path
     * @param destination Destination path
     * @return true if successful
     */
    virtual bool moveFile(const std::string& source, const std::string& destination) = 0;
    
    /**
     * @brief Copy a file
     * @param source Source path
     * @param destination Destination path
     * @return true if successful
     */
    virtual bool copyFile(const std::string& source, const std::string& destination) = 0;
    
    /**
     * @brief Create a directory
     * @param path Directory path
     * @param recursive Create parent directories
     * @return true if successful
     */
    virtual bool createDirectory(const std::string& path, bool recursive = false) = 0;
    
    /**
     * @brief Delete a directory
     * @param path Directory path
     * @param recursive Delete contents
     * @return true if successful
     */
    virtual bool deleteDirectory(const std::string& path, bool recursive = false) = 0;
    
    /**
     * @brief Search for files
     * @param pattern Filename pattern (wildcards)
     * @param path Directory to search
     * @param recursive Include subdirectories
     * @return Vector of file paths
     */
    virtual std::vector<std::string> searchFiles(const std::string& pattern,
                                                 const std::string& path = "",
                                                 bool recursive = true) const = 0;
    
    /**
     * @brief Get files by type
     * @param file_type File extension or type
     * @param path Directory to search
     * @return Vector of file information
     */
    virtual std::vector<FileInfo> getFilesByType(const std::string& file_type,
                                                 const std::string& path = "") const = 0;
    
    /**
     * @brief Get files by date range
     * @param start Start date
     * @param end End date
     * @param path Directory to search
     * @return Vector of file information
     */
    virtual std::vector<FileInfo> getFilesByDate(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        const std::string& path = "") const = 0;
    
    /**
     * @brief Get largest files
     * @param count Number of files to return
     * @param path Directory to search
     * @return Vector of file information
     */
    virtual std::vector<FileInfo> getLargestFiles(size_t count = 10,
                                                  const std::string& path = "") const = 0;
    
    /**
     * @brief Get oldest files
     * @param count Number of files to return
     * @param path Directory to search
     * @return Vector of file information
     */
    virtual std::vector<FileInfo> getOldestFiles(size_t count = 10,
                                                 const std::string& path = "") const = 0;
    
    /**
     * @brief Get file type distribution
     * @param path Directory to analyze
     * @return Map of file types to counts
     */
    virtual std::map<std::string, size_t> getFileTypeDistribution(const std::string& path = "") const = 0;
    
    /**
     * @brief Get file age distribution
     * @param path Directory to analyze
     * @return Map of age ranges to counts
     */
    virtual std::map<std::string, size_t> getFileAgeDistribution(const std::string& path = "") const = 0;
    
    /**
     * @brief Calculate total size of files by type
     * @param file_type File extension or type
     * @param path Directory to search
     * @return Total size in bytes
     */
    virtual int64_t getTotalSizeByType(const std::string& file_type,
                                       const std::string& path = "") const = 0;
};

#endif // STORAGE_API_HPP
