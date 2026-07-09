#include "AudioRecorder.hpp"

#include <iostream>
#include <fstream>
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
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/fifo.h>
#include <libavutil/samplefmt.h>
}

AudioRecorder::AudioRecorder() 
    : state(RecorderState::IDLE), 
      initialized(false),
      encoding(false),
      paused(false),
      av_format_ctx(nullptr),
      av_codec_ctx(nullptr),
      av_frame(nullptr),
      av_packet(nullptr),
      av_fifo(nullptr) {
}

AudioRecorder::~AudioRecorder() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool AudioRecorder::initialize(const AudioConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        shutdown();
    }
    
    config = cfg;
    
    // Initialize FFmpeg
    if (!initFFmpegAudio()) {
        notifyError("Failed to initialize FFmpeg");
        return false;
    }
    
    initialized = true;
    state = RecorderState::IDLE;
    resetStats();
    
    std::cout << "Audio Recorder initialized: " << config.sample_rate << "Hz, " 
              << config.channels << "ch, " << getCodecName(config.codec) << std::endl;
    
    return true;
}

bool AudioRecorder::initialize(int sample_rate, int channels, int bitrate, Codec codec) {
    AudioConfig cfg;
    cfg.sample_rate = sample_rate;
    cfg.channels = channels;
    cfg.bitrate = bitrate;
    cfg.codec = codec;
    return initialize(cfg);
}

void AudioRecorder::shutdown() {
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
    cleanupFFmpegAudio();
    
    initialized = false;
    state = RecorderState::IDLE;
    
    std::cout << "Audio Recorder shutdown" << std::endl;
}

// ============================================
// RECORDING CONTROL
// ============================================

bool AudioRecorder::startRecording(const std::string& filename, bool overwrite) {
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
    if (!openOutputAudio(filename)) {
        notifyError("Failed to open output: " + filename);
        return false;
    }
    
    // Write header
    if (!writeHeaderAudio()) {
        notifyError("Failed to write header");
        return false;
    }
    
    current_file = filename;
    state = RecorderState::RECORDING;
    paused = false;
    
    // Start encoding thread
    if (!encoding) {
        encoding = true;
        encode_thread = std::thread(&AudioRecorder::encodeLoopAudio, this);
    }
    
    // Reset stats
    resetStats();
    stats.start_time = std::chrono::system_clock::now();
    stats.current_file = filename;
    stats.is_recording = true;
    
    notifyStateChange();
    notifyProgress(0.0f);
    
    std::cout << "Audio recording started: " << filename << std::endl;
    return true;
}

std::string AudioRecorder::stopRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state != RecorderState::RECORDING && state != RecorderState::PAUSED) {
        return "";
    }
    
    state = RecorderState::STOPPING;
    
    // Flush encoder
    flushAudioEncoder();
    
    // Write trailer
    writeTrailerAudio();
    
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
    
    std::cout << "Audio recording stopped: " << filename << std::endl;
    return filename;
}

void AudioRecorder::pauseRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == RecorderState::RECORDING) {
        paused = true;
        state = RecorderState::PAUSED;
        notifyStateChange();
        std::cout << "Audio recording paused" << std::endl;
    }
}

void AudioRecorder::resumeRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == RecorderState::PAUSED) {
        paused = false;
        state = RecorderState::RECORDING;
        notifyStateChange();
        std::cout << "Audio recording resumed" << std::endl;
    }
}

