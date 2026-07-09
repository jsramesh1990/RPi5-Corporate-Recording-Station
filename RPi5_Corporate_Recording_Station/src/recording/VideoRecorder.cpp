#include "VideoRecorder.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

VideoRecorder::VideoRecorder() 
    : state(RecorderState::IDLE), 
      initialized(false),
      encoding(false),
      paused(false),
      av_format_ctx(nullptr),
      av_codec_ctx(nullptr),
      av_sws_ctx(nullptr),
      av_frame(nullptr),
      av_packet(nullptr) {
}

VideoRecorder::~VideoRecorder() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool VideoRecorder::initialize(const VideoConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        shutdown();
    }
    
    config = cfg;
    
    // Initialize FFmpeg
    if (!initFFmpeg()) {
        notifyError("Failed to initialize FFmpeg");
        return false;
    }
    
    initialized = true;
    state = RecorderState::IDLE;
    resetStats();
    
    std::cout << "Video Recorder initialized: " << config.width << "x" << config.height 
              << " @ " << config.framerate << "fps, " 
              << getCodecName(config.codec) << std::endl;
    
    return true;
}

bool VideoRecorder::initialize(int width, int height, int framerate,
                              int bitrate, Codec codec) {
    VideoConfig cfg;
    cfg.width = width;
    cfg.height = height;
    cfg.framerate = framerate;
    cfg.bitrate = bitrate;
    cfg.codec = codec;
    return initialize(cfg);
}

void VideoRecorder::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    // Stop recording if active
    if (state == RecorderState::RECORDING || state == RecorderState::PAUSED) {
        stopRecording();
    }
    
    // Stop encoding thread
    if (encoding) {
        encoding = false;
        if (encode_thread.joinable()) {
            encode_thread.join();
        }
    }
    
    // Cleanup FFmpeg
    cleanupFFmpeg();
    
    initialized = false;
    state = RecorderState::IDLE;
    
    std::cout << "Video Recorder shutdown" << std::endl;
}

// ============================================
// RECORDING CONTROL
// ============================================

bool VideoRecorder::startRecording(const std::string& filename, bool overwrite) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        notifyError("Not initialized");
        return false;
    }
    
    if (state == RecorderState::RECORDING) {
        notifyError("Already recording");
        return false;
    }
    
    // Check if file exists
    std::ifstream check(filename);
    if (check.good() && !overwrite) {
        notifyError("File already exists: " + filename);
        return false;
    }
    check.close();
    
    // Open output
    if (!openOutput(filename)) {
        notifyError("Failed to open output: " + filename);
        return false;
    }
    
    // Write header
    if (!writeHeader()) {
        notifyError("Failed to write header");
        return false;
    }
    
    current_file = filename;
    state = RecorderState::RECORDING;
    paused = false;
    
    // Start encoding thread
    if (!encoding) {
        encoding = true;
        encode_thread = std::thread(&VideoRecorder::encodeLoop, this);
    }
    
    // Reset stats
    resetStats();
    stats.start_time = std::chrono::system_clock::now();
    stats.current_file = filename;
    stats.is_recording = true;
    
    notifyStateChange();
    notifyProgress(0.0f);
    
    std::cout << "Video recording started: " << filename << std::endl;
    return true;
}

std::string VideoRecorder::stopRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state != RecorderState::RECORDING && state != RecorderState::PAUSED) {
        return "";
    }
    
    state = RecorderState::STOPPING;
    
    // Flush encoder
    flushEncoder();
    
    // Write trailer
    writeTrailer();
    
    // Cleanup output
    if (output_file.is_open()) {
        output_file.close();
    }
    
    std::string filename = current_file;
    current_file.clear();
    state = RecorderState::IDLE;
    stats.is_recording = false;
    
    notifyStateChange();
    notifyProgress(100.0f);
    
    std::cout << "Video recording stopped: " << filename << std::endl;
    return filename;
}

void VideoRecorder::pauseRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == RecorderState::RECORDING) {
        paused = true;
        state = RecorderState::PAUSED;
        notifyStateChange();
        std::cout << "Video recording paused" << std::endl;
    }
}

void VideoRecorder::resumeRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == RecorderState::PAUSED) {
        paused = false;
        state = RecorderState::RECORDING;
        notifyStateChange();
        std::cout << "Video recording resumed" << std::endl;
    }
}

