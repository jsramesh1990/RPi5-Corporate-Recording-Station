#include "AudioInterface.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>

// ALSA headers
#include <alsa/asoundlib.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <pthread.h>
#endif

AudioInterface::AudioInterface() 
    : initialized(false), 
      capture_running(false), 
      capture_paused(false),
      playback_running(false),
      playback_paused(false),
      alsa_handle(nullptr) {
    
    // Initialize default configuration
    config.device = "default";
    config.sample_rate = 48000;
    config.channels = 2;
    config.bits_per_sample = 16;
    config.buffer_size = 1024;
    config.period_size = 256;
    config.period_count = 4;
    config.format = AudioFormat::S16_LE;
    config.nonblocking = true;
    
    // Set thread priority
    set_thread_priority(-10);
}

AudioInterface::~AudioInterface() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool AudioInterface::initialize(const AudioConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        shutdown_alsa();
        shutdown_pulse();
        initialized = false;
    }
    
    config = cfg;
    
    // Convert format to ALSA
    snd_pcm_format_t alsa_format;
    switch (config.format) {
        case AudioFormat::S16_LE:
            alsa_format = SND_PCM_FORMAT_S16_LE;
            config.bits_per_sample = 16;
            break;
        case AudioFormat::S24_LE:
            alsa_format = SND_PCM_FORMAT_S24_LE;
            config.bits_per_sample = 24;
            break;
        case AudioFormat::S32_LE:
            alsa_format = SND_PCM_FORMAT_S32_LE;
            config.bits_per_sample = 32;
            break;
        case AudioFormat::FLOAT_LE:
            alsa_format = SND_PCM_FORMAT_FLOAT_LE;
            config.bits_per_sample = 32;
            break;
        case AudioFormat::U8:
            alsa_format = SND_PCM_FORMAT_U8;
            config.bits_per_sample = 8;
            break;
        default:
            alsa_format = SND_PCM_FORMAT_S16_LE;
            config.bits_per_sample = 16;
            break;
    }
    
    // Try to initialize ALSA first
    bool success = false;
    if (config.interface_type == AudioInterfaceType::ALSA || 
        config.interface_type == AudioInterfaceType::USB) {
        success = init_alsa(config);
    }
    
    // Fallback to PulseAudio if ALSA fails
    if (!success && config.interface_type == AudioInterfaceType::PULSE) {
        success = init_pulse(config);
    }
    
    if (!success) {
        std::cerr << "Failed to initialize audio interface" << std::endl;
        return false;
    }
    
    initialized = true;
    state = AudioState::IDLE;
    reset_stats();
    
    std::cout << "Audio interface initialized: " << config.device 
              << " (" << config.sample_rate << "Hz, " << config.channels 
              << "ch, " << config.bits_per_sample << "bit)" << std::endl;
    
    return true;
}

bool AudioInterface::initialize(const std::string& device_name, int sample_rate, int channels) {
    AudioConfig cfg;
    cfg.device = device_name;
    cfg.sample_rate = sample_rate;
    cfg.channels = channels;
    cfg.format = AudioFormat::S16_LE;
    cfg.bits_per_sample = 16;
    cfg.buffer_size = 1024;
    cfg.period_size = 256;
    cfg.period_count = 4;
    return initialize(cfg);
}

void AudioInterface::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Stop capture and playback
    if (capture_running) {
        stop_capture();
    }
    if (playback_running) {
        stop_playback();
    }
    
    // Clean up ALSA
    shutdown_alsa();
    
    // Clean up PulseAudio
    shutdown_pulse();
    
    initialized = false;
    state = AudioState::IDLE;
    
    std::cout << "Audio interface shutdown" << std::endl;
}

// ============================================
// ALSA IMPLEMENTATION
// ============================================

