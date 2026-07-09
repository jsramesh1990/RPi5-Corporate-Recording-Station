#ifndef VIDEO_RECORDER_HPP
#define VIDEO_RECORDER_HPP

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
 * @brief Video Recorder for Raspberry Pi 5
 * 
 * Provides video recording functionality with support for
 * H.264/H.265 encoding, hardware acceleration, and
 * multiple container formats.
 * Optimized for ARM Cortex-A76 with VideoCore VII.
 */
class VideoRecorder {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Codec {
        H264,
        H265,
        MJPEG,
        VP8,
        VP9,
        AV1
    };
    
    enum class Container {
        MP4,
        MKV,
        AVI,
        MOV,
        TS,
        FLV
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
     * @brief Video configuration
     */
    struct VideoConfig {
        int width = 1920;
        int height = 1080;
        int framerate = 30;
        int bitrate = 4000000;      // 4 Mbps
        int gop_size = 30;           // GOP size (keyframe interval)
        Codec codec = Codec::H264;
        Container container = Container::MP4;
        Quality quality = Quality::HIGH;
        std::string preset = "medium";  // FFmpeg preset
        std::string profile = "high";   // H.264 profile
        std::string level = "4.0";      // H.264 level
        bool hardware_acceleration = true;
        bool variable_framerate = false;
        int max_rate = 6000000;      // Max bitrate
        int buffer_size = 8000000;   // VBV buffer size
        int threads = 0;             // 0 = auto
        std::string pixel_format = "yuv420p";
        std::string color_range = "tv";
        std::string color_space = "bt709";
        std::string color_transfer = "bt709";
        std::string color_primaries = "bt709";
        std::map<std::string, std::string> extra_options;
    };
    
    /**
     * @brief Recording statistics
     */
    struct RecordingStats {
        uint64_t frames_recorded = 0;
        uint64_t frames_dropped = 0;
        uint64_t bytes_written = 0;
        double fps_actual = 0;
        double bitrate_actual = 0;
        double duration_seconds = 0;
        double cpu_usage = 0;
        double memory_usage = 0;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_frame_time;
        std::string current_file;
        bool is_recording = false;
    };
    
    /**
     * @brief Frame information
     */
    struct VideoFrame {
        std::vector<uint8_t> data;
        int width = 0;
        int height = 0;
        int64_t pts = 0;           // Presentation timestamp
        int64_t dts = 0;           // Decode timestamp
        int64_t duration = 0;
        bool is_keyframe = false;
        int stream_index = 0;
        std::map<std::string, std::string> metadata;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using FrameCallback = std::function<void(const VideoFrame&)>;
    using StatsCallback = std::function<void(const RecordingStats&)>;
    using StateCallback = std::function<void(RecorderState)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(float)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    VideoRecorder();
    ~VideoRecorder();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize video recorder
     * @param config Video configuration
     * @return true if successful
     */
    bool initialize(const VideoConfig& config = VideoConfig());
    
    /**
     * @brief Initialize with parameters
     * @param width Frame width
     * @param height Frame height
     * @param framerate Frames per second
     * @param bitrate Bitrate in bps
     * @param codec Video codec
     * @return true if successful
     */
    bool initialize(int width, int height, int framerate = 30,
                   int bitrate = 4000000, Codec codec = Codec::H264);
    
    /**
     * @brief Shutdown video recorder
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
    // FRAME PROCESSING
    // ============================================
    
    /**
     * @brief Write video frame
     * @param frame Video frame
     * @return true if successful
     */
    bool writeFrame(const VideoFrame& frame);
    
    /**
     * @brief Write raw frame data
     * @param data Frame data
     * @param width Frame width
     * @param height Frame height
     * @param format Pixel format
     * @return true if successful
     */
    bool writeFrame(const std::vector<uint8_t>& data, int width, int height,
                   const std::string& format = "yuv420p");
    
    /**
     * @brief Write RGB frame
     * @param data RGB data
     * @param width Frame width
     * @param height Frame height
     * @return true if successful
     */
    bool writeRGBFrame(const std::vector<uint8_t>& data, int width, int height);
    
    /**
     * @brief Write YUV frame
     * @param y Y plane
     * @param u U plane
     * @param v V plane
     * @param width Frame width
     * @param height Frame height
     * @return true if successful
     */
    bool writeYUVFrame(const std::vector<uint8_t>& y, 
                      const std::vector<uint8_t>& u,
                      const std::vector<uint8_t>& v,
                      int width, int height);
    
    /**
     * @brief Encode and write frame
     * @param data Raw frame data
     * @param timestamp Timestamp in microseconds
     * @return true if successful
     */
    bool encodeFrame(const std::vector<uint8_t>& data, int64_t timestamp = 0);
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Get configuration
     */
    VideoConfig getConfig() const { return config; }
    
    /**
     * @brief Set configuration
     */
    bool setConfig(const VideoConfig& config);
    
    /**
     * @brief Set resolution
     */
    bool setResolution(int width, int height);
    
    /**
     * @brief Set framerate
     */
    bool setFramerate(int framerate);
    
    /**
     * @brief Set bitrate
     */
    bool setBitrate(int bitrate);
    
    /**
     * @brief Set codec
     */
    bool setCodec(Codec codec);
    
    /**
     * @brief Set quality
     */
    bool setQuality(Quality quality);
    
    /**
     * @brief Get resolution
     */
    std::pair<int, int> getResolution() const { return {config.width, config.height}; }
    
    /**
     * @brief Get framerate
     */
    int getFramerate() const { return config.framerate; }
    
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
    
    void setOnFrame(FrameCallback callback);
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
    std::string generateFilename(const std::string& prefix = "video") const;
    
    /**
     * @brief Test recording
     */
    bool testRecording(int duration_seconds = 5);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    VideoConfig config;
    RecorderState state = RecorderState::IDLE;
    RecordingStats stats;
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    mutable std::mutex queue_mutex;
    
    // Recording file
    std::string current_file;
    std::ofstream output_file;
    
    // Frame queue
    std::queue<VideoFrame> frame_queue;
    size_t max_queue_size = 100;
    
    // Encoding thread
    std::thread encode_thread;
    std::atomic<bool> encoding;
    std::atomic<bool> paused;
    
    // FFmpeg context (opaque pointer)
    void* av_format_ctx = nullptr;
    void* av_codec_ctx = nullptr;
    void* av_sws_ctx = nullptr;
    void* av_frame = nullptr;
    void* av_packet = nullptr;
    
    // Callbacks
    FrameCallback on_frame;
    StatsCallback on_stats;
    StateCallback on_state_change;
    ErrorCallback on_error;
    ProgressCallback on_progress;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // FFmpeg functions
    bool initFFmpeg();
    void cleanupFFmpeg();
    bool openOutput(const std::string& filename);
    bool writeHeader();
    bool writeTrailer();
    bool encodeVideoFrame(const VideoFrame& frame);
    bool flushEncoder();
    
    // Frame conversion
    bool convertFrame(const VideoFrame& frame, void*& out_frame);
    bool convertRGBToYUV(const std::vector<uint8_t>& rgb, int width, int height,
                        std::vector<uint8_t>& yuv);
    
    // Thread functions
    void encodeLoop();
    void processFrame(const VideoFrame& frame);
    
    // Statistics
    void updateStats(const VideoFrame& frame);
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
    void delayMicroseconds(int us);
    std::string getTimestamp() const;
    uint64_t getCurrentTimeMicros() const;
};

#endif // VIDEO_RECORDER_HPP
