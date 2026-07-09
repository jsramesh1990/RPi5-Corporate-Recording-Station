#ifndef AUDIO_INTERFACE_HPP
#define AUDIO_INTERFACE_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>

/**
 * @brief Audio Interface for Recording Station
 * 
 * Provides audio capture and playback functionality
 * with support for ALSA, PulseAudio, and USB audio devices.
 * Optimized for Raspberry Pi 5 ARM Cortex-A76.
 */
class AudioInterface {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class AudioFormat {
        S16_LE,     // Signed 16-bit Little Endian
        S24_LE,     // Signed 24-bit Little Endian
        S32_LE,     // Signed 32-bit Little Endian
        FLOAT_LE,   // 32-bit Float Little Endian
        U8,         // Unsigned 8-bit
        S16_BE,     // Signed 16-bit Big Endian
        S24_BE,     // Signed 24-bit Big Endian
        S32_BE      // Signed 32-bit Big Endian
    };
    
    enum class AudioState {
        IDLE,
        RECORDING,
        PLAYING,
        PAUSED,
        ERROR
    };
    
    enum class AudioInterfaceType {
        ALSA,
        PULSE,
        USB,
        I2S,
        HDMI
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief Audio configuration
     */
    struct AudioConfig {
        std::string device = "hw:0,0";      // Audio device
        AudioInterfaceType interface_type = AudioInterfaceType::ALSA;
        AudioFormat format = AudioFormat::S16_LE;
        int sample_rate = 48000;             // Hz
        int channels = 2;                    // 1=mono, 2=stereo
        int bits_per_sample = 16;
        int buffer_size = 1024;              // Frames
        int period_size = 256;               // Frames
        int period_count = 4;
        bool nonblocking = true;
        bool mmap = false;
        int latency_ms = 10;                 // Target latency
    };
    
    /**
     * @brief Audio statistics
     */
    struct AudioStats {
        uint64_t total_samples = 0;
        uint64_t total_bytes = 0;
        uint64_t underruns = 0;
        uint64_t overruns = 0;
        uint64_t xruns = 0;
        double sample_rate_actual = 0;
        int buffer_size_actual = 0;
        int period_size_actual = 0;
        double latency_actual_ms = 0;
        double cpu_usage_percent = 0;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_update;
    };
    
    /**
     * @brief Audio device information
     */
    struct DeviceInfo {
        std::string name;
        std::string description;
        std::string device_path;
        AudioInterfaceType type;
        bool is_input;
        bool is_output;
        std::vector<int> supported_sample_rates;
        std::vector<int> supported_channels;
        std::vector<AudioFormat> supported_formats;
        int max_channels = 0;
        bool is_default = false;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using AudioDataCallback = std::function<void(const std::vector<int16_t>&)>;
    using AudioFloatCallback = std::function<void(const std::vector<float>&)>;
    using AudioErrorCallback = std::function<void(const std::string&)>;
    using AudioStateCallback = std::function<void(AudioState)>;
    using AudioStatsCallback = std::function<void(const AudioStats&)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    AudioInterface();
    ~AudioInterface();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize audio interface
     * @param config Audio configuration
     * @return true if successful
     */
    bool initialize(const AudioConfig& config = AudioConfig());
    
    /**
     * @brief Initialize with specific device
     * @param device_name Device name (e.g., "hw:0,0")
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @return true if successful
     */
    bool initialize(const std::string& device_name, int sample_rate = 48000, int channels = 2);
    
    /**
     * @brief Shutdown audio interface
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool is_initialized() const { return initialized; }
    
    // ============================================
    // DEVICE MANAGEMENT
    // ============================================
    
    /**
     * @brief List available audio devices
     * @return Vector of device information
     */
    std::vector<DeviceInfo> list_devices() const;
    
    /**
     * @brief Get default input device
     */
    std::string get_default_input_device() const;
    
    /**
     * @brief Get default output device
     */
    std::string get_default_output_device() const;
    
    /**
     * @brief Set audio device
     */
    bool set_device(const std::string& device_name);
    
    /**
     * @brief Get current device name
     */
    std::string get_device() const { return config.device; }
    