bool AudioInterface::init_alsa(const AudioConfig& cfg) {
    snd_pcm_t* handle = nullptr;
    int err;
    
    // Open PCM device
    err = snd_pcm_open(&handle, cfg.device.c_str(), 
                       SND_PCM_STREAM_CAPTURE, 
                       cfg.nonblocking ? SND_PCM_NONBLOCK : 0);
    if (err < 0) {
        std::cerr << "Failed to open ALSA capture device: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Set hardware parameters
    if (!set_alsa_params(cfg)) {
        snd_pcm_close(handle);
        return false;
    }
    
    alsa_handle = handle;
    audio_fd = snd_pcm_file_descriptor(handle);
    
    return true;
}

void AudioInterface::shutdown_alsa() {
    if (alsa_handle) {
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
        alsa_handle = nullptr;
    }
    audio_fd = -1;
}

bool AudioInterface::set_alsa_params(const AudioConfig& cfg) {
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    int err;
    
    // Set hardware parameters
    snd_pcm_hw_params_t* hw_params = nullptr;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(handle, hw_params);
    
    // Set access
    err = snd_pcm_hw_params_set_access(handle, hw_params, 
                                        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "Failed to set access: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Set format
    snd_pcm_format_t format;
    switch (cfg.format) {
        case AudioFormat::S16_LE: format = SND_PCM_FORMAT_S16_LE; break;
        case AudioFormat::S24_LE: format = SND_PCM_FORMAT_S24_LE; break;
        case AudioFormat::S32_LE: format = SND_PCM_FORMAT_S32_LE; break;
        case AudioFormat::FLOAT_LE: format = SND_PCM_FORMAT_FLOAT_LE; break;
        case AudioFormat::U8: format = SND_PCM_FORMAT_U8; break;
        default: format = SND_PCM_FORMAT_S16_LE; break;
    }
    err = snd_pcm_hw_params_set_format(handle, hw_params, format);
    if (err < 0) {
        std::cerr << "Failed to set format: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Set rate
    unsigned int rate = cfg.sample_rate;
    err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, nullptr);
    if (err < 0) {
        std::cerr << "Failed to set rate: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Set channels
    err = snd_pcm_hw_params_set_channels(handle, hw_params, cfg.channels);
    if (err < 0) {
        std::cerr << "Failed to set channels: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Set buffer size
    snd_pcm_uframes_t buffer_size = cfg.buffer_size;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer_size);
    if (err < 0) {
        std::cerr << "Failed to set buffer size: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Set period size
    snd_pcm_uframes_t period_size = cfg.period_size;
    err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period_size, nullptr);
    if (err < 0) {
        std::cerr << "Failed to set period size: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Apply hardware parameters
    err = snd_pcm_hw_params(handle, hw_params);
    if (err < 0) {
        std::cerr << "Failed to set hardware parameters: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Get actual parameters
    config.sample_rate = rate;
    config.buffer_size = buffer_size;
    config.period_size = period_size;
    
    return true;
}

void AudioInterface::handle_alsa_error(int err, const std::string& context) {
    std::string error_msg = context + ": " + snd_strerror(err);
    notify_error(error_msg);
    std::cerr << error_msg << std::endl;
}

// ============================================
// CAPTURE
// ============================================

bool AudioInterface::start_capture() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        notify_error("Not initialized");
        return false;
    }
    
    if (capture_running) {
        return true;  // Already running
    }
    
    // Prepare PCM
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (handle) {
        int err = snd_pcm_prepare(handle);
        if (err < 0) {
            handle_alsa_error(err, "Failed to prepare PCM");
            return false;
        }
    }
    
    // Start capture thread
    capture_running = true;
    capture_paused = false;
    capture_thread = std::thread(&AudioInterface::capture_loop, this);
    
    set_state(AudioState::RECORDING);
    reset_stats();
    
    std::cout << "Audio capture started" << std::endl;
    return true;
}

void AudioInterface::stop_capture() {
    if (!capture_running) {
        return;
    }
    
    capture_running = false;
    capture_paused = false;
    
    if (capture_thread.joinable()) {
        capture_thread.join();
    }
    
    // Stop PCM
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (handle) {
        snd_pcm_drop(handle);
    }
    
    set_state(AudioState::IDLE);
    std::cout << "Audio capture stopped" << std::endl;
}

void AudioInterface::pause_capture() {
    if (capture_running && !capture_paused) {
        capture_paused = true;
        set_state(AudioState::PAUSED);
        std::cout << "Audio capture paused" << std::endl;
    }
}

void AudioInterface::resume_capture() {
    if (capture_running && capture_paused) {
        capture_paused = false;
        set_state(AudioState::RECORDING);
        std::cout << "Audio capture resumed" << std::endl;
    }
}

void AudioInterface::capture_loop() {
    // Set thread priority
    set_thread_priority(-10);
    set_thread_affinity(2);  // Use core 2
    
    int frame_size = config.channels * (config.bits_per_sample / 8);
    int buffer_size = config.period_size * frame_size;
    
    std::vector<int16_t> buffer(config.period_size * config.channels);
    
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    
    while (capture_running) {
        if (capture_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Read audio
        snd_pcm_sframes_t frames_read = snd_pcm_readi(handle, buffer.data(), config.period_size);
        
        if (frames_read < 0) {
            // Handle xrun
            if (frames_read == -EPIPE) {
                snd_pcm_prepare(handle);
                stats.xruns++;
                continue;
            } else {
                std::cerr << "Read error: " << snd_strerror(frames_read) << std::endl;
                break;
            }
        } else if (frames_read > 0) {
            // Resize buffer to actual frames
            size_t samples = frames_read * config.channels;
            
            // Update stats
            update_stats(frames_read * frame_size);
            
            // Call callback
            if (on_audio_data) {
                std::vector<int16_t> data(buffer.begin(), buffer.begin() + samples);
                on_audio_data(data);
            }
        }
    }
}

int AudioInterface::read_audio(int16_t* buffer, size_t frames) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || !capture_running) {
        return -1;
    }
    
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (!handle) {
        return -1;
    }
    
    snd_pcm_sframes_t read_frames = snd_pcm_readi(handle, buffer, frames);
    
    if (read_frames < 0) {
        if (read_frames == -EPIPE) {
            snd_pcm_prepare(handle);
            stats.xruns++;
        } else {
            std::cerr << "Read error: " << snd_strerror(read_frames) << std::endl;
        }
        return -1;
    }
    
    update_stats(read_frames * config.channels * (config.bits_per_sample / 8));
    return read_frames;
}

int AudioInterface::read_audio(float* buffer, size_t frames) {
    // Allocate int16_t buffer
    std::vector<int16_t> int_buffer(frames * config.channels);
    int read_frames = read_audio(int_buffer.data(), frames);
    
    if (read_frames < 0) {
        return -1;
    }
    
    // Convert to float
    convert_format(int_buffer.data(), buffer, read_frames);
    return read_frames;
}

std::vector<int16_t> AudioInterface::read_audio() {
    std::vector<int16_t> result;
    
    if (!initialized || !capture_running) {
        return result;
    }
    
    // Non-blocking read
    size_t available = get_available_frames();
    if (available == 0) {
        return result;
    }
    
    size_t frames = std::min(available, size_t(config.period_size));
    result.resize(frames * config.channels);
    
    int read_frames = read_audio(result.data(), frames);
    if (read_frames < 0) {
        result.clear();
    } else if (read_frames < (int)frames) {
        result.resize(read_frames * config.channels);
    }
    
    return result;
}

size_t AudioInterface::get_available_frames() const {
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (!handle || !capture_running) {
        return 0;
    }
    
    snd_pcm_sframes_t avail = snd_pcm_avail_update(handle);
    if (avail < 0) {
        return 0;
    }
    
    return avail;
}

size_t AudioInterface::get_capture_buffer_size() const {
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (!handle) {
        return 0;
    }
    
    snd_pcm_uframes_t buffer_size;
    snd_pcm_get_params(handle, &buffer_size, nullptr);
    return buffer_size;
}

// ============================================
// PLAYBACK
// ============================================

bool AudioInterface::start_playback() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        notify_error("Not initialized");
        return false;
    }
    
    if (playback_running) {
        return true;
    }
    
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (handle) {
        int err = snd_pcm_prepare(handle);
        if (err < 0) {
            handle_alsa_error(err, "Failed to prepare PCM for playback");
            return false;
        }
    }
    
    playback_running = true;
    playback_paused = false;
    playback_thread = std::thread(&AudioInterface::playback_loop, this);
    
    set_state(AudioState::PLAYING);
    std::cout << "Audio playback started" << std::endl;
    return true;
}

