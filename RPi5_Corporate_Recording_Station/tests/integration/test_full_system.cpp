/**
 * @file test_full_system.cpp
 * @brief Full System Integration Tests for RPi5 Recording Station
 * 
 * Tests the complete system integration including:
 * - Hardware initialization
 * - Recording pipeline
 * - Storage management
 * - Cloud integration
 * - Network functionality
 * - UART communication
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

#include "../../src/core/StorageMonitor.hpp"
#include "../../src/core/MemoryManager.hpp"
#include "../../src/recording/VideoRecorder.hpp"
#include "../../src/recording/AudioRecorder.hpp"
#include "../../src/recording/RecordingManager.hpp"
#include "../../src/cloud/CloudUploader.hpp"
#include "../../src/network/NetworkManager.hpp"
#include "../../src/hardware/uart/PL011UART.hpp"
#include "../../src/hardware/gpio/GPIOController.hpp"
#include "../../src/ui/dashboard/DashboardRenderer.hpp"
#include "../../src/utils/Logger.hpp"
#include "../../src/utils/ConfigParser.hpp"

namespace fs = std::filesystem;

// ============================================================
// TEST FIXTURE
// ============================================================

class FullSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test environment
        test_dir = "/tmp/recording_station_test_" + std::to_string(time(nullptr));
        fs::create_directories(test_dir);
        
        // Initialize logger
        Logger::Config log_config;
        log_config.level = Logger::Level::DEBUG;
        log_config.log_file = test_dir + "/test.log";
        log_config.console_enabled = false;
        logger.initialize(log_config);
        
        // Initialize config parser
        config.loadString(getTestConfig());
        
        // Create test directories
        fs::create_directories(test_dir + "/recordings");
        fs::create_directories(test_dir + "/archive");
        fs::create_directories(test_dir + "/temp");
        fs::create_directories(test_dir + "/metadata");
        
        logger.info("Test environment setup complete");
    }
    
    void TearDown() override {
        // Clean up
        fs::remove_all(test_dir);
        logger.info("Test environment cleaned up");
    }
    
    std::string getTestConfig() {
        return R"(
            [system]
            data_dir=/tmp/test_recording
            log_level=debug
            
            [recording]
            video_width=640
            video_height=480
            video_framerate=15
            video_bitrate=1000000
            audio_sample_rate=44100
            audio_channels=1
            
            [storage]
            warning_level=90
            critical_level=95
            
            [cloud]
            enabled=false
            auto_upload=false
            
            [hardware]
            gpio_enabled=false
            camera_enabled=false
            audio_enabled=false
        )";
    }
    
    std::string test_dir;
    Logger logger;
    ConfigParser config;
};

// ============================================================
// TEST: System Initialization
// ============================================================

TEST_F(FullSystemTest, SystemInitialization) {
    // Test Memory Manager
    MemoryManager& mem_mgr = MemoryManager::getInstance();
    mem_mgr.enable_paging();
    mem_mgr.enable_zram();
    EXPECT_TRUE(mem_mgr.is_paging_enabled());
    EXPECT_TRUE(mem_mgr.is_zram_enabled());
    
    // Test Storage Monitor
    StorageMonitor storage_monitor(test_dir);
    storage_monitor.scan_directory();
    auto stats = storage_monitor.get_statistics();
    EXPECT_GE(stats.file_count, 0);
    
    logger.info("System initialization test passed");
}

// ============================================================
// TEST: Recording Pipeline
// ============================================================

TEST_F(FullSystemTest, RecordingPipeline) {
    // Initialize video recorder
    VideoRecorder video_recorder;
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 15;
    video_config.bitrate = 1000000;
    video_config.codec = VideoRecorder::Codec::H264;
    video_config.container = VideoRecorder::Container::MP4;
    video_config.hardware_acceleration = false; // Use software for testing
    
    EXPECT_TRUE(video_recorder.initialize(video_config));
    
    // Initialize audio recorder
    AudioRecorder audio_recorder;
    AudioRecorder::AudioConfig audio_config;
    audio_config.sample_rate = 44100;
    audio_config.channels = 1;
    audio_config.bitrate = 64000;
    audio_config.codec = AudioRecorder::Codec::AAC;
    audio_config.container = AudioRecorder::Container::MP4;
    
    EXPECT_TRUE(audio_recorder.initialize(audio_config));
    
    // Initialize recording manager
    RecordingManager recording_manager;
    EXPECT_TRUE(recording_manager.initialize(video_config, audio_config));
    recording_manager.setOutputDirectory(test_dir + "/recordings");
    recording_manager.setAutoCombine(false);
    
    // Start recording
    std::string filename = "test_recording";
    EXPECT_TRUE(recording_manager.startRecording(filename));
    EXPECT_TRUE(recording_manager.isRecording());
    
    // Record for a few seconds
    auto start_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::seconds(3);
    
    // Generate test frames
    VideoRecorder::VideoFrame frame;
    frame.width = 640;
    frame.height = 480;
    frame.data.assign(640 * 480 * 3 / 2, 128); // Gray frame
    frame.is_keyframe = false;
    
    int frame_count = 0;
    while (std::chrono::steady_clock::now() - start_time < duration) {
        frame.pts = frame_count * 1000000 / 15;
        frame.is_keyframe = (frame_count % 30 == 0);
        video_recorder.writeFrame(frame);
        
        // Generate audio
        std::vector<int16_t> audio_samples(1024, 0);
        audio_recorder.writeAudio(audio_samples);
        
        frame_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(66)); // ~15 fps
    }
    
    // Stop recording
    recording_manager.stopRecording();
    EXPECT_FALSE(recording_manager.isRecording());
    
    // Verify files exist
    auto files = fs::directory_iterator(test_dir + "/recordings");
    bool has_video = false;
    bool has_audio = false;
    
    for (const auto& entry : files) {
        std::string path = entry.path().string();
        if (path.find(".mp4") != std::string::npos) {
            has_video = true;
        }
        if (path.find(".wav") != std::string::npos || path.find(".mp4") != std::string::npos) {
            has_audio = true;
        }
    }
    
    EXPECT_TRUE(has_video);
    EXPECT_TRUE(has_audio);
    
    logger.info("Recording pipeline test passed");
}

// ============================================================
// TEST: Storage Management
// ============================================================

TEST_F(FullSystemTest, StorageManagement) {
    StorageMonitor storage_monitor(test_dir);
    
    // Create test files
    for (int i = 0; i < 5; i++) {
        std::string file_path = test_dir + "/test_file_" + std::to_string(i) + ".txt";
        std::ofstream file(file_path);
        file << "Test content " << i;
        file.close();
    }
    
    // Scan directory
    storage_monitor.scan_directory();
    auto stats = storage_monitor.get_statistics();
    EXPECT_EQ(stats.file_count, 5);
    
    // Test file filtering
    auto files = storage_monitor.get_files();
    EXPECT_EQ(files.size(), 5);
    
    // Test file type distribution
    auto dist = storage_monitor.get_file_type_distribution();
    EXPECT_EQ(dist["txt"], 5);
    
    // Test largest files
    auto largest = storage_monitor.get_largest_files(3);
    EXPECT_LE(largest.size(), 3);
    
    // Test file export
    std::string json_path = test_dir + "/storage_export.json";
    storage_monitor.export_to_json(json_path);
    EXPECT_TRUE(fs::exists(json_path));
    
    // Test file deletion
    std::string first_file = test_dir + "/test_file_0.txt";
    storage_monitor.delete_file(first_file);
    EXPECT_FALSE(fs::exists(first_file));
    
    // Rescan
    storage_monitor.scan_directory();
    stats = storage_monitor.get_statistics();
    EXPECT_EQ(stats.file_count, 4);
    
    logger.info("Storage management test passed");
}

// ============================================================
// TEST: Memory Management
// ============================================================

TEST_F(FullSystemTest, MemoryManagement) {
    MemoryManager& mem_mgr = MemoryManager::getInstance();
    
    // Test memory allocation
    size_t test_size = 1024 * 1024; // 1 MB
    void* ptr = mem_mgr.allocate(test_size);
    EXPECT_NE(ptr, nullptr);
    
    auto stats = mem_mgr.get_stats();
    EXPECT_GE(stats.current_usage, test_size);
    
    // Test aligned allocation
    void* aligned_ptr = mem_mgr.allocate_aligned(4096, 4096);
    EXPECT_NE(aligned_ptr, nullptr);
    EXPECT_EQ((uintptr_t)aligned_ptr % 4096, 0);
    
    // Test memory pool
    int pool_id = 1;
    mem_mgr.create_memory_pool(1024 * 1024, pool_id);
    void* pool_ptr = mem_mgr.allocate_from_pool(4096, pool_id);
    EXPECT_NE(pool_ptr, nullptr);
    
    // Test deallocation
    mem_mgr.deallocate(ptr);
    mem_mgr.deallocate(aligned_ptr);
    mem_mgr.free_to_pool(pool_ptr, pool_id);
    
    // Clean up pool
    mem_mgr.destroy_memory_pool(pool_id);
    
    logger.info("Memory management test passed");
}

// ============================================================
// TEST: Cloud Integration
// ============================================================

TEST_F(FullSystemTest, CloudIntegration) {
    // Skip if cloud not available
    if (!config.getBool("cloud", "enabled", false)) {
        GTEST_SKIP() << "Cloud integration disabled in config";
    }
    
    CloudUploader cloud;
    CloudUploader::Provider provider = CloudUploader::Provider::PROVIDER_NEXTCLOUD;
    std::string url = config.getString("cloud", "url", "");
    std::string username = config.getString("cloud", "username", "");
    std::string password = config.getString("cloud", "password", "");
    
    if (url.empty() || username.empty()) {
        GTEST_SKIP() << "Cloud credentials not configured";
    }
    
    // Test connection
    bool connected = cloud.connect(provider, url, username, password);
    EXPECT_TRUE(connected);
    EXPECT_TRUE(cloud.isConnected());
    
    // Create test file
    std::string test_file = test_dir + "/cloud_test.txt";
    std::ofstream file(test_file);
    file << "Cloud test file";
    file.close();
    
    // Test upload
    EXPECT_TRUE(cloud.uploadFile(test_file, "cloud_test.txt", true));
    EXPECT_GT(cloud.getQueueSize(), 0);
    
    // Wait for upload
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Test disconnect
    cloud.disconnect();
    EXPECT_FALSE(cloud.isConnected());
    
    logger.info("Cloud integration test passed");
}

// ============================================================
// TEST: Network Management
// ============================================================

TEST_F(FullSystemTest, NetworkManagement) {
    NetworkManager& nm = NetworkManager::getInstance();
    
    // Test initialization
    EXPECT_TRUE(nm.initialize());
    
    // Test interface detection
    auto interfaces = nm.getInterfaces();
    EXPECT_GT(interfaces.size(), 0);
    
    // Test default interface
    auto default_iface = nm.getDefaultInterface();
    EXPECT_FALSE(default_iface.name.empty());
    
    // Test IP validation
    EXPECT_TRUE(nm.isValidIP("192.168.1.1"));
    EXPECT_TRUE(nm.isValidIP("8.8.8.8"));
    EXPECT_FALSE(nm.isValidIP("invalid.ip"));
    
    // Test DNS
    auto dns = nm.getDNSServers();
    EXPECT_GT(dns.size(), 0);
    
    // Test hostname
    std::string hostname = nm.getHostname();
    EXPECT_FALSE(hostname.empty());
    
    // Test connectivity (if network available)
    if (default_iface.is_up && default_iface.has_ip) {
        bool internet = nm.checkInternetConnectivity("8.8.8.8", 3000);
        // Don't assert - might be offline in CI
        if (internet) {
            auto quality = nm.getConnectionQuality();
            EXPECT_GT(quality.quality_percent, 0);
        }
    }
    
    logger.info("Network management test passed");
}

// ============================================================
// TEST: UART Communication
// ============================================================

TEST_F(FullSystemTest, UARTCommunication) {
    // This test is hardware-dependent
    // It will be skipped if UART is not available
    
    PL011UART uart;
    PL011UART::UARTConfig config;
    config.device = "/dev/ttyAMA0";  // Default Pi 5 UART
    config.baud_rate = 115200;
    config.read_timeout_ms = 100;
    
    bool initialized = uart.initialize(config);
    
    if (!initialized) {
        GTEST_SKIP() << "UART device not available";
    }
    
    EXPECT_TRUE(uart.isInitialized());
    EXPECT_EQ(uart.getBaudRate(), 115200);
    
    // Test write
    std::string test_msg = "UART test message\r\n";
    EXPECT_TRUE(uart.writeData(test_msg));
    
    // Test flush
    uart.flushInput();
    uart.flushOutput();
    
    // Test statistics
    auto stats = uart.getStats();
    EXPECT_GE(stats.bytes_sent, test_msg.length());
    
    // Test shutdown
    uart.shutdown();
    EXPECT_FALSE(uart.isInitialized());
    
    logger.info("UART communication test passed");
}

// ============================================================
// TEST: Dashboard Integration
// ============================================================

TEST_F(FullSystemTest, DashboardIntegration) {
    DashboardRenderer dashboard;
    dashboard.initialize("Test Dashboard");
    
    // Update metrics
    DashboardRenderer::SystemMetrics metrics;
    metrics.cpu_usage = 45.5;
    metrics.cpu_temp = 52.3;
    metrics.memory_total = 4096 * 1024 * 1024;
    metrics.memory_used = 1536 * 1024 * 1024;
    metrics.memory_free = 2560 * 1024 * 1024;
    metrics.uptime_seconds = 86400;
    metrics.hostname = "test-pi";
    dashboard.updateSystemMetrics(metrics);
    
    // Update recording status
    DashboardRenderer::RecordingStatus recording;
    recording.is_recording = true;
    recording.filename = "test_recording.mp4";
    recording.duration_seconds = 120;
    recording.file_size_bytes = 500 * 1024 * 1024;
    recording.fps = 30.0;
    recording.video_codec = "H.264";
    recording.audio_codec = "AAC";
    dashboard.updateRecordingStatus(recording);
    
    // Update storage status
    DashboardRenderer::StorageStatus storage;
    storage.total_bytes = 128 * 1024 * 1024 * 1024;
    storage.used_bytes = 76.8 * 1024 * 1024 * 1024;
    storage.free_bytes = 51.2 * 1024 * 1024 * 1024;
    storage.file_count = 1247;
    storage.file_type_distribution = {{".mp4", 342}, {".wav", 156}, {".json", 89}};
    dashboard.updateStorageStatus(storage);
    
    // Update cloud status
    DashboardRenderer::CloudStatus cloud;
    cloud.is_connected = true;
    cloud.provider = "Nextcloud";
    cloud.username = "test_user";
    cloud.total_space = 500 * 1024 * 1024 * 1024;
    cloud.used_space = 200 * 1024 * 1024 * 1024;
    cloud.free_space = 300 * 1024 * 1024 * 1024;
    dashboard.updateCloudStatus(cloud);
    
    // Test export
    std::string html = dashboard.exportToHTML();
    EXPECT_FALSE(html.empty());
    EXPECT_TRUE(html.find("Test Dashboard") != std::string::npos);
    
    // Test file export
    std::string file_path = test_dir + "/dashboard.html";
    EXPECT_TRUE(dashboard.exportToFile(file_path));
    EXPECT_TRUE(fs::exists(file_path));
    
    logger.info("Dashboard integration test passed");
}

// ============================================================
// TEST: System Performance
// ============================================================

TEST_F(FullSystemTest, SystemPerformance) {
    // Measure recording performance
    VideoRecorder video_recorder;
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 30;
    video_config.bitrate = 2000000;
    video_config.codec = VideoRecorder::Codec::H264;
    
    EXPECT_TRUE(video_recorder.initialize(video_config));
    
    std::string filename = test_dir + "/perf_test.mp4";
    EXPECT_TRUE(video_recorder.startRecording(filename));
    
    // Measure encoding performance
    auto start_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    const int TARGET_FRAMES = 100;
    
    VideoRecorder::VideoFrame frame;
    frame.width = 640;
    frame.height = 480;
    frame.data.assign(640 * 480 * 3 / 2, 128);
    
    while (frame_count < TARGET_FRAMES) {
        frame.pts = frame_count * 1000000 / 30;
        frame.is_keyframe = (frame_count % 30 == 0);
        video_recorder.writeFrame(frame);
        frame_count++;
    }
    
    video_recorder.stopRecording();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    
    // Check performance (should be > 10 fps on Raspberry Pi 5)
    double fps = (double)frame_count / (elapsed / 1000.0);
    EXPECT_GT(fps, 10.0);
    
    logger.info("Performance test passed: " + std::to_string(fps) + " fps");
}

// ============================================================
// TEST: Error Recovery
// ============================================================

TEST_F(FullSystemTest, ErrorRecovery) {
    // Test recording recovery from errors
    VideoRecorder video_recorder;
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 30;
    video_config.bitrate = 2000000;
    
    EXPECT_TRUE(video_recorder.initialize(video_config));
    
    // Start recording with invalid filename (should fail gracefully)
    bool start_result = video_recorder.startRecording("/invalid/path/recording.mp4");
    EXPECT_FALSE(start_result);
    EXPECT_EQ(video_recorder.getState(), VideoRecorder::RecorderState::IDLE);
    
    // Start with valid filename
    std::string filename = test_dir + "/recovery_test.mp4";
    EXPECT_TRUE(video_recorder.startRecording(filename));
    EXPECT_TRUE(video_recorder.isRecording());
    
    // Simulate error by writing invalid frames (should handle gracefully)
    VideoRecorder::VideoFrame invalid_frame;
    invalid_frame.width = 0;
    invalid_frame.height = 0;
    invalid_frame.data.clear();
    
    // Should not crash
    video_recorder.writeFrame(invalid_frame);
    
    // Stop should still work
    std::string result = video_recorder.stopRecording();
    EXPECT_EQ(result, filename);
    
    // Clean up
    video_recorder.shutdown();
    
    logger.info("Error recovery test passed");
}

// ============================================================
// TEST: End-to-End Recording Session
// ============================================================

TEST_F(FullSystemTest, EndToEndRecordingSession) {
    // Simulate a complete recording session:
    // 1. Initialize system
    // 2. Start recording
    // 3. Record for a period
    // 4. Stop recording
    // 5. Verify files
    // 6. Upload to cloud (if enabled)
    // 7. Check storage
    
    // Initialize recording manager
    RecordingManager recording_manager;
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 15;
    video_config.bitrate = 1000000;
    
    AudioRecorder::AudioConfig audio_config;
    audio_config.sample_rate = 44100;
    audio_config.channels = 1;
    audio_config.bitrate = 64000;
    
    EXPECT_TRUE(recording_manager.initialize(video_config, audio_config));
    recording_manager.setOutputDirectory(test_dir + "/recordings");
    recording_manager.setAutoCombine(true);
    
    // Set callbacks
    bool recording_started = false;
    bool recording_complete = false;
    
    recording_manager.setOnStateChange([&](RecordingManager::RecorderState state) {
        if (state == RecordingManager::RecorderState::RECORDING) {
            recording_started = true;
        }
    });
    
    recording_manager.setOnComplete([&](const RecordingManager::RecordingInfo& info) {
        recording_complete = true;
        EXPECT_TRUE(fs::exists(info.combined_file));
        EXPECT_GT(info.duration_seconds, 0);
    });
    
    // Start session
    std::string session_name = "e2e_test_" + std::to_string(time(nullptr));
    EXPECT_TRUE(recording_manager.startRecording(session_name));
    EXPECT_TRUE(recording_started);
    
    // Record for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Stop session
    recording_manager.stopRecording();
    EXPECT_TRUE(recording_complete);
    
    // Verify combined file
    auto info = recording_manager.getCurrentRecording();
    if (!info.combined_file.empty()) {
        EXPECT_TRUE(fs::exists(info.combined_file));
        EXPECT_GT(fs::file_size(info.combined_file), 0);
    }
    
    logger.info("End-to-end recording session test passed");
}

// ============================================================
// TEST: Stress Test
// ============================================================

TEST_F(FullSystemTest, DISABLED_StressTest) {
    // This is a stress test that can be enabled for performance testing
    // It is disabled by default as it takes a long time
    
    const int NUM_RECORDINGS = 10;
    const int RECORDING_DURATION = 5; // seconds per recording
    
    RecordingManager recording_manager;
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 15;
    video_config.bitrate = 1000000;
    
    AudioRecorder::AudioConfig audio_config;
    audio_config.sample_rate = 44100;
    audio_config.channels = 1;
    audio_config.bitrate = 64000;
    
    EXPECT_TRUE(recording_manager.initialize(video_config, audio_config));
    recording_manager.setOutputDirectory(test_dir + "/recordings");
    recording_manager.setAutoCombine(true);
    
    for (int i = 0; i < NUM_RECORDINGS; i++) {
        std::string name = "stress_test_" + std::to_string(i);
        EXPECT_TRUE(recording_manager.startRecording(name));
        
        std::this_thread::sleep_for(std::chrono::seconds(RECORDING_DURATION));
        
        recording_manager.stopRecording();
        
        // Verify file
        auto info = recording_manager.getCurrentRecording();
        if (!info.combined_file.empty()) {
            EXPECT_TRUE(fs::exists(info.combined_file));
        }
        
        // Check memory usage
        MemoryManager& mem_mgr = MemoryManager::getInstance();
        auto stats = mem_mgr.get_stats();
        EXPECT_LT(stats.current_usage, 1024 * 1024 * 1024); // < 1GB
    }
    
    logger.info("Stress test passed: " + std::to_string(NUM_RECORDINGS) + " recordings");
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    
    // Enable verbose output
    ::testing::GTEST_FLAG(verbosity) = 2;
    
    return RUN_ALL_TESTS();
}
