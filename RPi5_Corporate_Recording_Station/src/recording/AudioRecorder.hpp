#ifndef AUDIO_RECORDER_HPP
#define AUDIO_RECORDER_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <cstdint>
#include <chrono>

/**
 * @brief Audio Recorder for Raspberry Pi 5
 * 
 * Provides audio recording functionality with support for
 * various codecs, sample rates, and container formats.
 * Optimized for ARM Cortex-A76.
 */
class AudioRecorder {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Codec {
        AAC,
        MP3,
        OPUS,
        FLAC,
        WAV,
        PCM
    };
    
    enum class Container {
        MP4,
        MKV,
        MP3,
        FLAC,
        WAV,
        OGG
    };
    
    enum class Quality {
        LOW,
        MEDIUM,
        HIGH,
        ULTRA
    };
    
    enum class RecorderState {
        IDLE,
        INITIALIZING,
        RECORDING,
        PAUSED,
        STOPPING,
        ERROR
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief Audio configuration
     */
    struct AudioConfig {
        int sample_rate = 48000;
        int channels = 2;
        int bitrate = 128000;          // 128 Kbps
        int bits_per_sample = 16;
        Codec codec = Codec::AAC;
        Container container = Container::MP4;
        Quality quality = Quality::HIGH;
        int buffer_size = 1024;
        int period_size = 256;
        std::string device = "default";
        bool hardware_acceleration = true;
        std::string profile = "aac_low";
        std::map<std::string, std::string> extra_options;
    };
    
    /**
     * @brief Recording statistics
     */
    struct RecordingStats {
        uint64_t samples_recorded = 0;
        uint64_t bytes_written = 0;
        uint64_t underruns = 0;
        uint64_t overruns = 0;
        double duration_seconds = 0;
        double cpu_usage = 0;
        double memory_usage = 0;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_sample_time;
        std::string current_file;
        bool is_recording = false;
    };
    
    /**
     * @brief Audio data
     */
    struct AudioData {
        std::vector<int16_t> samples;
        int sample_rate = 48000;
        int channels = 2;
        int bits_per_sample = 16;
        int64_t pts = 0;
        std::map<std::string, std::string> metadata;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using AudioCallback = std::function<void(const AudioData&)>;
    using StatsCallback = std::function<void(const RecordingStats&)>;
    using StateCallback = std::function<void(RecorderState)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(float)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    AudioRecorder();
    ~AudioRecorder();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize audio recorder
     * @param config Audio configuration
     * @return true if successful
     */
    bool initialize(const AudioConfig& config = AudioConfig());
    
    /**
     * @brief Initialize with parameters
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @param bitrate Bitrate in bps
     * @param codec Audio codec
     * @return true if successful
     */
    bool initialize(int sample_rate, int channels = 2,
                   int bitrate = 128000, Codec codec = Codec::AAC);
    
    /**
     * @brief Shutdown audio recorder
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // RECORDING CONTROL
    // ============================================
    
    /**
     * @brief Start recording
     * @param filename Output filename
     * @param overwrite Overwrite existing file
     * @return true if successful
     */
    bool startRecording(const std::string& filename, bool overwrite = false);
    
    /**
     * @brief Stop recording
     * @return Output filename
     */
    std::string stopRecording();
    
    /**
     * @brief Pause recording
     */
    void pauseRecording();
    
    /**
     * @brief Resume recording
     */
    void resumeRecording();
    
    /**
     * @brief Check if recording
     */
    bool isRecording() const { return state == RecorderState::RECORDING; }
    
    /**
     * @brief Check if paused
     */
    bool isPaused() const { return state == RecorderState::PAUSED; }
    
    /**
     * @brief Get current state
     */
    RecorderState getState() const { return state; }
    
    /**
     * @brief Get state name
     */
    std::string getStateName() const;
    
    // ============================================
    // AUDIO PROCESSING
    // ============================================
    
    /**
     * @brief Write audio data
     * @param data Audio data
     * @return true if successful
     */
    bool writeAudio(const AudioData& data);
    
    /**
     * @brief Write raw audio samples
     * @param samples Audio samples
     * @return true if successful
     */
    bool writeAudio(const std::vector<int16_t>& samples);
    
