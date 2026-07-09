#include "H264Encoder.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>
}

// Preset mapping
const std::map<H264Encoder::Preset, std::string> H264Encoder::preset_map = {
    {Preset::ULTRAFAST, "ultrafast"},
    {Preset::SUPERFAST, "superfast"},
    {Preset::VERYFAST, "veryfast"},
    {Preset::FASTER, "faster"},
    {Preset::FAST, "fast"},
    {Preset::MEDIUM, "medium"},
    {Preset::SLOW, "slow"},
    {Preset::SLOWER, "slower"},
    {Preset::VERYSLOW, "veryslow"},
    {Preset::PLACEBO, "placebo"}
};

const std::map<H264Encoder::Profile, std::string> H264Encoder::profile_map = {
    {Profile::BASELINE, "baseline"},
    {Profile::MAIN, "main"},
    {Profile::HIGH, "high"},
    {Profile::HIGH_10, "high10"},
    {Profile::HIGH_422, "high422"},
    {Profile::HIGH_444, "high444"}
};

const std::map<H264Encoder::Level, std::string> H264Encoder::level_map = {
    {Level::LEVEL_1_0, "1.0"},
    {Level::LEVEL_1_1, "1.1"},
    {Level::LEVEL_1_2, "1.2"},
    {Level::LEVEL_1_3, "1.3"},
    {Level::LEVEL_2_0, "2.0"},
    {Level::LEVEL_2_1, "2.1"},
    {Level::LEVEL_2_2, "2.2"},
    {Level::LEVEL_3_0, "3.0"},
    {Level::LEVEL_3_1, "3.1"},
    {Level::LEVEL_3_2, "3.2"},
    {Level::LEVEL_4_0, "4.0"},
    {Level::LEVEL_4_1, "4.1"},
    {Level::LEVEL_4_2, "4.2"},
    {Level::LEVEL_5_0, "5.0"},
    {Level::LEVEL_5_1, "5.1"},
    {Level::LEVEL_5_2, "5.2"}
};

H264Encoder::H264Encoder() 
    : state(EncoderState::IDLE),
      initialized(false),
      force_keyframe(false),
      av_codec_ctx(nullptr),
      av_frame(nullptr),
      av_packet(nullptr),
      av_sws_ctx(nullptr),
      av_hw_ctx(nullptr),
      hw_accel_available(false) {
}

H264Encoder::~H264Encoder() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool H264Encoder::initialize(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        shutdown();
    }
    
    config = cfg;
    
    // Check hardware acceleration support
    if (config.hardware_acceleration) {
        hw_accel_available = checkHardwareSupport();
        if (!hw_accel_available) {
            std::cout << "Hardware acceleration not available, using software encoding" << std::endl;
        }
    }
    
    // Initialize FFmpeg encoder
    if (!initFFmpegEncoder()) {
        notifyError("Failed to initialize FFmpeg encoder");
        return false;
    }
    
    initialized = true;
    state = EncoderState::INITIALIZED;
    resetStats();
    
    std::cout << "H.264 Encoder initialized: " << config.width << "x" << config.height
              << " @ " << config.framerate << "fps, " << config.bitrate/1000000 << " Mbps"
              << (hw_accel_available ? " (HW)" : "") << std::endl;
    
    return true;
}

bool H264Encoder::initialize(int width, int height, int framerate, int bitrate) {
    Config cfg;
    cfg.width = width;
    cfg.height = height;
    cfg.framerate = framerate;
    cfg.bitrate = bitrate;
    return initialize(cfg);
}

void H264Encoder::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    // Flush encoder
    flush();
    
    // Cleanup
    cleanupFFmpegEncoder();
    
    initialized = false;
    state = EncoderState::IDLE;
    
    std::cout << "H.264 Encoder shutdown" << std::endl;
}

// ============================================
// ENCODING
// ============================================

