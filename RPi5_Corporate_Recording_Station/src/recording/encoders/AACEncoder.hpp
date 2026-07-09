#ifndef AAC_ENCODER_HPP
#define AAC_ENCODER_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <map>
#include <queue>

/**
 * @brief AAC Audio Encoder for Raspberry Pi 5
 * 
 * Provides AAC audio encoding with support for
 * various sample rates and bitrates.
 * Optimized for ARM Cortex-A76.
 */
class AACEncoder {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Profile {
        AAC_LC,
        HE_AAC,
        HE_AAC_V2,
        AAC_LD,
        AAC_ELD
    };
    
    enum class EncoderState {
        IDLE,
        INITIALIZED,
        ENCODING,
        FLUSHING,
        ERROR
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief AAC encoder configuration
     */
    struct Config {
        int sample_rate = 48000;
        int channels = 2;
        int bitrate = 128000;           // 128 Kbps
        int frame_size = 1024;          // Samples per frame
        Profile profile = Profile::AAC_LC;
        std::string output_format = "adts";  // adts, raw, m4a
        std::map<std::string, std::string> extra_options;
    };
    
    /**
     * @brief Encoded audio frame
     */
    struct EncodedFrame {
        std::vector<uint8_t> data;
        int64_t pts = 0;                // Presentation timestamp
        int64_t duration = 0;
        int sample_rate = 0;
        int channels = 0;
        int bitrate = 0;
        bool is_keyframe = true;
        int size_bytes = 0;
    };
    
    /**
     * @brief Encoder statistics
     */
    struct Stats {
        uint64_t frames_encoded = 0;
        uint64_t samples_encoded = 0;
        uint64_t bytes_encoded = 0;
        double bitrate_actual = 0;
        double cpu_usage = 0;
        double encode_time_ms = 0;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_frame_time;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using FrameCallback = std::function<void(const EncodedFrame&)>;
    using StatsCallback = std::function<void(const Stats&)>;
    using StateCallback = std::function<void(EncoderState)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    AACEncoder();
    ~AACEncoder();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize encoder
     * @param config Encoder configuration
     * @return true if successful
     */
    bool initialize(const Config& config = Config());
    
    /**
     * @brief Initialize with parameters
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @param bitrate Bitrate in bps
     * @return true if successful
     */
    bool initialize(int sample_rate, int channels = 2, int bitrate = 128000);
    
    /**
     * @brief Shutdown encoder
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    // ============================================
    // ENCODING
    // ============================================
    
    /**
     * @brief Encode audio samples
     * @param samples Audio samples (interleaved)
     * @param pts Presentation timestamp
     * @return true if successful
     */
    bool encodeAudio(const std::vector<int16_t>& samples, int64_t pts = 0);
    
    /**
     * @brief Encode audio samples (float)
     * @param samples Float audio samples
     * @param pts Presentation timestamp
     * @return true if successful
     */
    bool encodeAudioFloat(const std::vector<float>& samples, int64_t pts = 0);
    
    /**
     * @brief Flush encoder
     */
    bool flush();
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Get configuration
     */
    Config getConfig() const { return config; }
    
    /**
     * @brief Set bitrate
     */
    bool setBitrate(int bitrate);
    
    /**
     * @brief Set sample rate
     */
    bool setSampleRate(int sample_rate);
    
    /**
     * @brief Set channels
     */
    bool setChannels(int channels);
    
    /**
     * @brief Set profile
     */
    bool setProfile(Profile profile);
    
    /**
     * @brief Get bitrate
     */
    int getBitrate() const { return config.bitrate; }
    
    /**
     * @brief Get sample rate
     */
    int getSampleRate() const { return config.sample_rate; }
    
    /**
     * @brief Get channels
     */
    int getChannels() const { return config.channels; }
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get encoder statistics
     */
    Stats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    /**
     * @brief Get state
     */
    EncoderState getState() const { return state; }
    
    /**
     * @brief Get state name
     */
    std::string getStateName() const;
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnFrame(FrameCallback callback);
    void setOnStats(StatsCallback callback);
    void setOnStateChange(StateCallback callback);
    void setOnError(ErrorCallback callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get profile name
     */
    std::string getProfileName(Profile profile) const;
    
    /**
     * @brief Test encoder
     */
    bool testEncoder(int duration_seconds = 5);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    Config config;
    EncoderState state = EncoderState::IDLE;
    Stats stats;
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    mutable std::mutex queue_mutex;
    
    // Frame queue
    std::queue<EncodedFrame> frame_queue;
    size_t max_queue_size = 100;
    
    // FFmpeg context
    void* av_codec_ctx = nullptr;
    void* av_frame = nullptr;
    void* av_packet = nullptr;
    
    // Callbacks
    FrameCallback on_frame;
    StatsCallback on_stats;
    StateCallback on_state_change;
    ErrorCallback on_error;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // FFmpeg functions
    bool initFFmpegEncoder();
    void cleanupFFmpegEncoder();
    bool openEncoder();
    bool closeEncoder();
    bool sendFrame(void* frame);
    bool receivePacket();
    
    // Frame conversion
    bool convertToAudioFrame(const std::vector<int16_t>& samples, void*& out_frame);
    bool convertToAudioFrame(const std::vector<float>& samples, void*& out_frame);
    
    // Statistics
    void updateStats(const EncodedFrame& frame);
    void notifyStats();
    
    // State management
    void setState(EncoderState new_state);
    void notifyStateChange();
    void notifyError(const std::string& error);
    
    // Helper functions
    std::string getProfileString(Profile profile) const;
    int getSampleFormat() const;
    int getChannelLayout() const;
    void delayMicroseconds(int us);
    uint64_t getCurrentTimeMicros() const;
    
    // Profile mapping
    static const std::map<Profile, std::string> profile_map;
};

#endif // AAC_ENCODER_HPP