std::string VideoRecorder::getStateName() const {
    switch (state) {
        case RecorderState::IDLE: return "IDLE";
        case RecorderState::INITIALIZING: return "INITIALIZING";
        case RecorderState::RECORDING: return "RECORDING";
        case RecorderState::PAUSED: return "PAUSED";
        case RecorderState::STOPPING: return "STOPPING";
        case RecorderState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ============================================
// FRAME PROCESSING
// ============================================

bool VideoRecorder::writeFrame(const VideoFrame& frame) {
    if (state != RecorderState::RECORDING && state != RecorderState::PAUSED) {
        return false;
    }
    
    if (paused) {
        return false;
    }
    
    // Queue frame for encoding
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (frame_queue.size() < max_queue_size) {
        frame_queue.push(frame);
        return true;
    }
    
    stats.frames_dropped++;
    return false;
}

bool VideoRecorder::writeFrame(const std::vector<uint8_t>& data, int width, int height,
                              const std::string& format) {
    VideoFrame frame;
    frame.data = data;
    frame.width = width;
    frame.height = height;
    frame.pts = getCurrentTimeMicros();
    return writeFrame(frame);
}

bool VideoRecorder::writeRGBFrame(const std::vector<uint8_t>& data, int width, int height) {
    return writeFrame(data, width, height, "rgb24");
}

bool VideoRecorder::writeYUVFrame(const std::vector<uint8_t>& y,
                                 const std::vector<uint8_t>& u,
                                 const std::vector<uint8_t>& v,
                                 int width, int height) {
    // Combine YUV planes into single buffer
    size_t y_size = width * height;
    size_t uv_size = width * height / 4;
    std::vector<uint8_t> data(y_size + uv_size * 2);
    
    memcpy(data.data(), y.data(), y_size);
    memcpy(data.data() + y_size, u.data(), uv_size);
    memcpy(data.data() + y_size + uv_size, v.data(), uv_size);
    
    return writeFrame(data, width, height, "yuv420p");
}

bool VideoRecorder::encodeFrame(const std::vector<uint8_t>& data, int64_t timestamp) {
    VideoFrame frame;
    frame.data = data;
    frame.pts = timestamp;
    return writeFrame(frame);
}

// ============================================
// FFMPEG FUNCTIONS
// ============================================

bool VideoRecorder::initFFmpeg() {
    av_register_all();
    avformat_network_init();
    return true;
}

void VideoRecorder::cleanupFFmpeg() {
    if (av_frame) {
        av_frame_free((AVFrame**)&av_frame);
        av_frame = nullptr;
    }
    if (av_packet) {
        av_packet_free((AVPacket**)&av_packet);
        av_packet = nullptr;
    }
    if (av_sws_ctx) {
        sws_freeContext((SwsContext*)av_sws_ctx);
        av_sws_ctx = nullptr;
    }
    if (av_codec_ctx) {
        avcodec_free_context((AVCodecContext**)&av_codec_ctx);
        av_codec_ctx = nullptr;
    }
    if (av_format_ctx) {
        avformat_free_context((AVFormatContext*)av_format_ctx);
        av_format_ctx = nullptr;
    }
}

bool VideoRecorder::openOutput(const std::string& filename) {
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVCodec* codec = nullptr;
    AVStream* stream = nullptr;
    int ret;
    
    // Allocate format context
    ret = avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, filename.c_str());
    if (ret < 0) {
        return false;
    }
    
    // Find encoder
    codec = avcodec_find_encoder_by_name(getCodecString(config.codec).c_str());
    if (!codec) {
        avformat_free_context(fmt_ctx);
        return false;
    }
    
    // Create stream
    stream = avformat_new_stream(fmt_ctx, codec);
    if (!stream) {
        avformat_free_context(fmt_ctx);
        return false;
    }
    stream->id = fmt_ctx->nb_streams - 1;
    
    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        avformat_free_context(fmt_ctx);
        return false;
    }
    
    // Set codec parameters
    codec_ctx->width = config.width;
    codec_ctx->height = config.height;
    codec_ctx->time_base = {1, config.framerate};
    codec_ctx->framerate = {config.framerate, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = config.bitrate;
    codec_ctx->gop_size = config.gop_size;
    codec_ctx->max_b_frames = 2;
    
    // Set private options
    av_opt_set(codec_ctx->priv_data, "preset", config.preset.c_str(), 0);
    av_opt_set(codec_ctx->priv_data, "profile", config.profile.c_str(), 0);
    av_opt_set(codec_ctx->priv_data, "level", config.level.c_str(), 0);
    
    // Open codec
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return false;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return false;
    }
    stream->time_base = codec_ctx->time_base;
    
    // Open output file
    ret = avio_open(&fmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return false;
    }
    
    av_format_ctx = fmt_ctx;
    av_codec_ctx = codec_ctx;
    
    // Allocate frame
    av_frame = av_frame_alloc();
    av_packet = av_packet_alloc();
    
    return true;
}

