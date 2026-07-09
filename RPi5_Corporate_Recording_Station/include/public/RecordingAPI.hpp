#ifndef RECORDING_API_HPP
#define RECORDING_API_HPP

#include <string>
#include <vector>
#include <chrono>
#include <functional>

/**
 * @brief Public API for recording operations
 * 
 * This interface provides a stable API for controlling
 * video and audio recording functionality.
 */
class RecordingAPI {
public:
    virtual ~RecordingAPI() = default;
    
    /**
     * @brief Recording status structure
     */
    struct RecordingStatus {
        bool is_recording = false;
        std::string filename;
        std::chrono::system_clock::time_point start_time;
        int64_t duration_seconds = 0;
        int64_t file_size_bytes = 0;
        int video_width = 0;
        int video_height = 0;
        int video_framerate = 0;
        int audio_sample_rate = 0;
        int audio_channels = 0;
    };
    
    /**
     * @brief Recording configuration
     */
    struct RecordingConfig {
        // Video settings
        int width = 1920;
        int height = 1080;
        int framerate = 30;
        int bitrate = 4000000;  // 4 Mbps
        std::string codec = "h264";
        std::string format = "mp4";
        
        // Audio settings
        int sample_rate = 48000;
        int channels = 2;
        int audio_bitrate = 128000;  // 128 Kbps
        std::string audio_codec = "aac";
        
        // Recording limits
        int max_duration = 3600;  // seconds
        int64_t max_file_size = 4LL * 1024 * 1024 * 1024;  // 4 GB
        std::string output_directory = "/var/lib/recording-station/recordings";
    };
    
    /**
     * @brief Start recording
     * @param config Recording configuration
     * @return true if recording started successfully
     */
    virtual bool startRecording(const RecordingConfig& config = RecordingConfig()) = 0;
    
    /**
     * @brief Stop recording
     * @return Filename of the recorded file
     */
    virtual std::string stopRecording() = 0;
    
    /**
     * @brief Pause recording
     */
    virtual void pauseRecording() = 0;
    
    /**
     * @brief Resume recording
     */
    virtual void resumeRecording() = 0;
    
    /**
     * @brief Get current recording status
     */
    virtual RecordingStatus getStatus() const = 0;
    
    /**
     * @brief Get list of recordings
     * @param limit Maximum number of recordings to return
     * @param offset Pagination offset
     * @return Vector of recording filenames
     */
    virtual std::vector<std::string> listRecordings(int limit = 100, int offset = 0) const = 0;
    
    /**
     * @brief Delete a recording
     * @param filename Filename to delete
     * @return true if successful
     */
    virtual bool deleteRecording(const std::string& filename) = 0;
    
    /**
     * @brief Set onRecordingStarted callback
     */
    virtual void setOnRecordingStarted(std::function<void()> callback) = 0;
    
    /**
     * @brief Set onRecordingStopped callback
     */
    virtual void setOnRecordingStopped(std::function<void(const std::string&)> callback) = 0;
    
    /**
     * @brief Set onRecordingProgress callback
     */
    virtual void setOnRecordingProgress(std::function<void(int64_t, int64_t)> callback) = 0;
    
    /**
     * @brief Get recording configuration
     */
    virtual RecordingConfig getConfig() const = 0;
    
    /**
     * @brief Set recording configuration
     */
    virtual void setConfig(const RecordingConfig& config) = 0;
};

#endif // RECORDING_API_HPP
