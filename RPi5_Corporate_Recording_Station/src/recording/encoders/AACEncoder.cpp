#include "AACEncoder.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cmath>

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

const std::map<AACEncoder::Profile, std::string> AACEncoder::profile_map = {
    {Profile::AAC_LC, "aac_low"},
    {Profile::HE_AAC, "aac_he"},
    {Profile::HE_AAC_V2, "aac_he_v2"},
    {Profile::AAC_LD, "aac_ld"},
    {Profile::AAC_ELD, "aac_eld"}
};

AACEncoder::AACEncoder() 
    : state(EncoderState::IDLE),
      initialized(false),
      av_codec_ctx(nullptr),
      av_frame(nullptr),
      av_packet(nullptr) {
}

AACEncoder::~AACEncoder() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool AACEncoder::initialize(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        shutdown();
    }
    
    config = cfg;
    
    // Initialize FFmpeg encoder
    if (!initFFmpegEncoder()) {
        notifyError("Failed to initialize FFmpeg encoder");
        return false;
    }
    
    initialized = true;
    state = EncoderState::INITIALIZED;
    resetStats();
    
    std::cout << "AAC Encoder initialized: " << config.sample_rate << "Hz, "
              << config.channels << "ch, " << config.bitrate/1000 << " Kbps"
              << std::endl;
    
    return true;
}

bool AACEncoder::initialize(int sample_rate, int channels, int bitrate) {
    Config cfg;
    cfg.sample_rate = sample_rate;
    cfg.channels = channels;
    cfg.bitrate = bitrate;
    return initialize(cfg);
}

void AACEncoder::shutdown() {
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
    
    std::cout << "AAC Encoder shutdown" << std::endl;
}

// ============================================
// ENCODING
// ============================================

bool AACEncoder::encodeAudio(const std::vector<int16_t>& samples, int64_t pts) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || state == EncoderState::ERROR) {
        return false;
    }
    
    // Convert samples to audio frame
    void* frame = nullptr;
    if (!convertToAudioFrame(samples, frame)) {
        return false;
    }
    
    // Set frame properties
    AVFrame* avframe = (AVFrame*)frame;
    avframe->pts = pts > 0 ? pts : getCurrentTimeMicros() / 1000;
    
    // Send frame to encoder
    if (!sendFrame(frame)) {
        return false;
    }
    
    // Receive encoded packets
    return receivePacket();
}

bool AACEncoder::encodeAudioFloat(const std::vector<float>& samples, int64_t pts) {
    // Convert float to int16
    std::vector<int16_t> int_samples(samples.size());
    for (size_t i = 0; i < samples.size(); i++) {
        float val = samples[i] * 32767.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        int_samples[i] = static_cast<int16_t>(val);
    }
    return encodeAudio(int_samples, pts);
}

bool AACEncoder::flush() {
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

bool AACEncoder::initFFmpegEncoder() {
    AVCodec* codec = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    int ret;
    
    // Find encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        codec = avcodec_find_encoder_by_name("libfdk_aac");
    }
    if (!codec) {
        codec = avcodec_find_encoder_by_name("aac");
    }
    
    if (!codec) {
        notifyError("Failed to find AAC encoder");
        return false;
    }
    
    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        notifyError("Failed to allocate codec context");
        return false;
    }
    
    // Set codec parameters
    codec_ctx->sample_rate = config.sample_rate;
    codec_ctx->channels = config.channels;
    codec_ctx->channel_layout = getChannelLayout();
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_ctx->bit_rate = config.bitrate;
    codec_ctx->time_base = {1, config.sample_rate};
    codec_ctx->frame_size = config.frame_size;
    
    // Set private options
    av_opt_set(codec_ctx->priv_data, "profile", getProfileString(config.profile).c_str(), 0);
    
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
    frame->nb_samples = config.frame_size;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->sample_rate = config.sample_rate;
    frame->channel_layout = getChannelLayout();
    frame->channels = config.channels;
    
    av_frame_get_buffer(frame, 0);
    
    av_codec_ctx = codec_ctx;
    
    return true;
}

void AACEncoder::cleanupFFmpegEncoder() {
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

bool AACEncoder::sendFrame(void* frame) {
    AVCodecContext* codec_ctx = (AVCodecContext*)av_codec_ctx;
    int ret = avcodec_send_frame(codec_ctx, (AVFrame*)frame);
    
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        notifyError("Failed to send frame to encoder");
        return false;
    }
    
    return true;
}

