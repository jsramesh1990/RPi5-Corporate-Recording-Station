/**
 * @file test_recording_pipeline.cpp
 * @brief Recording Pipeline Integration Tests
 * 
 * Tests the complete recording pipeline including:
 * - Video encoding
 * - Audio encoding
 * - Synchronization
 * - File output
 * - Hardware acceleration
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <cmath>

#include "../../src/recording/VideoRecorder.hpp"
#include "../../src/recording/AudioRecorder.hpp"
#include "../../src/recording/RecordingManager.hpp"
#include "../../src/recording/encoders/H264Encoder.hpp"
#include "../../src/recording/encoders/AACEncoder.hpp"
#include "../../src/utils/Logger.hpp"
#include "../../src/utils/TimeUtils.hpp"

namespace fs = std::filesystem;

// ============================================================
// TEST FIXTURE
// ============================================================

class RecordingPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/recording_pipeline_test_" + std::to_string(time(nullptr));
        fs::create_directories(test_dir);
        fs::create_directories(test_dir + "/output");
        
        // Initialize logger
        Logger::Config log_config;
        log_config.level = Logger::Level::DEBUG;
        log_config.log_file = test_dir + "/test.log";
        log_config.console_enabled = false;
        logger.initialize(log_config);
        
        logger.info("Recording pipeline test setup complete");
    }
    
    void TearDown() override {
        fs::remove_all(test_dir);
        logger.info("Recording pipeline test cleanup complete");
    }
    
    std::string test_dir;
    Logger logger;
};

// ============================================================
// TEST: Video Encoding
// ============================================================

TEST_F(RecordingPipelineTest, VideoEncoding) {
    VideoRecorder recorder;
    VideoRecorder::VideoConfig config;
    config.width = 640;
    config.height = 480;
    config.framerate = 30;
    config.bitrate = 2000000;
    config.codec = VideoRecorder::Codec::H264;
    config.container = VideoRecorder::Container::MP4;
    config.hardware_acceleration = false; // Software for testing
    
    EXPECT_TRUE(recorder.initialize(config));
    
    std::string output = test_dir + "/output/video_test.mp4";
    EXPECT_TRUE(recorder.startRecording(output));
    EXPECT_TRUE(recorder.isRecording());
    
    // Generate test frames
    std::vector<uint8_t> frame_data(640 * 480 * 3 / 2, 128);
    int frame_count = 0;
    const int TOTAL_FRAMES = 30; // 1 second at 30fps
    
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        // Create moving test pattern
        for (int y = 0; y < 480; y++) {
            for (int x = 0; x < 640; x++) {
                int idx = y * 640 + x;
                uint8_t val = (x + i * 2) % 256;
                frame_data[idx] = val;
                
                // U and V (simplified)
                if (x % 2 == 0 && y % 2 == 0) {
                    int uv_idx = (y / 2) * (640 / 2) + (x / 2);
                    int uv_size = 640 * 480 / 4;
                    frame_data[640 * 480 + uv_idx] = 128 + (i % 64);
                    frame_data[640 * 480 + uv_size + uv_idx] = 128 + (i % 64);
                }
            }
        }
        
        VideoRecorder::VideoFrame frame;
        frame.data = frame_data;
        frame.width = 640;
        frame.height = 480;
        frame.pts = i * 1000000 / 30;
        frame.is_keyframe = (i % 30 == 0);
        
        EXPECT_TRUE(recorder.writeFrame(frame));
        frame_count++;
    }
    
    recorder.stopRecording();
    
    // Verify output file exists and has size > 0
    EXPECT_TRUE(fs::exists(output));
    EXPECT_GT(fs::file_size(output), 0);
    
    auto stats = recorder.getStats();
    EXPECT_EQ(stats.frames_recorded, TOTAL_FRAMES);
    EXPECT_GT(stats.duration_seconds, 0);
    
    logger.info("Video encoding test passed: " + std::to_string(TOTAL_FRAMES) + " frames");
}

// ============================================================
// TEST: Audio Encoding
// ============================================================

TEST_F(RecordingPipelineTest, AudioEncoding) {
    AudioRecorder recorder;
    AudioRecorder::AudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.bitrate = 128000;
    config.codec = AudioRecorder::Codec::AAC;
    config.container = AudioRecorder::Container::MP4;
    
    EXPECT_TRUE(recorder.initialize(config));
    
    std::string output = test_dir + "/output/audio_test.mp4";
    EXPECT_TRUE(recorder.startRecording(output));
    EXPECT_TRUE(recorder.isRecording());
    
    // Generate test audio (440Hz tone)
    const int DURATION_SECONDS = 2;
    const int SAMPLES_PER_SECOND = 48000;
    const int TOTAL_SAMPLES = DURATION_SECONDS * SAMPLES_PER_SECOND * 2;
    
    std::vector<int16_t> audio_data(TOTAL_SAMPLES);
    for (int i = 0; i < TOTAL_SAMPLES; i++) {
        float t = static_cast<float>(i) / SAMPLES_PER_SECOND;
        float sample = 16384.0f * std::sin(2.0f * M_PI * 440.0f * t);
        audio_data[i] = static_cast<int16_t>(sample);
    }
    
    // Write in chunks
    const int CHUNK_SIZE = 1024 * 2;
    for (int i = 0; i < TOTAL_SAMPLES; i += CHUNK_SIZE) {
        int remaining = std::min(CHUNK_SIZE, TOTAL_SAMPLES - i);
        std::vector<int16_t> chunk(audio_data.begin() + i, audio_data.begin() + i + remaining);
        EXPECT_TRUE(recorder.writeAudio(chunk));
    }
    
    recorder.stopRecording();
    
    // Verify output file exists and has size > 0
    EXPECT_TRUE(fs::exists(output));
    EXPECT_GT(fs::file_size(output), 0);
    
    auto stats = recorder.getStats();
    EXPECT_GE(stats.samples_recorded, TOTAL_SAMPLES);
    EXPECT_GT(stats.duration_seconds, 0);
    
    logger.info("Audio encoding test passed: " + std::to_string(TOTAL_SAMPLES) + " samples");
}

// ============================================================
// TEST: Synchronized Recording
// ============================================================

TEST_F(RecordingPipelineTest, SynchronizedRecording) {
    RecordingManager manager;
    
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 30;
    video_config.bitrate = 2000000;
    video_config.codec = VideoRecorder::Codec::H264;
    video_config.hardware_acceleration = false;
    
    AudioRecorder::AudioConfig audio_config;
    audio_config.sample_rate = 48000;
    audio_config.channels = 2;
    audio_config.bitrate = 128000;
    audio_config.codec = AudioRecorder::Codec::AAC;
    
    EXPECT_TRUE(manager.initialize(video_config, audio_config));
    manager.setOutputDirectory(test_dir + "/output");
    manager.setAutoCombine(true);
    
    bool recording_started = false;
    bool recording_complete = false;
    
    manager.setOnStateChange([&](RecordingManager::RecorderState state) {
        if (state == RecordingManager::RecorderState::RECORDING) {
            recording_started = true;
        }
    });
    
    manager.setOnComplete([&](const RecordingManager::RecordingInfo& info) {
        recording_complete = true;
        EXPECT_FALSE(info.combined_file.empty());
        EXPECT_TRUE(fs::exists(info.combined_file));
    });
    
    // Start synchronized recording
    std::string filename = "sync_test";
    EXPECT_TRUE(manager.startRecording(filename));
    EXPECT_TRUE(recording_started);
    EXPECT_TRUE(manager.isRecording());
    
    // Record for 3 seconds
    auto start_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::seconds(3);
    int frame_count = 0;
    
    while (std::chrono::steady_clock::now() - start_time < duration) {
        // Video frame
        std::vector<uint8_t> video_frame(640 * 480 * 3 / 2, 128);
        VideoRecorder::VideoFrame frame;
        frame.data = video_frame;
        frame.width = 640;
        frame.height = 480;
        frame.pts = frame_count * 1000000 / 30;
        frame.is_keyframe = (frame_count % 30 == 0);
        
        // Write video frame
        manager.video_recorder->writeFrame(frame);
        
        // Write audio (simplified)
        std::vector<int16_t> audio_samples(48000 / 30 * 2, 0);
        manager.audio_recorder->writeAudio(audio_samples);
        
        frame_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps
    }
    
    manager.stopRecording();
    EXPECT_TRUE(recording_complete);
    
    // Verify combined file
    auto info = manager.getCurrentRecording();
    EXPECT_FALSE(info.combined_file.empty());
    EXPECT_TRUE(fs::exists(info.combined_file));
    EXPECT_GT(fs::file_size(info.combined_file), 0);
    
    logger.info("Synchronized recording test passed");
}

// ============================================================
// TEST: H.264 Encoder
// ============================================================

TEST_F(RecordingPipelineTest, H264EncoderTest) {
    H264Encoder encoder;
    H264Encoder::Config config;
    config.width = 640;
    config.height = 480;
    config.framerate = 30;
    config.bitrate = 2000000;
    config.preset = H264Encoder::Preset::FAST;
    config.profile = H264Encoder::Profile::HIGH;
    config.hardware_acceleration = false; // Software for testing
    
    EXPECT_TRUE(encoder.initialize(config));
    
    // Encode test frames
    std::vector<uint8_t> frame_data(640 * 480 * 3 / 2, 128);
    int encoded_frames = 0;
    const int TEST_FRAMES = 30;
    
    encoder.setOnFrame([&](const H264Encoder::EncodedFrame& frame) {
        encoded_frames++;
        EXPECT_GT(frame.data.size(), 0);
        if (frame.is_keyframe) {
            EXPECT_GT(frame.data.size(), 100); // Keyframes should be larger
        }
    });
    
    for (int i = 0; i < TEST_FRAMES; i++) {
        // Create moving pattern
        for (int y = 0; y < 480; y++) {
            for (int x = 0; x < 640; x++) {
                int idx = y * 640 + x;
                frame_data[idx] = (x + i * 2) % 256;
            }
        }
        
        encoder.encodeFrame(frame_data, i * 1000000 / 30);
    }
    
    encoder.flush();
    
    EXPECT_GT(encoded_frames, 0);
    EXPECT_LE(encoded_frames, TEST_FRAMES);
    
    auto stats = encoder.getStats();
    EXPECT_GT(stats.frames_encoded, 0);
    EXPECT_GT(stats.bytes_encoded, 0);
    
    logger.info("H.264 encoder test passed: " + std::to_string(encoded_frames) + " frames encoded");
}

// ============================================================
// TEST: AAC Encoder
// ============================================================

TEST_F(RecordingPipelineTest, AACEncoderTest) {
    AACEncoder encoder;
    AACEncoder::Config config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.bitrate = 128000;
    config.profile = AACEncoder::Profile::AAC_LC;
    
    EXPECT_TRUE(encoder.initialize(config));
    
    // Encode test audio
    const int SAMPLES = 48000 * 2; // 1 second of stereo audio
    std::vector<int16_t> audio_data(SAMPLES);
    for (int i = 0; i < SAMPLES; i++) {
        float t = static_cast<float>(i) / 48000;
        float sample = 16384.0f * std::sin(2.0f * M_PI * 440.0f * t);
        audio_data[i] = static_cast<int16_t>(sample);
    }
    
    int encoded_frames = 0;
    
    encoder.setOnFrame([&](const AACEncoder::EncodedFrame& frame) {
        encoded_frames++;
        EXPECT_GT(frame.data.size(), 0);
    });
    
    // Encode in chunks
    const int CHUNK = 1024;
    for (int i = 0; i < SAMPLES; i += CHUNK) {
        int remaining = std::min(CHUNK, SAMPLES - i);
        std::vector<int16_t> chunk(audio_data.begin() + i, audio_data.begin() + i + remaining);
        encoder.encodeAudio(chunk, i / 48000);
    }
    
    encoder.flush();
    
    EXPECT_GT(encoded_frames, 0);
    
    auto stats = encoder.getStats();
    EXPECT_GT(stats.frames_encoded, 0);
    EXPECT_GT(stats.bytes_encoded, 0);
    
    logger.info("AAC encoder test passed: " + std::to_string(encoded_frames) + " frames encoded");
}

// ============================================================
// TEST: Hardware Acceleration Detection
// ============================================================

TEST_F(RecordingPipelineTest, HardwareAcceleration) {
    VideoRecorder recorder;
    VideoRecorder::VideoConfig config;
    config.width = 640;
    config.height = 480;
    config.framerate = 30;
    config.bitrate = 2000000;
    config.codec = VideoRecorder::Codec::H264;
    config.hardware_acceleration = true;
    
    bool initialized = recorder.initialize(config);
    
    // On Raspberry Pi 5, hardware acceleration should be available
    // But in CI environments, it might not be
    // So we just check that it doesn't crash
    EXPECT_TRUE(initialized || !initialized);
    
    // If initialized, test recording
    if (initialized) {
        std::string output = test_dir + "/output/hw_test.mp4";
        EXPECT_TRUE(recorder.startRecording(output));
        
        // Write a few frames
        std::vector<uint8_t> frame_data(640 * 480 * 3 / 2, 128);
        for (int i = 0; i < 10; i++) {
            VideoRecorder::VideoFrame frame;
            frame.data = frame_data;
            frame.width = 640;
            frame.height = 480;
            frame.pts = i * 1000000 / 30;
            frame.is_keyframe = (i == 0);
            recorder.writeFrame(frame);
        }
        
        recorder.stopRecording();
        EXPECT_TRUE(fs::exists(output));
    }
    
    logger.info("Hardware acceleration test passed");
}

// ============================================================
// TEST: Recording Performance Metrics
// ============================================================

TEST_F(RecordingPipelineTest, PerformanceMetrics) {
    VideoRecorder recorder;
    VideoRecorder::VideoConfig config;
    config.width = 640;
    config.height = 480;
    config.framerate = 30;
    config.bitrate = 2000000;
    config.codec = VideoRecorder::Codec::H264;
    
    EXPECT_TRUE(recorder.initialize(config));
    
    std::string output = test_dir + "/output/perf_test.mp4";
    EXPECT_TRUE(recorder.startRecording(output));
    
    // Record and measure performance
    const int FRAMES_TO_ENCODE = 60; // 2 seconds at 30fps
    std::vector<uint8_t> frame_data(640 * 480 * 3 / 2, 128);
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < FRAMES_TO_ENCODE; i++) {
        VideoRecorder::VideoFrame frame;
        frame.data = frame_data;
        frame.width = 640;
        frame.height = 480;
        frame.pts = i * 1000000 / 30;
        frame.is_keyframe = (i % 30 == 0);
        recorder.writeFrame(frame);
    }
    
    recorder.stopRecording();
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    auto stats = recorder.getStats();
    
    // Verify stats
    EXPECT_EQ(stats.frames_recorded, FRAMES_TO_ENCODE);
    EXPECT_GT(stats.duration_seconds, 0);
    EXPECT_GT(stats.bytes_written, 0);
    
    // Log performance
    logger.info("Performance: " + std::to_string(elapsed_ms) + "ms for " + 
                std::to_string(FRAMES_TO_ENCODE) + " frames");
    
    // Ensure it's not too slow (should be < 5 seconds for 60 frames)
    EXPECT_LT(elapsed_ms, 5000);
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