bool H264Encoder::encodeFrame(const std::vector<uint8_t>& data, int64_t pts, bool is_keyframe) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || state == EncoderState::ERROR) {
        return false;
    }
    
    if (is_keyframe || force_keyframe) {
        force_keyframe = false;
    }
    
    // Convert data to YUV frame
    void* frame = nullptr;
    if (!convertToYUV(data, config.width, config.height, "yuv420p", frame)) {
        return false;
    }
    
    // Set frame properties
    AVFrame* avframe = (AVFrame*)frame;
    avframe->pts = pts > 0 ? pts : getCurrentTimeMicros() / 1000;
    
    if (is_keyframe || force_keyframe) {
        avframe->key_frame = 1;
        avframe->pict_type = AV_PICTURE_TYPE_I;
        force_keyframe = false;
    }
    
    // Send frame to encoder
    if (!sendFrame(frame)) {
        return false;
    }
    
    // Receive encoded packets
    return receivePacket();
}

bool H264Encoder::encodeFrame(const std::vector<uint8_t>& y,
                             const std::vector<uint8_t>& u,
                             const std::vector<uint8_t>& v,
                             int64_t pts) {
    // Combine YUV planes into single buffer
    size_t y_size = config.width * config.height;
    size_t uv_size = config.width * config.height / 4;
    std::vector<uint8_t> data(y_size + uv_size * 2);
    
    memcpy(data.data(), y.data(), y_size);
    memcpy(data.data() + y_size, u.data(), uv_size);
    memcpy(data.data() + y_size + uv_size, v.data(), uv_size);
    
    return encodeFrame(data, pts);
}

bool H264Encoder::encodeRGB(const std::vector<uint8_t>& data, int width, int height, int64_t pts) {
    std::vector<uint8_t> yuv;
    if (!convertRGBToYUV(data, width, height, yuv)) {
        return false;
    }
    return encodeFrame(yuv, pts);
}

bool H264Encoder::flush() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return false;
    }
    
    state = EncoderState::FLUSHING;
    
    // Send null frame to flush encoder
    if (!sendFrame(nullptr)) {
        return false;
    }
    
    // Receive remaining packets
    while (receivePacket()) {
        // Continue receiving packets
    }
    
    state = EncoderState::INITIALIZED;
    return true;
}

// ============================================
// FFMPEG FUNCTIONS
// ============================================

bool H264Encoder::initFFmpegEncoder() {
    AVCodec* codec = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    int ret;
    
    // Find encoder
    if (hw_accel_available && config.hardware_acceleration) {
        // Try hardware encoder first
        codec = avcodec_find_encoder_by_name("h264_v4l2m2m");
        if (!codec) {
            codec = avcodec_find_encoder_by_name("h264_omx");
        }
        if (!codec) {
            codec = avcodec_find_encoder_by_name("h264_vaapi");
        }
        if (!codec) {
            // Fallback to software
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        }
    } else {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    
    if (!codec) {
        notifyError("Failed to find H.264 encoder");
        return false;
    }
    
    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        notifyError("Failed to allocate codec context");
        return false;
    }
    
    // Set codec parameters
    codec_ctx->width = config.width;
    codec_ctx->height = config.height;
    codec_ctx->time_base = {1, config.framerate};
    codec_ctx->framerate = {config.framerate, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = config.bitrate;
    codec_ctx->rc_max_rate = config.max_bitrate;
    codec_ctx->rc_buffer_size = config.buffer_size;
    codec_ctx->gop_size = config.gop_size;
    codec_ctx->max_b_frames = config.b_frames;
    codec_ctx->refs = config.ref_frames;
    codec_ctx->thread_count = config.threads == 0 ? av_cpu_count() : config.threads;
    
    // Set private options
    av_opt_set(codec_ctx->priv_data, "preset", getPresetString(config.preset).c_str(), 0);
    av_opt_set(codec_ctx->priv_data, "profile", getProfileString(config.profile).c_str(), 0);
    av_opt_set(codec_ctx->priv_data, "level", getLevelString(config.level).c_str(), 0);
    
    if (config.cbr) {
        av_opt_set(codec_ctx->priv_data, "ratecontrol", "cbr", 0);
    }
    
    // Open codec
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        avcodec_free_context(&codec_ctx);
        notifyError("Failed to open codec");
        return false;
    }
    
    // Allocate frame and packet
    av_frame = av_frame_alloc();
    av_packet = av_packet_alloc();
    
    // Setup frame
    AVFrame* frame = (AVFrame*)av_frame;
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = config.width;
    frame->height = config.height;
    
    av_frame_get_buffer(frame, 0);
    
    av_codec_ctx = codec_ctx;
    
    return true;
}

