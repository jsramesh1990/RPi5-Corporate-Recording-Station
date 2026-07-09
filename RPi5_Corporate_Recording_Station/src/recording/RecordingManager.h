#ifndef RECORDING_MANAGER_HPP
#define RECORDING_MANAGER_HPP

#include "VideoRecorder.hpp"
#include "AudioRecorder.hpp"
#include <memory>
#include <functional>
#include <mutex>

/**
 * @brief Recording Manager - Orchestrates Video and Audio Recording
 * 
 * Manages synchronized video and audio recording with
 * proper timing and synchronization.
 */
class RecordingManager {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class SyncMode {
        INDEPENDENT,    // Video and audio record separately
        SYNC_START,     // Synchronized start only
        SYNC_FULL       // Fully synchronized with timestamp matching
    };
    
    enum class RecorderState {
        IDLE,
        RECORDING,
        PAUSED,
        ERROR
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    struct RecordingInfo {
        std::string video_file;
        std::string audio_file;
        std::string combined_file;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point stop_time;
        double duration_seconds = 0;
        bool has_video = false;
        bool has_audio = false;
        bool is_complete = false;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using StateCallback = std::function<void(RecorderState)>;
    using ProgressCallback = std::function<void(float)>;
    using CompleteCallback = std::function<void(const RecordingInfo&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    RecordingManager();
    ~RecordingManager();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    bool initialize(const VideoRecorder::VideoConfig& video_config,
                    const AudioRecorder::AudioConfig& audio_config);
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // RECORDING CONTROL
    // ============================================
    
    bool startRecording(const std::string& base_filename = "");
    bool stopRecording();
    void pauseRecording();
    void resumeRecording();
    bool isRecording() const { return state == RecorderState::RECORDING; }
    bool isPaused() const { return state == RecorderState::PAUSED; }
    RecorderState getState() const { return state; }
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    void setSyncMode(SyncMode mode) { sync_mode = mode; }
    void setOutputDirectory(const std::string& dir) { output_dir = dir; }
    void setAutoCombine(bool enable) { auto_combine = enable; }
    void setMaxRecordingTime(int seconds) { max_recording_time = seconds; }
    
    // ============================================
    // STATISTICS
    // ============================================
    
    RecordingInfo getCurrentRecording() const;
    VideoRecorder::RecordingStats getVideoStats() const;
    AudioRecorder::RecordingStats getAudioStats() const;
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnStateChange(StateCallback callback);
    void setOnProgress(ProgressCallback callback);
    void setOnComplete(CompleteCallback callback);
    void setOnError(ErrorCallback callback);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    bool initialized = false;
    RecorderState state = RecorderState::IDLE;
    SyncMode sync_mode = SyncMode::SYNC_FULL;
    std::string output_dir = "/var/lib/recording-station/recordings";
    bool auto_combine = true;
    int max_recording_time = 0;  // 0 = unlimited
    
    std::unique_ptr<VideoRecorder> video_recorder;
    std::unique_ptr<AudioRecorder> audio_recorder;
    
    RecordingInfo current_recording;
    mutable std::mutex mutex;
    
    // Callbacks
    StateCallback on_state_change;
    ProgressCallback on_progress;
    CompleteCallback on_complete;
    ErrorCallback on_error;
    
    // Internal methods
    void setState(RecorderState new_state);
    void notifyStateChange();
    void notifyProgress(float progress);
    void notifyComplete(const RecordingInfo& info);
    void notifyError(const std::string& error);
    std::string generateCombinedFilename(const std::string& base);
    bool combineFiles(const std::string& video_file, const std::string& audio_file,
                     const std::string& output_file);
    void updateCombinedProgress(float video_progress, float audio_progress);
};

#endif // RECORDING_MANAGER_HPP