    /**
     * @brief Write audio from interface
     * @param buffer Audio buffer
     * @param frames Number of frames
     * @return true if successful
     */
    bool writeAudio(const int16_t* buffer, size_t frames);
    
    /**
     * @brief Write float audio
     * @param buffer Float audio buffer
     * @param frames Number of frames
     * @return true if successful
     */
    bool writeAudioFloat(const float* buffer, size_t frames);
    
    /**
     * @brief Encode and write audio
     * @param samples Raw audio samples
     * @param timestamp Timestamp in microseconds
     * @return true if successful
     */
    bool encodeAudio(const std::vector<int16_t>& samples, int64_t timestamp = 0);
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Get configuration
     */
    AudioConfig getConfig() const { return config; }
    
    /**
     * @brief Set configuration
     */
    bool setConfig(const AudioConfig& config);
    
    /**
     * @brief Set sample rate
     */
    bool setSampleRate(int sample_rate);
    
    /**
     * @brief Set channels
     */
    bool setChannels(int channels);
    
    /**
     * @brief Set bitrate
     */
    bool setBitrate(int bitrate);
    
    /**
     * @brief Set codec
     */
    bool setCodec(Codec codec);
    
    /**
     * @brief Get sample rate
     */
    int getSampleRate() const { return config.sample_rate; }
    
    /**
     * @brief Get channels
     */
    int getChannels() const { return config.channels; }
    
    /**
     * @brief Get bitrate
     */
    int getBitrate() const { return config.bitrate; }
    
    /**
     * @brief Get codec
     */
    Codec getCodec() const { return config.codec; }
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get recording statistics
     */
    RecordingStats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnAudio(AudioCallback callback);
    void setOnStats(StatsCallback callback);
    void setOnStateChange(StateCallback callback);
    void setOnError(ErrorCallback callback);
    void setOnProgress(ProgressCallback callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get codec name
     */
    std::string getCodecName(Codec codec) const;
    
    /**
     * @brief Get container name
     */
    std::string getContainerName(Container container) const;
    
    /**
     * @brief Get quality name
     */
    std::string getQualityName(Quality quality) const;
    
    /**
     * @brief Get file extension
     */
    std::string getFileExtension(Container container) const;
    
    /**
     * @brief Generate filename with timestamp
     */
    std::string generateFilename(const std::string& prefix = "audio") const;
    
    /**
     * @brief Test recording
     */
    bool testRecording(int duration_seconds = 5);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    AudioConfig config;
    RecorderState state = RecorderState::IDLE;
    RecordingStats stats;
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    mutable std::mutex queue_mutex;
    
    // Recording file
    std::string current_file;
    std::ofstream output_file;
    
    // Audio queue
    std::queue<AudioData> audio_queue;
    size_t max_queue_size = 100;
    
    // Encoding thread
    std::thread encode_thread;
    std::atomic<bool> encoding;
    std::atomic<bool> paused;
    
    // FFmpeg context (opaque pointer)
    void* av_format_ctx = nullptr;
    void* av_codec_ctx = nullptr;
    void* av_frame = nullptr;
    void* av_packet = nullptr;
    void* av_fifo = nullptr;
    
    // Callbacks
    AudioCallback on_audio;
    StatsCallback on_stats;
    StateCallback on_state_change;
    ErrorCallback on_error;
    ProgressCallback on_progress;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // FFmpeg functions
    bool initFFmpegAudio();
    void cleanupFFmpegAudio();
    bool openOutputAudio(const std::string& filename);
    bool writeHeaderAudio();
    bool writeTrailerAudio();
    bool encodeAudioFrame(const AudioData& data);
    bool flushAudioEncoder();
    
    // Thread functions
    void encodeLoopAudio();
    void processAudio(const AudioData& data);
    
    // Statistics
    void updateStats(const AudioData& data);
    void notifyStats();
    
    // State management
    void setState(RecorderState new_state);
    void notifyStateChange();
    void notifyError(const std::string& error);
    void notifyProgress(float progress);
    
    // Helper functions
    std::string getCodecString(Codec codec) const;
    std::string getContainerString(Container container) const;
    int getCodecID(Codec codec) const;
    int getContainerID(Container container) const;
    int bytesPerSample() const;
    std::string getTimestamp() const;
    uint64_t getCurrentTimeMicros() const;
};

#endif // AUDIO_RECORDER_HPP