void H264Encoder::cleanupFFmpegEncoder() {
    if (av_sws_ctx) {
        sws_freeContext((SwsContext*)av_sws_ctx);
        av_sws_ctx = nullptr;
    }
    if (av_hw_ctx) {
        av_buffer_unref((AVBufferRef**)&av_hw_ctx);
        av_hw_ctx = nullptr;
    }
    if (av_frame) {
        av_frame_free((AVFrame**)&av_frame);
        av_frame = nullptr;
    }
    if (av_packet) {
        av_packet_free((AVPacket**)&av_packet);
        av_packet = nullptr;
    }
    if (av_codec_ctx) {
        avcodec_free_context((AVCodecContext**)&av_codec_ctx);
        av_codec_ctx = nullptr;
    }
}

bool H264Encoder::openEncoder() {
    // Already opened in init
    return true;
}

bool H264Encoder::closeEncoder() {
    // Cleanup handled in shutdown
    return true;
}

bool H264Encoder::sendFrame(void* frame) {
    AVCodecContext* codec_ctx = (AVCodecContext*)av_codec_ctx;
    int ret = avcodec_send_frame(codec_ctx, (AVFrame*)frame);
    
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        notifyError("Failed to send frame to encoder");
        return false;
    }
    
    return true;
}

bool H264Encoder::receivePacket() {
    AVCodecContext* codec_ctx = (AVCodecContext*)av_codec_ctx;
    AVPacket* packet = (AVPacket*)av_packet;
    int ret = avcodec_receive_packet(codec_ctx, packet);
    
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return false;
    }
    
    if (ret < 0) {
        notifyError("Failed to receive packet from encoder");
        return false;
    }
    
    // Create encoded frame
    EncodedFrame frame;
    frame.data.assign(packet->data, packet->data + packet->size);
    frame.pts = packet->pts;
    frame.dts = packet->dts;
    frame.duration = packet->duration;
    frame.is_keyframe = packet->flags & AV_PKT_FLAG_KEY;
    frame.size_bytes = packet->size;
    
    // Determine frame type
    if (packet->flags & AV_PKT_FLAG_KEY) {
        frame.frame_type = 0; // I-frame
    } else if (packet->flags & AV_PKT_FLAG_DISPOSABLE) {
        frame.frame_type = 2; // B-frame
    } else {
        frame.frame_type = 1; // P-frame
    }
    
    // Update statistics
    updateStats(frame);
    
    // Callback
    if (on_frame) {
        on_frame(frame);
    }
    
    // Add to queue
    std::lock_guard<std::mutex> queue_lock(queue_mutex);
    if (frame_queue.size() < max_queue_size) {
        frame_queue.push(frame);
    }
    
    av_packet_unref(packet);
    return true;
}

