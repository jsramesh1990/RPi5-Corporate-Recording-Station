#ifndef FILE_METADATA_HPP
#define FILE_METADATA_HPP

#include <string>
#include <map>
#include <chrono>
#include <cstdint>

/**
 * @brief Time credentials for file tracking
 * 
 * This structure holds all time-related metadata for files,
 * crucial for corporate record keeping and auditing.
 */
struct TimeCredentials {
    std::chrono::system_clock::time_point creation_time;
    std::chrono::system_clock::time_point modification_time;
    std::chrono::system_clock::time_point access_time;
    std::chrono::system_clock::time_point status_change_time;
    
    // Human-readable formats
    std::string creation_formatted;
    std::string modification_formatted;
    std::string access_formatted;
    std::string status_change_formatted;
    
    // Epoch timestamps (Unix)
    int64_t creation_epoch = 0;
    int64_t modification_epoch = 0;
    int64_t access_epoch = 0;
    int64_t status_change_epoch = 0;
    
    // Timezone information
    std::string timezone;
    int timezone_offset = 0; // in seconds
    
    // Age calculations
    int64_t age_seconds = 0;
    int64_t age_days = 0;
    int64_t age_months = 0;
    
    TimeCredentials() = default;
};

/**
 * @brief File metadata with comprehensive tracking
 */
struct FileMetadata {
    std::string filename;
    std::string path;
    std::string file_type;
    uint64_t size_bytes = 0;
    TimeCredentials time_creds;
    std::string checksum_sha256;
    std::string checksum_md5;
    std::map<std::string, std::string> custom_metadata;
    bool is_compressed = false;
    bool is_encrypted = false;
    std::string owner;
    std::string group;
    int permissions = 0;
    
    // Additional metadata for recording files
    struct RecordingMetadata {
        int video_width = 0;
        int video_height = 0;
        int video_framerate = 0;
        std::string video_codec;
        int audio_sample_rate = 0;
        int audio_channels = 0;
        std::string audio_codec;
        int duration_seconds = 0;
        std::string camera_model;
        std::string microphone_model;
    } recording_metadata;
    
    FileMetadata() = default;
};

#endif // FILE_METADATA_HPP