void AudioInterface::stop_playback() {
    if (!playback_running) {
        return;
    }
    
    playback_running = false;
    playback_paused = false;
    
    if (playback_thread.joinable()) {
        playback_thread.join();
    }
    
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (handle) {
        snd_pcm_drop(handle);
    }
    
    set_state(AudioState::IDLE);
    std::cout << "Audio playback stopped" << std::endl;
}

void AudioInterface::pause_playback() {
    if (playback_running && !playback_paused) {
        playback_paused = true;
        set_state(AudioState::PAUSED);
        std::cout << "Audio playback paused" << std::endl;
    }
}

void AudioInterface::resume_playback() {
    if (playback_running && playback_paused) {
        playback_paused = false;
        set_state(AudioState::PLAYING);
        std::cout << "Audio playback resumed" << std::endl;
    }
}

void AudioInterface::playback_loop() {
    // Set thread priority
    set_thread_priority(-10);
    set_thread_affinity(3);
    
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    
    while (playback_running) {
        if (playback_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Get next buffer from queue
        std::vector<int16_t> buffer;
        {
            std::lock_guard<std::mutex> qlock(queue_mutex);
            if (!playback_queue.empty()) {
                buffer = std::move(playback_queue.front());
                playback_queue.erase(playback_queue.begin());
            }
        }
        
        if (buffer.empty()) {
            // No data, sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        // Write audio
        snd_pcm_sframes_t frames_written = snd_pcm_writei(handle, buffer.data(), 
                                                          buffer.size() / config.channels);
        
        if (frames_written < 0) {
            if (frames_written == -EPIPE) {
                snd_pcm_prepare(handle);
                stats.xruns++;
            } else {
                std::cerr << "Write error: " << snd_strerror(frames_written) << std::endl;
            }
        } else {
            update_stats(frames_written * config.channels * (config.bits_per_sample / 8));
        }
    }
}

int AudioInterface::write_audio(const int16_t* buffer, size_t frames) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || !playback_running) {
        return -1;
    }
    
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsa_handle);
    if (!handle) {
        return -1;
    }
    
    snd_pcm_sframes_t written = snd_pcm_writei(handle, buffer, frames);
    
    if (written < 0) {
        if (written == -EPIPE) {
            snd_pcm_prepare(handle);
            stats.xruns++;
        } else {
            std::cerr << "Write error: " << snd_strerror(written) << std::endl;
        }
        return -1;
    }
    
    update_stats(written * config.channels * (config.bits_per_sample / 8));
    return written;
}