bool AACEncoder::receivePacket() {
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
    frame.duration = packet->duration;
    frame.sample_rate = config.sample_rate;
    frame.channels = config.channels;
    frame.bitrate = config.bitrate;
    frame.size_bytes = packet->size;
    frame.is_keyframe = true;
    
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

bool AACEncoder::convertToAudioFrame(const std::vector<int16_t>& samples, void*& out_frame) {
    AVFrame* frame = (AVFrame*)av_frame;
    
    if (samples.empty()) {
        out_frame = nullptr;
        return true;
    }
    
    // Ensure frame has enough space
    if (frame->nb_samples != (int)samples.size() / config.channels) {
        frame->nb_samples = samples.size() / config.channels;
        av_frame_get_buffer(frame, 0);
    }
    
    // Copy data
    memcpy(frame->data[0], samples.data(), samples.size() * sizeof(int16_t));
    
    out_frame = frame;
    return true;
}

bool AACEncoder::convertToAudioFrame(const std::vector<float>& samples, void*& out_frame) {
    std::vector<int16_t> int_samples(samples.size());
    for (size_t i = 0; i < samples.size(); i++) {
        float val = samples[i] * 32767.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        int_samples[i] = static_cast<int16_t>(val);
    }
    return convertToAudioFrame(int_samples, out_frame);
}

// ============================================
// STATISTICS
// ============================================

AACEncoder::Stats AACEncoder::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void AACEncoder::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = Stats{};
    stats.start_time = std::chrono::system_clock::now();
    stats.last_frame_time = stats.start_time;
}

void AACEncoder::updateStats(const EncodedFrame& frame) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.frames_encoded++;
    stats.samples_encoded += config.frame_size * config.channels;
    stats.bytes_encoded += frame.size_bytes;
    stats.last_frame_time = std::chrono::system_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        stats.last_frame_time - stats.start_time).count();
    if (elapsed > 0) {
        stats.bitrate_actual = (stats.bytes_encoded * 8.0) / elapsed;
    }
}

void AACEncoder::notifyStats() {
    if (on_stats) {
        on_stats(getStats());
    }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void AACEncoder::setState(EncoderState new_state) {
    if (state != new_state) {
        state = new_state;
        notifyStateChange();
    }
}

void AACEncoder::notifyStateChange() {
    if (on_state_change) {
        on_state_change(state);
    }
}

void AACEncoder::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "AAC Encoder Error: " << error << std::endl;
}

std::string AACEncoder::getStateName() const {
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

bool AACEncoder::setBitrate(int bitrate) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.bitrate = bitrate;
    return true;
}

bool AACEncoder::setSampleRate(int sample_rate) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.sample_rate = sample_rate;
    return true;
}

bool AACEncoder::setChannels(int channels) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.channels = channels;
    return true;
}

bool AACEncoder::setProfile(Profile profile) {
    if (state == EncoderState::ENCODING) {
        return false;
    }
    config.profile = profile;
    return true;
}

// ============================================
// CALLBACKS
// ============================================

void AACEncoder::setOnFrame(FrameCallback callback) {
    on_frame = callback;
}

void AACEncoder::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

void AACEncoder::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void AACEncoder::setOnError(ErrorCallback callback) {
    on_error = callback;
}

// ============================================
// UTILITY
// ============================================

std::string AACEncoder::getProfileName(Profile profile) const {
    auto it = profile_map.find(profile);
    return it != profile_map.end() ? it->second : "aac_low";
}

std::string AACEncoder::getProfileString(Profile profile) const {
    return getProfileName(profile);
}

int AACEncoder::getSampleFormat() const {
    return AV_SAMPLE_FMT_S16;
}

int AACEncoder::getChannelLayout() const {
    return config.channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
}

bool AACEncoder::testEncoder(int duration_seconds) {
    std::cout << "Testing AAC encoder for " << duration_seconds << " seconds..." << std::endl;
    
    // Generate test audio
    int samples_per_second = config.sample_rate;
    int total_samples = duration_seconds * samples_per_second * config.channels;
    std::vector<int16_t> buffer(samples_per_second * config.channels);
    
    for (int i = 0; i < duration_seconds; i++) {
        // Generate a 440Hz tone
        for (int j = 0; j < samples_per_second; j++) {
            float t = static_cast<float>(j) / config.sample_rate;
            float sample = 16384.0f * sinf(2.0f * M_PI * 440.0f * t);
            int16_t val = static_cast<int16_t>(sample);
            
            for (int c = 0; c < config.channels; c++) {
                buffer[j * config.channels + c] = val;
            }
        }
        encodeAudio(buffer, i * 1000000);
        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / 10));
    }
    
    flush();
    std::cout << "Test encoding complete" << std::endl;
    return true;
}

void AACEncoder::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

uint64_t AACEncoder::getCurrentTimeMicros() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