bool VideoRecorder::writeHeader() {
    AVFormatContext* fmt_ctx = (AVFormatContext*)av_format_ctx;
    int ret = avformat_write_header(fmt_ctx, nullptr);
    return ret >= 0;
}

bool VideoRecorder::writeTrailer() {
    AVFormatContext* fmt_ctx = (AVFormatContext*)av_format_ctx;
    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
        avio_close(fmt_ctx->pb);
    }
    return true;
}

bool VideoRecorder::encodeVideoFrame(const VideoFrame& frame) {
    AVCodecContext* codec_ctx = (AVCodecContext*)av_codec_ctx;
    AVFrame* avframe = (AVFrame*)av_frame;
    AVPacket* packet = (AVPacket*)av_packet;
    int ret;
    
    // Convert frame if needed
    if (!convertFrame(frame, (void*&)avframe)) {
        return false;
    }
    
    // Set PTS
    avframe->pts = frame.pts / 1000;  // Convert to milliseconds
    
    // Encode frame
    ret = avcodec_send_frame(codec_ctx, avframe);
    if (ret < 0) {
        return false;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            return false;
        }
        
        // Write packet
        av_interleaved_write_frame((AVFormatContext*)av_format_ctx, packet);
        av_packet_unref(packet);
    }
    
    return true;
}

bool VideoRecorder::flushEncoder() {
    AVCodecContext* codec_ctx = (AVCodecContext*)av_codec_ctx;
    AVPacket* packet = (AVPacket*)av_packet;
    int ret;
    
    ret = avcodec_send_frame(codec_ctx, nullptr);
    if (ret < 0) {
        return false;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            return false;
        }
        
        av_interleaved_write_frame((AVFormatContext*)av_format_ctx, packet);
        av_packet_unref(packet);
    }
    
    return true;
}

bool VideoRecorder::convertFrame(const VideoFrame& frame, void*& out_frame) {
    AVFrame* avframe = (AVFrame*)av_frame;
    
    // Set frame properties
    avframe->width = config.width;
    avframe->height = config.height;
    avframe->format = AV_PIX_FMT_YUV420P;
    
    // Allocate buffer
    int ret = av_image_alloc(avframe->data, avframe->linesize, 
                            config.width, config.height,
                            AV_PIX_FMT_YUV420P, 1);
    if (ret < 0) {
        return false;
    }
    
    // Copy data (simplified - would need proper conversion)
    size_t y_size = config.width * config.height;
    size_t uv_size = y_size / 4;
    
    if (frame.data.size() >= y_size + uv_size * 2) {
        memcpy(avframe->data[0], frame.data.data(), y_size);
        memcpy(avframe->data[1], frame.data.data() + y_size, uv_size);
        memcpy(avframe->data[2], frame.data.data() + y_size + uv_size, uv_size);
    }
    
    out_frame = avframe;
    return true;
}

// ============================================
// ENCODING THREAD
// ============================================

void VideoRecorder::encodeLoop() {
    while (encoding) {
        VideoFrame frame;
        bool has_frame = false;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!frame_queue.empty()) {
                frame = frame_queue.front();
                frame_queue.pop();
                has_frame = true;
            }
        }
        
        if (has_frame) {
            processFrame(frame);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void VideoRecorder::processFrame(const VideoFrame& frame) {
    if (state != RecorderState::RECORDING) {
        return;
    }
    
    // Encode frame
    if (encodeVideoFrame(frame)) {
        updateStats(frame);
        notifyStats();
        
        // Calculate progress
        if (stats.duration_seconds > 0) {
            float progress = std::min(100.0f, (float)(stats.duration_seconds / 3600.0f) * 100.0f);
            notifyProgress(progress);
        }
        
        // Callback
        if (on_frame) {
            on_frame(frame);
        }
    }
}

// ============================================
// STATISTICS
// ============================================

VideoRecorder::RecordingStats VideoRecorder::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void VideoRecorder::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = RecordingStats{};
}

void VideoRecorder::updateStats(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.frames_recorded++;
    stats.bytes_written += frame.data.size();
    
    auto now = std::chrono::system_clock::now();
    stats.last_frame_time = now;
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time).count();
    if (elapsed > 0) {
        stats.duration_seconds = elapsed;
        stats.fps_actual = static_cast<double>(stats.frames_recorded) / elapsed;
        stats.bitrate_actual = (stats.bytes_written * 8.0) / elapsed;
    }
}

void VideoRecorder::notifyStats() {
    if (on_stats) {
        on_stats(getStats());
    }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void VideoRecorder::setState(RecorderState new_state) {
    if (state != new_state) {
        state = new_state;
        notifyStateChange();
    }
}

void VideoRecorder::notifyStateChange() {
    if (on_state_change) {
        on_state_change(state);
    }
}

void VideoRecorder::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "Video Recorder Error: " << error << std::endl;
}