bool H264Encoder::setupHardwareAcceleration() {
    // Setup hardware acceleration context
    AVCodecContext* codec_ctx = (AVCodecContext*)av_codec_ctx;
    AVBufferRef* hw_ctx = nullptr;
    int ret;
    
    ret = av_hwdevice_ctx_create(&hw_ctx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
    if (ret < 0) {
        return false;
    }
    
    codec_ctx->hw_device_ctx = av_buffer_ref(hw_ctx);
    av_buffer_unref(&hw_ctx);
    
    return true;
}

bool H264Encoder::checkHardwareSupport() {
    // Check if hardware encoding is available
    AVCodec* codec = avcodec_find_encoder_by_name("h264_v4l2m2m");
    if (codec) return true;
    
    codec = avcodec_find_encoder_by_name("h264_omx");
    if (codec) return true;
    
    codec = avcodec_find_encoder_by_name("h264_vaapi");
    if (codec) return true;
    
    return false;
}

bool H264Encoder::convertToYUV(const std::vector<uint8_t>& data, int width, int height,
                              const std::string& format, void*& out_frame) {
    // Assume YUV420P format
    AVFrame* frame = (AVFrame*)av_frame;
    
    // Copy data to frame
    if (format == "yuv420p") {
        size_t y_size = width * height;
        size_t uv_size = y_size / 4;
        
        if (data.size() >= y_size + uv_size * 2) {
            memcpy(frame->data[0], data.data(), y_size);
            memcpy(frame->data[1], data.data() + y_size, uv_size);
            memcpy(frame->data[2], data.data() + y_size + uv_size, uv_size);
        }
    } else if (format == "rgb24") {
        // Convert RGB to YUV
        std::vector<uint8_t> yuv;
        convertRGBToYUV(data, width, height, yuv);
        return convertToYUV(yuv, width, height, "yuv420p", out_frame);
    }
    
    out_frame = frame;
    return true;
}

bool H264Encoder::convertRGBToYUV(const std::vector<uint8_t>& rgb, int width, int height,
                                 std::vector<uint8_t>& yuv) {
    // Simple RGB to YUV conversion (BT.709)
    size_t y_size = width * height;
    size_t uv_size = y_size / 4;
    yuv.resize(y_size + uv_size * 2);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            float r = rgb[idx] / 255.0f;
            float g = rgb[idx + 1] / 255.0f;
            float b = rgb[idx + 2] / 255.0f;
            
            // Y
            float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            yuv[y * width + x] = static_cast<uint8_t>(Y * 255.0f);
            
            // U and V (4:2:0 subsampling)
            if (x % 2 == 0 && y % 2 == 0) {
                int uv_idx = (y / 2) * (width / 2) + (x / 2);
                float U = 0.492f * (b - Y);
                float V = 0.877f * (r - Y);
                yuv[y_size + uv_idx] = static_cast<uint8_t>((U + 0.5f) * 255.0f);
                yuv[y_size + uv_size + uv_idx] = static_cast<uint8_t>((V + 0.5f) * 255.0f);
            }
        }
    }
    
    return true;
}

// ============================================
// STATISTICS
// ============================================

H264Encoder::Stats H264Encoder::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void H264Encoder::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = Stats{};
    stats.start_time = std::chrono::system_clock::now();
    stats.last_frame_time = stats.start_time;
}

void H264Encoder::updateStats(const EncodedFrame& frame) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.frames_encoded++;
    stats.bytes_encoded += frame.size_bytes;
    
    if (frame.is_keyframe) {
        stats.keyframes_encoded++;
    }
    
    auto now = std::chrono::system_clock::now();
    stats.last_frame_time = now;
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time).count();
    if (elapsed > 0) {
        stats.fps_actual = static_cast<double>(stats.frames_encoded) / elapsed;
        stats.bitrate_actual = (stats.bytes_encoded * 8.0) / elapsed;
        stats.avg_frame_size = static_cast<double>(stats.bytes_encoded) / stats.frames_encoded;
    }
}

void H264Encoder::notifyStats() {
    if (on_stats) {
        on_stats(getStats());
    }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void H264Encoder::setState(EncoderState new_state) {
    if (state != new_state) {
        state = new_state;
        notifyStateChange();
    }
}

void H264Encoder::notifyStateChange() {
    if (on_state_change) {
        on_state_change(state);
    }
}

void H264Encoder::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "H.264 Encoder Error: " << error << std::endl;
}