    // ============================================
    // CAPTURE (RECORDING)
    // ============================================
    
    /**
     * @brief Start audio capture
     * @return true if successful
     */
    bool start_capture();
    
    /**
     * @brief Stop audio capture
     */
    void stop_capture();
    
    /**
     * @brief Pause audio capture
     */
    void pause_capture();
    
    /**
     * @brief Resume audio capture
     */
    void resume_capture();
    
    /**
     * @brief Check if capturing
     */
    bool is_capturing() const { return state == AudioState::RECORDING; }
    
    /**
     * @brief Read audio data (blocking)
     * @param buffer Buffer to fill
     * @param frames Number of frames to read
     * @return Number of frames read, -1 on error
     */
    int read_audio(int16_t* buffer, size_t frames);
    
    /**
     * @brief Read audio data (blocking, float)
     * @param buffer Buffer to fill
     * @param frames Number of frames to read
     * @return Number of frames read, -1 on error
     */
    int read_audio(float* buffer, size_t frames);
    
    /**
     * @brief Read audio data (non-blocking)
     * @return Vector of audio samples
     */
    std::vector<int16_t> read_audio();
    
    /**
     * @brief Get capture buffer size
     */
    size_t get_capture_buffer_size() const;
    
    /**
     * @brief Get available capture frames
     */
    size_t get_available_frames() const;
    
    // ============================================
    // PLAYBACK
    // ============================================
    
    /**
     * @brief Start audio playback
     * @return true if successful
     */
    bool start_playback();
    
    /**
     * @brief Stop audio playback
     */
    void stop_playback();
    
    /**
     * @brief Pause audio playback
     */
    void pause_playback();
    
    /**
     * @brief Resume audio playback
     */
    void resume_playback();
    
    /**
     * @brief Check if playing
     */
    bool is_playing() const { return state == AudioState::PLAYING; }
    
    /**
     * @brief Write audio data (blocking)
     * @param buffer Audio data buffer
     * @param frames Number of frames to write
     * @return Number of frames written, -1 on error
     */
    int write_audio(const int16_t* buffer, size_t frames);
    
    /**
     * @brief Write audio data (blocking, float)
     * @param buffer Audio data buffer
     * @param frames Number of frames to write
     * @return Number of frames written, -1 on error
     */
    int write_audio(const float* buffer, size_t frames);
    
    /**
     * @brief Write audio data (non-blocking)
     * @param data Vector of audio samples
     * @return Number of frames written
     */
    size_t write_audio(const std::vector<int16_t>& data);
    
    /**
     * @brief Queue audio for playback
     * @param data Audio data to play
     * @return true if queued successfully
     */
    bool queue_audio(const std::vector<int16_t>& data);
    
    /**
     * @brief Clear playback queue
     */
    void clear_playback_queue();
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    /**
     * @brief Set audio data callback (for real-time processing)
     * @param callback Function called with audio data
     */
    void set_on_audio_data(AudioDataCallback callback);
    
    /**
     * @brief Set audio error callback
     * @param callback Function called on error
     */
    void set_on_error(AudioErrorCallback callback);
    
    /**
     * @brief Set state change callback
     * @param callback Function called on state change
     */
    void set_on_state_change(AudioStateCallback callback);
    
    /**
     * @brief Set statistics callback
     * @param callback Function called with stats
     */
    void set_on_stats(AudioStatsCallback callback);
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Get current configuration
     */
    AudioConfig get_config() const { return config; }
    
    /**
     * @brief Set sample rate
     */
    bool set_sample_rate(int sample_rate);
    
    /**
     * @brief Set channels
     */
    bool set_channels(int channels);
    
    /**
     * @brief Set buffer size
     */
    bool set_buffer_size(int buffer_size);
    
    /**
     * @brief Get sample rate
     */
    int get_sample_rate() const { return config.sample_rate; }
    
    /**
     * @brief Get channels
     */
    int get_channels() const { return config.channels; }
    
    /**
     * @brief Get bits per sample
     */
    int get_bits_per_sample() const { return config.bits_per_sample; }
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get audio statistics
     */
    AudioStats get_stats() const;
    
