#ifndef H264_ENCODER_HPP
#define H264_ENCODER_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <map>
#include <queue>

/**
 * @brief H.264 Video Encoder for Raspberry Pi 5
 * 
 * Provides H.264 encoding with hardware acceleration support
 * using VideoCore VII GPU on Raspberry Pi 5.
 * Optimized for ARM Cortex-A76.
 */
class H264Encoder {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Preset {
        ULTRAFAST,
        SUPERFAST,
        VERYFAST,
        FASTER,
        FAST,
        MEDIUM,
        SLOW,
        SLOWER,
        VERYSLOW,
        PLACEBO
    };
    
    enum class Profile {
        BASELINE,
        MAIN,
        HIGH,
        HIGH_10,
        HIGH_422,
        HIGH_444
    };
    
    enum class Level {
        LEVEL_1_0,
        LEVEL_1_1,
        LEVEL_1_2,
        LEVEL_1_3,
        LEVEL_2_0,
        LEVEL_2_1,
        LEVEL_2_2,
        LEVEL_3_0,
        LEVEL_3_1,
        LEVEL_3_2,
        LEVEL_4_0,
        LEVEL_4_1,
        LEVEL_4_2,
        LEVEL_5_0,
        LEVEL_5_1,
        LEVEL_5_2
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
     * @brief H.264 encoder configuration
     */
    struct Config {
        int width = 1920;
        int height = 1080;
        int framerate = 30;
        int bitrate = 4000000;          // 4 Mbps
        int max_bitrate = 6000000;      // 6 Mbps
        int buffer_size = 8000000;      // 8 MB
        int gop_size = 30;              // GOP size (keyframe interval)
        int b_frames = 2;               // Number of B-frames
        int ref_frames = 3;             // Reference frames
        int threads = 0;                // 0 = auto
        Preset preset = Preset::MEDIUM;
        Profile profile = Profile::HIGH;
        Level level = Level::LEVEL_4_0;
        bool hardware_acceleration = true;
        bool annexb = true;             // Annex B format
        bool repeat_headers = true;     // Repeat SPS/PPS
        bool cbr = false;               // Constant bitrate
        int lookahead = 0;              // Lookahead frames (0 = auto)
        int slice_count = 1;            // Number of slices per frame
        std::map<std::string, std::string> extra_options;
    };
    
    /**
     * @brief Encoded frame
     */
    struct EncodedFrame {
        std::vector<uint8_t> data;
        int64_t pts = 0;                // Presentation timestamp
        int64_t dts = 0;                // Decode timestamp
        int64_t duration = 0;
        bool is_keyframe = false;
        bool is_reference = false;
        int frame_type = 0;             // 0=I, 1=P, 2=B
        int size_bytes = 0;
        int quality_score = 0;
    };
    
    /**
     * @brief Encoder statistics
     */
    struct Stats {
        uint64_t frames_encoded = 0;
        uint64_t keyframes_encoded = 0;
        uint64_t bytes_encoded = 0;
        double fps_actual = 0;
        double bitrate_actual = 0;
        double avg_frame_size = 0;
        double cpu_usage = 0;
        double encode_time_ms = 0;
        int quality_score = 0;
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
    
    H264Encoder();
    ~H264Encoder();
    
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
     * @param width Frame width
     * @param height Frame height
     * @param framerate Frames per second
     * @param bitrate Bitrate in bps
     * @return true if successful
     */
    bool initialize(int width, int height, int framerate = 30, int bitrate = 4000000);
    
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
     * @brief Encode a frame
     * @param data YUV420P frame data
     * @param pts Presentation timestamp
     * @param is_keyframe Force keyframe
     * @return true if successful
     */
    bool encodeFrame(const std::vector<uint8_t>& data, int64_t pts = 0, bool is_keyframe = false);
    
    /**
     * @brief Encode a frame from YUV planes
     * @param y Y plane
     * @param u U plane
     * @param v V plane
     * @param pts Presentation timestamp
     * @return true if successful
     */
    bool encodeFrame(const std::vector<uint8_t>& y, 
                    const std::vector<uint8_t>& u,
                    const std::vector<uint8_t>& v,
                    int64_t pts = 0);
    
    /**
     * @brief Encode a frame from RGB data
     * @param data RGB24 data
     * @param width Frame width
     * @param height Frame height
     * @param pts Presentation timestamp
     * @return true if successful
     */
    bool encodeRGB(const std::vector<uint8_t>& data, int width, int height, int64_t pts = 0);
    
    /**
     * @brief Flush encoder (get remaining frames)
     */
    bool flush();
    
    /**
     * @brief Force keyframe on next frame
     */
    void forceKeyframe() { force_keyframe = true; }
    
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
     * @brief Set framerate
     */
    bool setFramerate(int framerate);
    
    /**
     * @brief Set resolution
     */
    bool setResolution(int width, int height);
    
    /**
     * @brief Set preset
     */
    bool setPreset(Preset preset);
    
    /**
     * @brief Set profile
     */
    bool setProfile(Profile profile);
    
    /**
     * @brief Get bitrate
     */
    int getBitrate() const { return config.bitrate; }
    
    /**
     * @brief Get framerate
     */
    int getFramerate() const { return config.framerate; }
    
    /**
     * @brief Get resolution
     */
    std::pair<int, int> getResolution() const { return {config.width, config.height}; }
    
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
     * @brief Get preset name
     */
    std::string getPresetName(Preset preset) const;
    
    /**
     * @brief Get profile name
     */
    std::string getProfileName(Profile profile) const;
    
    /**
     * @brief Get level name
     */
    std::string getLevelName(Level level) const;
    
    /**
     * @brief Test encoder
     */
    bool testEncoder(int duration_seconds = 5);
    
    /**
     * @brief Get encoder capabilities
     */
    struct Capabilities {
        bool supports_hardware = false;
        bool supports_10bit = false;
        bool supports_444 = false;
        bool supports_lossless = false;
        int max_width = 4096;
        int max_height = 4096;
        int max_framerate = 120;
        int max_bitrate = 100000000;  // 100 Mbps
    };
    
    Capabilities getCapabilities() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    Config config;
    EncoderState state = EncoderState::IDLE;
    Stats stats;
    bool initialized = false;
    bool force_keyframe = false;
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
    void* av_sws_ctx = nullptr;
    void* av_hw_ctx = nullptr;
    
    // Hardware acceleration
    bool hw_accel_available = false;
    
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
    bool setupHardwareAcceleration();
    bool checkHardwareSupport();
    
    // Frame conversion
    bool convertToYUV(const std::vector<uint8_t>& data, int width, int height,
                     const std::string& format, void*& out_frame);
    bool convertRGBToYUV(const std::vector<uint8_t>& rgb, int width, int height,
                        std::vector<uint8_t>& yuv);
    
    // Statistics
    void updateStats(const EncodedFrame& frame);
    void notifyStats();
    
    // State management
    void setState(EncoderState new_state);
    void notifyStateChange();
    void notifyError(const std::string& error);
    
    // Helper functions
    std::string getPresetString(Preset preset) const;
    std::string getProfileString(Profile profile) const;
    std::string getLevelString(Level level) const;
    int getLevelValue(Level level) const;
    void delayMicroseconds(int us);
    uint64_t getCurrentTimeMicros() const;
    
    // Preset mapping
    static const std::map<Preset, std::string> preset_map;
    static const std::map<Profile, std::string> profile_map;
    static const std::map<Level, std::string> level_map;
};

#endif // H264_ENCODER_HPP