std::string H264Encoder::getStateName() const {
    switch (state) {
        case EncoderState::IDLE: return "IDLE";
        case EncoderState::INITIALIZED: return "INITIALIZED";
        case EncoderState::ENCODING: return "ENCODING";
        case EncoderState::FLUSHING: return "FLUSHING";
        case EncoderState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ============================================
// CONFIGURATION
// ============================================

bool H264Encoder::setBitrate(int bitrate) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.bitrate = bitrate;
    return true;
}

bool H264Encoder::setFramerate(int framerate) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.framerate = framerate;
    return true;
}

bool H264Encoder::setResolution(int width, int height) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.width = width;
    config.height = height;
    return true;
}

bool H264Encoder::setPreset(Preset preset) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.preset = preset;
    return true;
}

bool H264Encoder::setProfile(Profile profile) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.profile = profile;
    return true;
}

// ============================================
// CALLBACKS
// ============================================

void H264Encoder::setOnFrame(FrameCallback callback) {
    on_frame = callback;
}

void H264Encoder::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

void H264Encoder::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void H264Encoder::setOnError(ErrorCallback callback) {
    on_error = callback;
}

// ============================================
// UTILITY
// ============================================

std::string H264Encoder::getPresetName(Preset preset) const {
    auto it = preset_map.find(preset);
    return it != preset_map.end() ? it->second : "medium";
}

std::string H264Encoder::getProfileName(Profile profile) const {
    auto it = profile_map.find(profile);
    return it != profile_map.end() ? it->second : "high";
}

std::string H264Encoder::getLevelName(Level level) const {
    auto it = level_map.find(level);
    return it != level_map.end() ? it->second : "4.0";
}

std::string H264Encoder::getPresetString(Preset preset) const {
    return getPresetName(preset);
}

std::string H264Encoder::getProfileString(Profile profile) const {
    return getProfileName(profile);
}

std::string H264Encoder::getLevelString(Level level) const {
    return getLevelName(level);
}

bool H264Encoder::testEncoder(int duration_seconds) {
    std::cout << "Testing H.264 encoder for " << duration_seconds << " seconds..." << std::endl;
    
    // Generate test frames
    int frame_count = duration_seconds * config.framerate;
    std::vector<uint8_t> frame_data(config.width * config.height * 3 / 2);
    
    for (int i = 0; i < frame_count; i++) {
        // Generate test pattern
        for (int y = 0; y < config.height; y++) {
            for (int x = 0; x < config.width; x++) {
                int idx = y * config.width + x;
                // Moving gradient
                uint8_t val = (x + i * 2) % 256;
                frame_data[idx] = val;  // Y
                if (x % 2 == 0 && y % 2 == 0) {
                    int uv_idx = (y / 2) * (config.width / 2) + (x / 2);
                    int uv_size = config.width * config.height / 4;
                    frame_data[config.width * config.height + uv_idx] = 128 + (i % 64);
                    frame_data[config.width * config.height + uv_size + uv_idx] = 128 + (i % 64);
                }
            }
        }
        
        encodeFrame(frame_data, i * 1000000 / config.framerate);
        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / config.framerate));
    }
    
    flush();
    std::cout << "Test encoding complete" << std::endl;
    return true;
}

H264Encoder::Capabilities H264Encoder::getCapabilities() const {
    Capabilities caps;
    caps.supports_hardware = hw_accel_available;
    caps.supports_10bit = false;
    caps.supports_444 = false;
    caps.supports_lossless = false;
    caps.max_width = 4096;
    caps.max_height = 4096;
    caps.max_framerate = 120;
    caps.max_bitrate = 100000000;
    return caps;
}

void H264Encoder::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

uint64_t H264Encoder::getCurrentTimeMicros() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