std::string AudioRecorder::getStateName() const {
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
// AUDIO PROCESSING
// ============================================

bool AudioRecorder::writeAudio(const AudioData& data) {
    if (state != RecorderState::RECORDING && state != RecorderState::PAUSED) {
        return false;
    }
    
    if (paused) {
        return false;
    }
    
    // Queue audio for encoding
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (audio_queue.size() < max_queue_size) {
        audio_queue.push(data);
        return true;
    }
    
    stats.overruns++;
    return false;
}

bool AudioRecorder::writeAudio(const std::vector<int16_t>& samples) {
    AudioData data;
    data.samples = samples;
    data.sample_rate = config.sample_rate;
    data.channels = config.channels;
    data.bits_per_sample = config.bits_per_sample;
    data.pts = getCurrentTimeMicros();
    return writeAudio(data);
}

bool AudioRecorder::writeAudio(const int16_t* buffer, size_t frames) {
    size_t samples = frames * config.channels;
    std::vector<int16_t> data(buffer, buffer + samples);
    return writeAudio(data);
}

bool AudioRecorder::writeAudioFloat(const float* buffer, size_t frames) {
    size_t samples = frames * config.channels;
    std::vector<int16_t> int_data(samples);
    
    for (size_t i = 0; i < samples; i++) {
        float val = buffer[i] * 32767.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        int_data[i] = static_cast<int16_t>(val);
    }
    
    return writeAudio(int_data);
}

bool AudioRecorder::encodeAudio(const std::vector<int16_t>& samples, int64_t timestamp) {
    AudioData data;
    data.samples = samples;
    data.pts = timestamp;
    return writeAudio(data);
}

// ============================================
// FFMPEG FUNCTIONS
// ============================================

bool AudioRecorder::initFFmpegAudio() {
    av_register_all();
    avformat_network_init();
    return true;
}

void AudioRecorder::cleanupFFmpegAudio() {
    if (av_fifo) {
        av_fifo_free((AVFifoBuffer*)av_fifo);
        av_fifo = nullptr;
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
    if (av_format_ctx) {
        avformat_free_context((AVFormatContext*)av_format_ctx);
        av_format_ctx = nullptr;
    }
}

bool AudioRecorder::openOutputAudio(const std::string& filename) {
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
    codec_ctx->sample_rate = config.sample_rate;
    codec_ctx->channels = config.channels;
    codec_ctx->channel_layout = config.channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_ctx->bit_rate = config.bitrate;
    codec_ctx->time_base = {1, config.sample_rate};
    
    // Set private options
    av_opt_set(codec_ctx->priv_data, "profile", config.profile.c_str(), 0);
    
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
    
    // Allocate frame and packet
    av_frame = av_frame_alloc();
    av_packet = av_packet_alloc();
    av_fifo = av_fifo_alloc(1024);
    
    return true;
}

bool AudioRecorder::writeHeaderAudio() {
    AVFormatContext* fmt_ctx = (AVFormatContext*)av_format_ctx;
    int ret = avformat_write_header(fmt_ctx, nullptr);
    return ret >= 0;
}

bool AudioRecorder::writeTrailerAudio() {
    AVFormatContext* fmt_ctx = (AVFormatContext*)av_format_ctx;
    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
        avio_close(fmt_ctx->pb);
    }
    return true;
}

bool AudioRecorder::encodeAudioFrame(const AudioData& data) {
    AVCodecContext* codec_ctx = (AVCodecContext*)av_codec_ctx;
    AVFrame* avframe = (AVFrame*)av_frame;
    AVPacket* packet = (AVPacket*)av_packet;
    int ret;
    
    // Set frame properties
    avframe->nb_samples = data.samples.size() / config.channels;
    avframe->format = AV_SAMPLE_FMT_S16;
    avframe->sample_rate = config.sample_rate;
    avframe->channel_layout = config.channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    avframe->channels = config.channels;
    
    // Allocate buffer
    ret = av_frame_get_buffer(avframe, 0);
    if (ret < 0) {
        return false;
    }
    
    // Copy data
    memcpy(avframe->data[0], data.samples.data(), data.samples.size() * sizeof(int16_t));
    
    // Set PTS
    avframe->pts = data.pts / 1000;
    
    // Encode frame
    ret = avcodec_send_frame(codec_ctx, avframe);
    if (ret < 0) {
        av_frame_unref(avframe);
        return false;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            av_frame_unref(avframe);
            return false;
        }
        
        // Write packet
        av_interleaved_write_frame((AVFormatContext*)av_format_ctx, packet);
        av_packet_unref(packet);
    }
    
    av_frame_unref(avframe);
    return true;
}

bool AudioRecorder::flushAudioEncoder() {
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

// ============================================
// ENCODING THREAD
// ============================================

void AudioRecorder::encodeLoopAudio() {
    while (encoding) {
        AudioData data;
        bool has_data = false;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!audio_queue.empty()) {
                data = audio_queue.front();
                audio_queue.pop();
                has_data = true;
            }
        }
        
        if (has_data) {
            processAudio(data);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void AudioRecorder::processAudio(const AudioData& data) {
    if (state != RecorderState::RECORDING) {
        return;
    }
    
    // Encode audio
    if (encodeAudioFrame(data)) {
        updateStats(data);
        notifyStats();
        
        // Calculate progress
        if (stats.duration_seconds > 0) {
            float progress = std::min(100.0f, (float)(stats.duration_seconds / 3600.0f) * 100.0f);
            notifyProgress(progress);
        }
        
        // Callback
        if (on_audio) {
            on_audio(data);
        }
    }
}

// ============================================
// STATISTICS
// ============================================

AudioRecorder::RecordingStats AudioRecorder::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void AudioRecorder::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = RecordingStats{};
}

void AudioRecorder::updateStats(const AudioData& data) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.samples_recorded += data.samples.size();
    stats.bytes_written += data.samples.size() * sizeof(int16_t);
    
    auto now = std::chrono::system_clock::now();
    stats.last_sample_time = now;
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time).count();
    if (elapsed > 0) {
        stats.duration_seconds = elapsed;
    }
}