    /**
     * @brief Reset statistics
     */
    void reset_stats();
    
    /**
     * @brief Get current state
     */
    AudioState get_state() const { return state; }
    
    /**
     * @brief Get state name
     */
    std::string get_state_name() const;
    
    // ============================================
    // AUDIO PROCESSING
    // ============================================
    
    /**
     * @brief Convert audio format
     */
    void convert_format(const int16_t* input, float* output, size_t frames);
    void convert_format(const float* input, int16_t* output, size_t frames);
    
    /**
     * @brief Apply gain to audio
     */
    void apply_gain(int16_t* buffer, size_t frames, float gain);
    void apply_gain(float* buffer, size_t frames, float gain);
    
    /**
     * @brief Mix audio channels
     */
    void mix_channels(const int16_t* input, int16_t* output, size_t frames, 
                      int input_channels, int output_channels);
    
    /**
     * @brief Resample audio (simple linear interpolation)
     */
    void resample(const int16_t* input, size_t input_frames, 
                  int16_t* output, size_t output_frames,
                  int input_rate, int output_rate);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Test audio device
     */
    bool test_device();
    
    /**
     * @brief Generate test tone
     */
    std::vector<int16_t> generate_tone(float frequency, int duration_ms, float amplitude = 0.5f);
    
    /**
     * @brief Get audio format name
     */
    std::string get_format_name(AudioFormat format) const;
    
    /**
     * @brief Get interface type name
     */
    std::string get_interface_type_name(AudioInterfaceType type) const;
    
    // ============================================
    // PLATFORM-SPECIFIC
    // ============================================
    
    /**
     * @brief Get ALSA device info (ARM64/Raspberry Pi)
     */
    std::vector<DeviceInfo> get_alsa_devices() const;
    
    /**
     * @brief Get I2S audio device info (Raspberry Pi HAT)
     */
    std::vector<DeviceInfo> get_i2s_devices() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    AudioConfig config;
    AudioState state = AudioState::IDLE;
    AudioStats stats;
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    // Audio callbacks
    AudioDataCallback on_audio_data;
    AudioErrorCallback on_error;
    AudioStateCallback on_state_change;
    AudioStatsCallback on_stats;
    
    // Capture thread
    std::thread capture_thread;
    std::atomic<bool> capture_running;
    std::atomic<bool> capture_paused;
    
    // Playback thread
    std::thread playback_thread;
    std::atomic<bool> playback_running;
    std::atomic<bool> playback_paused;
    
    // Playback queue
    std::vector<std::vector<int16_t>> playback_queue;
    std::mutex queue_mutex;
    size_t max_queue_size = 10;
    
    // Internal state
    void* alsa_handle = nullptr;  // ALSA handle (opaque)
    void* pulse_handle = nullptr; // PulseAudio handle (opaque)
    int audio_fd = -1;
    
    // Buffer
    std::vector<int16_t> capture_buffer;
    std::vector<int16_t> playback_buffer;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // ALSA specific
    bool init_alsa(const AudioConfig& cfg);
    void shutdown_alsa();
    int read_alsa(int16_t* buffer, size_t frames);
    int write_alsa(const int16_t* buffer, size_t frames);
    bool set_alsa_params(const AudioConfig& cfg);
    void handle_alsa_error(int err, const std::string& context);
    
    // PulseAudio specific
    bool init_pulse(const AudioConfig& cfg);
    void shutdown_pulse();
    int read_pulse(int16_t* buffer, size_t frames);
    int write_pulse(const int16_t* buffer, size_t frames);
    
    // Thread functions
    void capture_loop();
    void playback_loop();
    
    // Statistics
    void update_stats(size_t bytes_processed);
    void notify_stats();
    
    // State management
    void set_state(AudioState new_state);
    void notify_error(const std::string& error);
    
    // Helper functions
    int format_to_bytes(AudioFormat format) const;
    double get_current_time_ms() const;
    void set_thread_priority(int priority = -10);
    void set_thread_affinity(int cpu = 0);
};

#endif // AUDIO_INTERFACE_HPP