int AudioInterface::write_audio(const float* buffer, size_t frames) {
    // Convert to int16_t
    std::vector<int16_t> int_buffer(frames * config.channels);
    convert_format(buffer, int_buffer.data(), frames);
    return write_audio(int_buffer.data(), frames);
}

size_t AudioInterface::write_audio(const std::vector<int16_t>& data) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    if (playback_queue.size() >= max_queue_size) {
        return 0;
    }
    
    playback_queue.push_back(data);
    return data.size();
}

bool AudioInterface::queue_audio(const std::vector<int16_t>& data) {
    return write_audio(data) > 0;
}

void AudioInterface::clear_playback_queue() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    playback_queue.clear();
}

// ============================================
// CALLBACKS
// ============================================

void AudioInterface::set_on_audio_data(AudioDataCallback callback) {
    on_audio_data = callback;
}

void AudioInterface::set_on_error(AudioErrorCallback callback) {
    on_error = callback;
}

void AudioInterface::set_on_state_change(AudioStateCallback callback) {
    on_state_change = callback;
}

void AudioInterface::set_on_stats(AudioStatsCallback callback) {
    on_stats = callback;
}

// ============================================
// STATISTICS
// ============================================

AudioInterface::AudioStats AudioInterface::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void AudioInterface::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = AudioStats{};
    stats.start_time = std::chrono::system_clock::now();
    stats.last_update = stats.start_time;
}

void AudioInterface::update_stats(size_t bytes_processed) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.total_bytes += bytes_processed;
    stats.total_samples += bytes_processed / (config.bits_per_sample / 8);
    stats.last_update = std::chrono::system_clock::now();
    
    // Calculate CPU usage (simplified)
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        stats.last_update - stats.start_time).count();
    if (elapsed > 0) {
        stats.cpu_usage_percent = (stats.total_bytes * 8.0) / 
                                  (elapsed * (config.sample_rate / 1000.0));
    }
}