void AudioRecorder::notifyStats() {
    if (on_stats) {
        on_stats(getStats());
    }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void AudioRecorder::setState(RecorderState new_state) {
    if (state != new_state) {
        state = new_state;
        notifyStateChange();
    }
}

void AudioRecorder::notifyStateChange() {
    if (on_state_change) {
        on_state_change(state);
    }
}

void AudioRecorder::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "Audio Recorder Error: " << error << std::endl;
}

void AudioRecorder::notifyProgress(float progress) {
    if (on_progress) {
        on_progress(progress);
    }
}

// ============================================
// CALLBACKS
// ============================================

void AudioRecorder::setOnAudio(AudioCallback callback) {
    on_audio = callback;
}

void AudioRecorder::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

void AudioRecorder::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void AudioRecorder::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void AudioRecorder::setOnProgress(ProgressCallback callback) {
    on_progress = callback;
}

// ============================================
// CONFIGURATION
// ============================================

bool AudioRecorder::setConfig(const AudioConfig& cfg) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config = cfg;
    return true;
}

bool AudioRecorder::setSampleRate(int sample_rate) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.sample_rate = sample_rate;
    return true;
}

bool AudioRecorder::setChannels(int channels) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.channels = channels;
    return true;
}

bool AudioRecorder::setBitrate(int bitrate) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.bitrate = bitrate;
    return true;
}

bool AudioRecorder::setCodec(Codec codec) {
    if (state == RecorderState::RECORDING) {
        return false;
    }
    config.codec = codec;
    return true;
}

// ============================================
// UTILITY
// ============================================

std::string AudioRecorder::getCodecName(Codec codec) const {
    switch (codec) {
        case Codec::AAC: return "AAC";
        case Codec::MP3: return "MP3";
        case Codec::OPUS: return "Opus";
        case Codec::FLAC: return "FLAC";
        case Codec::WAV: return "WAV (PCM)";
        case Codec::PCM: return "PCM";
        default: return "Unknown";
    }
}

std::string AudioRecorder::getContainerName(Container container) const {
    switch (container) {
        case Container::MP4: return "MP4";
        case Container::MKV: return "Matroska";
        case Container::MP3: return "MP3";
        case Container::FLAC: return "FLAC";
        case Container::WAV: return "WAV";
        case Container::OGG: return "Ogg";
        default: return "Unknown";
    }
}

std::string AudioRecorder::getQualityName(Quality quality) const {
    switch (quality) {
        case Quality::LOW: return "Low";
        case Quality::MEDIUM: return "Medium";
        case Quality::HIGH: return "High";
        case Quality::ULTRA: return "Ultra";
        default: return "Unknown";
    }
}

std::string AudioRecorder::getFileExtension(Container container) const {
    switch (container) {
        case Container::MP4: return ".mp4";
        case Container::MKV: return ".mkv";
        case Container::MP3: return ".mp3";
        case Container::FLAC: return ".flac";
        case Container::WAV: return ".wav";
        case Container::OGG: return ".ogg";
        default: return ".wav";
    }
}

std::string AudioRecorder::generateFilename(const std::string& prefix) const {
    std::string timestamp = getTimestamp();
    std::string ext = getFileExtension(config.container);
    return prefix + "_" + timestamp + ext;
}

bool AudioRecorder::testRecording(int duration_seconds) {
    std::cout << "Testing audio recording for " << duration_seconds << " seconds..." << std::endl;
    
    std::string filename = generateFilename("test");
    if (!startRecording(filename)) {
        return false;
    }
    
    // Generate test tones
    int samples_per_second = config.sample_rate;
    int total_samples = duration_seconds * samples_per_second;
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
        writeAudio(buffer);
    }
    
    stopRecording();
    std::cout << "Test recording complete: " << filename << std::endl;
    return true;
}

std::string AudioRecorder::getCodecString(Codec codec) const {
    switch (codec) {
        case Codec::AAC: return "aac";
        case Codec::MP3: return "libmp3lame";
        case Codec::OPUS: return "libopus";
        case Codec::FLAC: return "flac";
        case Codec::WAV: return "pcm_s16le";
        case Codec::PCM: return "pcm_s16le";
        default: return "aac";
    }
}

std::string AudioRecorder::getContainerString(Container container) const {
    switch (container) {
        case Container::MP4: return "mp4";
        case Container::MKV: return "matroska";
        case Container::MP3: return "mp3";
        case Container::FLAC: return "flac";
        case Container::WAV: return "wav";
        case Container::OGG: return "ogg";
        default: return "mp4";
    }
}

std::string AudioRecorder::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", tm_info);
    return std::string(buffer);
}

uint64_t AudioRecorder::getCurrentTimeMicros() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