void VideoRecorder::notifyProgress(float progress) {
    if (on_progress) {
        on_progress(progress);
    }
}

// ============================================
// CALLBACKS
// ============================================

void VideoRecorder::setOnFrame(FrameCallback callback) {
    on_frame = callback;
}

void VideoRecorder::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

void VideoRecorder::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void VideoRecorder::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void VideoRecorder::setOnProgress(ProgressCallback callback) {
    on_progress = callback;
}

// ============================================
// CONFIGURATION
// ============================================

bool VideoRecorder::setConfig(const VideoConfig& cfg) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config = cfg;
    return true;
}

bool VideoRecorder::setResolution(int width, int height) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.width = width;
    config.height = height;
    return true;
}

bool VideoRecorder::setFramerate(int framerate) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.framerate = framerate;
    return true;
}

bool VideoRecorder::setBitrate(int bitrate) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.bitrate = bitrate;
    return true;
}

bool VideoRecorder::setCodec(Codec codec) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.codec = codec;
    return true;
}

bool VideoRecorder::setQuality(Quality quality) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.quality = quality;
    return true;
}

// ============================================
// UTILITY
// ============================================

std::string VideoRecorder::getCodecName(Codec codec) const {
    switch (codec) {
        case Codec::H264: return "H.264";
        case Codec::H265: return "H.265/HEVC";
        case Codec::MJPEG: return "MJPEG";
        case Codec::VP8: return "VP8";
        case Codec::VP9: return "VP9";
        case Codec::AV1: return "AV1";
        default: return "Unknown";
    }
}

std::string VideoRecorder::getContainerName(Container container) const {
    switch (container) {
        case Container::MP4: return "MP4";
        case Container::MKV: return "Matroska";
        case Container::AVI: return "AVI";
        case Container::MOV: return "QuickTime";
        case Container::TS: return "MPEG-TS";
        case Container::FLV: return "FLV";
        default: return "Unknown";
    }
}

std::string VideoRecorder::getQualityName(Quality quality) const {
    switch (quality) {
        case Quality::LOW: return "Low";
        case Quality::MEDIUM: return "Medium";
        case Quality::HIGH: return "High";
        case Quality::ULTRA: return "Ultra";
        default: return "Unknown";
    }
}

std::string VideoRecorder::getFileExtension(Container container) const {
    switch (container) {
        case Container::MP4: return ".mp4";
        case Container::MKV: return ".mkv";
        case Container::AVI: return ".avi";
        case Container::MOV: return ".mov";
        case Container::TS: return ".ts";
        case Container::FLV: return ".flv";
        default: return ".mp4";
    }
}

std::string VideoRecorder::generateFilename(const std::string& prefix) const {
    std::string timestamp = getTimestamp();
    std::string ext = getFileExtension(config.container);
    return prefix + "_" + timestamp + ext;
}

bool VideoRecorder::testRecording(int duration_seconds) {
    std::cout << "Testing video recording for " << duration_seconds << " seconds..." << std::endl;
    
    std::string filename = generateFilename("test");
    if (!startRecording(filename)) {
        return false;
    }
    
    // Generate test frames
    int frame_count = duration_seconds * config.framerate;
    for (int i = 0; i < frame_count; i++) {
        // Create a test pattern frame
        std::vector<uint8_t> frame_data(config.width * config.height * 3 / 2);
        // Fill with gradient pattern
        for (int y = 0; y < config.height; y++) {
            for (int x = 0; x < config.width; x++) {
                int idx = y * config.width + x;
                frame_data[idx] = (x * 255) / config.width;  // Y
                frame_data[config.width * config.height + idx / 2] = (y * 255) / config.height;  // U/V
            }
        }
        
        writeFrame(frame_data, config.width, config.height, "yuv420p");
        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / config.framerate));
    }
    
    stopRecording();
    std::cout << "Test recording complete: " << filename << std::endl;
    return true;
}

std::string VideoRecorder::getCodecString(Codec codec) const {
    switch (codec) {
        case Codec::H264: return "libx264";
        case Codec::H265: return "libx265";
        case Codec::MJPEG: return "mjpeg";
        case Codec::VP8: return "libvpx";
        case Codec::VP9: return "libvpx-vp9";
        case Codec::AV1: return "libaom-av1";
        default: return "libx264";
    }
}

std::string VideoRecorder::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", tm_info);
    return std::string(buffer);
}

uint64_t VideoRecorder::getCurrentTimeMicros() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void VideoRecorder::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}