void AudioInterface::notify_stats() {
    if (on_stats) {
        auto stats_copy = get_stats();
        on_stats(stats_copy);
    }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void AudioInterface::set_state(AudioState new_state) {
    if (state != new_state) {
        state = new_state;
        if (on_state_change) {
            on_state_change(state);
        }
    }
}

void AudioInterface::notify_error(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
}

std::string AudioInterface::get_state_name() const {
    switch (state) {
        case AudioState::IDLE: return "IDLE";
        case AudioState::RECORDING: return "RECORDING";
        case AudioState::PLAYING: return "PLAYING";
        case AudioState::PAUSED: return "PAUSED";
        case AudioState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ============================================
// AUDIO PROCESSING
// ============================================

void AudioInterface::convert_format(const int16_t* input, float* output, size_t frames) {
    size_t samples = frames * config.channels;
    for (size_t i = 0; i < samples; i++) {
        output[i] = static_cast<float>(input[i]) / 32768.0f;
    }
}

void AudioInterface::convert_format(const float* input, int16_t* output, size_t frames) {
    size_t samples = frames * config.channels;
    for (size_t i = 0; i < samples; i++) {
        float val = input[i] * 32767.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        output[i] = static_cast<int16_t>(val);
    }
}

void AudioInterface::apply_gain(int16_t* buffer, size_t frames, float gain) {
    size_t samples = frames * config.channels;
    for (size_t i = 0; i < samples; i++) {
        buffer[i] = static_cast<int16_t>(buffer[i] * gain);
    }
}

void AudioInterface::apply_gain(float* buffer, size_t frames, float gain) {
    size_t samples = frames * config.channels;
    for (size_t i = 0; i < samples; i++) {
        buffer[i] *= gain;
    }
}

void AudioInterface::mix_channels(const int16_t* input, int16_t* output, size_t frames,
                                 int input_channels, int output_channels) {
    // Simple channel mixing (sum and average)
    for (size_t i = 0; i < frames; i++) {
        int sum = 0;
        for (int c = 0; c < input_channels && c < output_channels; c++) {
            sum += input[i * input_channels + c];
        }
        output[i] = sum / std::min(input_channels, output_channels);
        
        // Copy if more output channels
        for (int c = 1; c < output_channels; c++) {
            output[i * output_channels + c] = output[i];
        }
    }
}

void AudioInterface::resample(const int16_t* input, size_t input_frames,
                             int16_t* output, size_t output_frames,
                             int input_rate, int output_rate) {
    if (input_frames == 0 || output_frames == 0) {
        return;
    }
    
    float ratio = static_cast<float>(output_frames) / input_frames;
    
    for (size_t i = 0; i < output_frames; i++) {
        float pos = i / ratio;
        size_t index = static_cast<size_t>(pos);
        float frac = pos - index;
        
        if (index >= input_frames - 1) {
            output[i] = input[input_frames - 1];
        } else {
            // Linear interpolation
            int16_t sample0 = input[index];
            int16_t sample1 = input[index + 1];
            output[i] = static_cast<int16_t>(sample0 + frac * (sample1 - sample0));
        }
    }
}

// ============================================
// UTILITY
// ============================================

std::vector<int16_t> AudioInterface::generate_tone(float frequency, int duration_ms, float amplitude) {
    std::vector<int16_t> result;
    int num_samples = static_cast<int>(config.sample_rate * duration_ms / 1000.0);
    result.resize(num_samples * config.channels);
    
    for (int i = 0; i < num_samples; i++) {
        float t = static_cast<float>(i) / config.sample_rate;
        float sample = amplitude * 32767.0f * sin(2.0f * M_PI * frequency * t);
        int16_t val = static_cast<int16_t>(sample);
        
        for (int c = 0; c < config.channels; c++) {
            result[i * config.channels + c] = val;
        }
    }
    
    return result;
}

std::string AudioInterface::get_format_name(AudioFormat format) const {
    switch (format) {
        case AudioFormat::S16_LE: return "S16_LE";
        case AudioFormat::S24_LE: return "S24_LE";
        case AudioFormat::S32_LE: return "S32_LE";
        case AudioFormat::FLOAT_LE: return "FLOAT_LE";
        case AudioFormat::U8: return "U8";
        case AudioFormat::S16_BE: return "S16_BE";
        case AudioFormat::S24_BE: return "S24_BE";
        case AudioFormat::S32_BE: return "S32_BE";
        default: return "UNKNOWN";
    }
}

std::string AudioInterface::get_interface_type_name(AudioInterfaceType type) const {
    switch (type) {
        case AudioInterfaceType::ALSA: return "ALSA";
        case AudioInterfaceType::PULSE: return "PulseAudio";
        case AudioInterfaceType::USB: return "USB";
        case AudioInterfaceType::I2S: return "I2S";
        case AudioInterfaceType::HDMI: return "HDMI";
        default: return "UNKNOWN";
    }
}

bool AudioInterface::test_device() {
    std::cout << "Testing audio device..." << std::endl;
    
    // Generate a test tone
    auto tone = generate_tone(440, 1000, 0.3f);
    
    // Play the tone
    if (!start_playback()) {
        return false;
    }
    
    write_audio(tone);
    
    // Wait for playback
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    stop_playback();
    
    std::cout << "Device test complete" << std::endl;
    return true;
}

// ============================================
// PLATFORM-SPECIFIC
// ============================================

std::vector<AudioInterface::DeviceInfo> AudioInterface::list_devices() const {
    std::vector<DeviceInfo> devices;
    
    // Get ALSA devices
    auto alsa_devices = get_alsa_devices();
    devices.insert(devices.end(), alsa_devices.begin(), alsa_devices.end());
    
    // Get I2S devices
    auto i2s_devices = get_i2s_devices();
    devices.insert(devices.end(), i2s_devices.begin(), i2s_devices.end());
    
    return devices;
}

std::vector<AudioInterface::DeviceInfo> AudioInterface::get_alsa_devices() const {
    std::vector<DeviceInfo> devices;
    
    int card = -1;
    snd_card_next(&card);
    
    while (card >= 0) {
        char name[32];
        char longname[128];
        char description[256];
        
        snd_ctl_t* ctl;
        if (snd_ctl_open(&ctl, name, 0) == 0) {
            // Get card info
            snd_ctl_card_info_t* info;
            snd_ctl_card_info_alloca(&info);
            snd_ctl_card_info(ctl, info);
            
            DeviceInfo dev;
            dev.name = snd_ctl_card_info_get_name(info);
            dev.description = snd_ctl_card_info_get_longname(info);
            dev.device_path = "hw:" + std::to_string(card);
            dev.type = AudioInterfaceType::ALSA;
            dev.is_input = true;
            dev.is_output = true;
            
            // Add supported rates (simplified)
            dev.supported_sample_rates = {44100, 48000, 96000, 192000};
            dev.supported_channels = {1, 2, 4, 6, 8};
            dev.max_channels = 8;
            
            devices.push_back(dev);
            snd_ctl_close(ctl);
        }
        
        snd_card_next(&card);
    }
    
    return devices;
}

std::vector<AudioInterface::DeviceInfo> AudioInterface::get_i2s_devices() const {
    std::vector<DeviceInfo> devices;
    
    // Check for common I2S devices on Raspberry Pi
    std::vector<std::string> i2s_devices = {
        "snd_rpi_hifiberry_dac",
        "snd_rpi_hifiberry_amp",
        "snd_rpi_googlevoicehat_codec",
        "snd_rpi_justboom_dac",
        "snd_rpi_audioinjector_ultra"
    };
    
    for (const auto& device : i2s_devices) {
        DeviceInfo dev;
        dev.name = device;
        dev.description = "I2S Audio HAT";
        dev.device_path = "hw:" + device;
        dev.type = AudioInterfaceType::I2S;
        dev.is_input = true;
        dev.is_output = true;
        dev.supported_sample_rates = {44100, 48000, 96000, 192000};
        dev.supported_channels = {1, 2, 4};
        dev.max_channels = 4;
        devices.push_back(dev);
    }
    
    return devices;
}

std::string AudioInterface::get_default_input_device() const {
    return "default";
}

std::string AudioInterface::get_default_output_device() const {
    return "default";
}

bool AudioInterface::set_device(const std::string& device_name) {
    if (initialized) {
        return false;
    }
    config.device = device_name;
    return true;
}

// ============================================
// THREAD UTILITIES
// ============================================

void AudioInterface::set_thread_priority(int priority) {
    #ifdef __linux__
    struct sched_param param;
    param.sched_priority = 0;
    sched_setscheduler(0, SCHED_OTHER, &param);
    
    // Set nice value
    if (priority != 0) {
        nice(priority);
    }
    #endif
}

void AudioInterface::set_thread_affinity(int cpu) {
    #ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    #endif
}
