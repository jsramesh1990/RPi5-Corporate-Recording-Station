/**
 * @file stress_test.cpp
 * @brief Stress Test for RPi5 Recording Station
 * 
 * Stress tests system under heavy load including:
 * - Long-duration recording
 * - Concurrent operations
 * - Memory pressure
 * - Storage pressure
 * - Network load
 * - CPU load
 * - Recovery from failures
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "../../src/core/MemoryManager.hpp"
#include "../../src/core/StorageMonitor.hpp"
#include "../../src/recording/VideoRecorder.hpp"
#include "../../src/recording/AudioRecorder.hpp"
#include "../../src/recording/RecordingManager.hpp"
#include "../../src/cloud/CloudUploader.hpp"
#include "../../src/network/NetworkManager.hpp"
#include "../../src/utils/Logger.hpp"
#include "../../src/utils/TimeUtils.hpp"

namespace fs = std::filesystem;

// ============================================================
// STRESS TEST CONFIGURATION
// ============================================================

struct StressTestConfig {
    int duration_seconds = 60;
    int concurrent_operations = 4;
    int memory_pressure_mb = 512;
    int storage_pressure_mb = 1024;
    bool test_recording = true;
    bool test_cloud = false;
    bool test_network = true;
    bool test_memory = true;
    bool test_storage = true;
    bool test_cpu = true;
    int error_injection_rate = 5; // percentage
    bool verbose = false;
};

// ============================================================
// STRESS TEST FIXTURE
// ============================================================

class StressTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/stress_test_" + std::to_string(time(nullptr));
        fs::create_directories(test_dir);
        fs::create_directories(test_dir + "/recordings");
        fs::create_directories(test_dir + "/temp");
        
        // Initialize logger (reduced verbosity)
        Logger::Config log_config;
        log_config.level = Logger::Level::WARNING;
        log_config.log_file = test_dir + "/stress.log";
        log_config.console_enabled = false;
        logger.initialize(log_config);
        
        // Initialize components
        memory_manager = &MemoryManager::getInstance();
        storage_monitor = std::make_unique<StorageMonitor>(test_dir);
        network_manager = &NetworkManager::getInstance();
        network_manager->initialize();
        
        config.duration_seconds = 30; // Default for CI
        config.concurrent_operations = 2;
        config.memory_pressure_mb = 256;
        config.storage_pressure_mb = 512;
        config.error_injection_rate = 0;
        config.verbose = false;
        
        stop_test = false;
        errors_detected = 0;
        
        logger.info("Stress test setup complete");
    }
    
    void TearDown() override {
        stop_test = true;
        
        if (recording_manager) {
            recording_manager->shutdown();
        }
        
        fs::remove_all(test_dir);
        logger.info("Stress test cleanup complete");
    }
    
    // Stress test helpers
    void injectError() {
        if (config.error_injection_rate > 0) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 100);
            
            if (dist(gen) < config.error_injection_rate) {
                // Simulate an error
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    dist(gen) % 500 + 100));
            }
        }
    }
    
    bool shouldContinue() {
        return !stop_test && 
               (std::chrono::steady_clock::now() - test_start < 
                std::chrono::seconds(config.duration_seconds));
    }
    
    std::string test_dir;
    Logger logger;
    StressTestConfig config;
    MemoryManager* memory_manager;
    std::unique_ptr<StorageMonitor> storage_monitor;
    NetworkManager* network_manager;
    std::unique_ptr<RecordingManager> recording_manager;
    std::atomic<bool> stop_test;
    std::atomic<int> errors_detected;
    std::chrono::steady_clock::time_point test_start;
    
    // Random number generation
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<> dist{0, 100};
};

// ============================================================
// TEST: Long Duration Recording
// ============================================================

TEST_F(StressTest, LongDurationRecording) {
    std::cout << "\n=== Long Duration Recording Stress Test ===" << std::endl;
    std::cout << "Duration: " << config.duration_seconds << " seconds" << std::endl;
    
    // Initialize recording manager
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 15;
    video_config.bitrate = 1000000;
    video_config.codec = VideoRecorder::Codec::H264;
    video_config.hardware_acceleration = false;
    
    AudioRecorder::AudioConfig audio_config;
    audio_config.sample_rate = 44100;
    audio_config.channels = 1;
    audio_config.bitrate = 64000;
    audio_config.codec = AudioRecorder::Codec::AAC;
    
    recording_manager = std::make_unique<RecordingManager>();
    EXPECT_TRUE(recording_manager->initialize(video_config, audio_config));
    recording_manager->setOutputDirectory(test_dir + "/recordings");
    recording_manager->setAutoCombine(true);
    
    // Start recording
    recording_manager->startRecording("stress_test");
    EXPECT_TRUE(recording_manager->isRecording());
    
    test_start = std::chrono::steady_clock::now();
    int frame_count = 0;
    int segment_count = 0;
    
    // Generate test frames
    std::vector<uint8_t> frame_data(640 * 480 * 3 / 2, 128);
    
    while (shouldContinue()) {
        // Record for a segment
        const int SEGMENT_FRAMES = 30;
        
        for (int i = 0; i < SEGMENT_FRAMES && shouldContinue(); i++) {
            VideoRecorder::VideoFrame frame;
            frame.data = frame_data;
            frame.width = 640;
            frame.height = 480;
            frame.pts = frame_count * 1000000 / 15;
            frame.is_keyframe = (frame_count % 30 == 0);
            
            // Write video frame
            if (recording_manager->video_recorder) {
                recording_manager->video_recorder->writeFrame(frame);
            }
            
            // Write audio (simplified)
            std::vector<int16_t> audio_samples(1024, 0);
            if (recording_manager->audio_recorder) {
                recording_manager->audio_recorder->writeAudio(audio_samples);
            }
            
            frame_count++;
            injectError();
        }
        
        segment_count++;
        
        // Check if recording is still active
        if (!recording_manager->isRecording()) {
            errors_detected++;
            std::cout << "  Recording stopped unexpectedly at " 
                      << segment_count << " segments" << std::endl;
            break;
        }
        
        // Log progress
        if (segment_count % 10 == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - test_start).count();
            std::cout << "  Recorded " << frame_count << " frames in " 
                      << elapsed << "s" << std::endl;
        }
        
        // Simulate real-time (15fps)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Stop recording
    recording_manager->stopRecording();
    
    // Verify recording completed
    auto info = recording_manager->getCurrentRecording();
    EXPECT_GT(frame_count, 0);
    EXPECT_FALSE(info.combined_file.empty());
    
    if (!info.combined_file.empty() && fs::exists(info.combined_file)) {
        auto size = fs::file_size(info.combined_file);
        std::cout << "  Recording file size: " << size / (1024 * 1024) << " MB" << std::endl;
        EXPECT_GT(size, 0);
    }
    
    std::cout << "  Stress test completed: " << frame_count << " frames, "
              << segment_count << " segments, " 
              << errors_detected << " errors" << std::endl;
}

// ============================================================
// TEST: Concurrent Operations
// ============================================================

TEST_F(StressTest, ConcurrentOperations) {
    std::cout << "\n=== Concurrent Operations Stress Test ===" << std::endl;
    std::cout << "Concurrent operations: " << config.concurrent_operations << std::endl;
    std::cout << "Duration: " << config.duration_seconds << " seconds" << std::endl;
    
    test_start = std::chrono::steady_clock::now();
    std::atomic<int> operations_completed(0);
    std::atomic<int> operations_failed(0);
    std::mutex log_mutex;
    
    // Create worker threads
    std::vector<std::thread> workers;
    
    for (int i = 0; i < config.concurrent_operations; i++) {
        workers.emplace_back([&, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 3);
            
            while (shouldContinue()) {
                int op = op_dist(gen);
                bool success = false;
                
                try {
                    switch (op) {
                        case 0: { // Memory allocation
                            size_t size = (dist(gen) % 10 + 1) * 1024 * 1024;
                            void* ptr = memory_manager->allocate(size);
                            if (ptr) {
                                memory_manager->deallocate(ptr);
                                success = true;
                            }
                            break;
                        }
                        case 1: { // File I/O
                            std::string path = test_dir + "/temp/concurrent_" + 
                                               std::to_string(i) + "_" + 
                                               std::to_string(operations_completed) + ".tmp";
                            std::ofstream file(path);
                            file << "Test data" << std::endl;
                            file.close();
                            fs::remove(path);
                            success = true;
                            break;
                        }
                        case 2: { // Storage scan
                            storage_monitor->scan_directory();
                            success = true;
                            break;
                        }
                        case 3: { // Network check
                            if (config.test_network) {
                                network_manager->checkInternetConnectivity("8.8.8.8", 1000);
                                success = true;
                            }
                            break;
                        }
                    }
                } catch (...) {
                    success = false;
                }
                
                if (success) {
                    operations_completed++;
                } else {
                    operations_failed++;
                }
                
                injectError();
                
                // Small delay to prevent CPU saturation
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    dist(gen) % 50 + 10));
            }
        });
    }
    
    // Monitor progress
    int last_completed = 0;
    while (shouldContinue()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        int current = operations_completed.load();
        int failed = operations_failed.load();
        std::cout << "  Operations: " << current << " completed, " 
                  << failed << " failed" << std::endl;
        last_completed = current;
    }
    
    // Wait for workers
    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    
    std::cout << "  Concurrent stress test completed: "
              << operations_completed << " operations, "
              << operations_failed << " failures" << std::endl;
    
    EXPECT_GT(operations_completed, 0);
}

// ============================================================
// TEST: Memory Pressure
// ============================================================

TEST_F(StressTest, MemoryPressure) {
    std::cout << "\n=== Memory Pressure Stress Test ===" << std::endl;
    std::cout << "Memory pressure: " << config.memory_pressure_mb << " MB" << std::endl;
    
    const size_t TOTAL_MB = config.memory_pressure_mb;
    const size_t CHUNK_MB = 10;
    const int NUM_CHUNKS = TOTAL_MB / CHUNK_MB;
    
    std::vector<void*> allocations;
    allocations.reserve(NUM_CHUNKS);
    
    test_start = std::chrono::steady_clock::now();
    size_t allocated_mb = 0;
    bool failed = false;
    
    // Allocate memory gradually
    for (int i = 0; i < NUM_CHUNKS && shouldContinue(); i++) {
        size_t size = CHUNK_MB * 1024 * 1024;
        void* ptr = memory_manager->allocate(size);
        
        if (ptr) {
            // Touch memory to force allocation
            memset(ptr, i & 0xFF, size);
            allocations.push_back(ptr);
            allocated_mb += CHUNK_MB;
            
            // Randomly free some allocations
            if (dist(gen) % 10 < 3 && !allocations.empty()) {
                int idx = dist(gen) % allocations.size();
                memory_manager->deallocate(allocations[idx]);
                allocations.erase(allocations.begin() + idx);
                allocated_mb -= CHUNK_MB;
            }
        } else {
            failed = true;
            errors_detected++;
        }
        
        injectError();
        
        // Check memory usage
        auto stats = memory_manager->get_stats();
        double usage_mb = stats.current_usage / (1024.0 * 1024.0);
        
        if (i % 10 == 0) {
            std::cout << "  Memory usage: " << usage_mb << " MB / " 
                      << allocated_mb << " MB allocated" << std::endl;
        }
        
        // Ensure we don't exceed memory limit
        EXPECT_LT(usage_mb, total_ram_mb * 0.8);
    }
    
    // Clean up
    for (void* ptr : allocations) {
        memory_manager->deallocate(ptr);
    }
    allocations.clear();
    
    std::cout << "  Memory stress test completed. "
              << "Peak allocated: " << allocated_mb << " MB, "
              << "Errors: " << errors_detected << std::endl;
}

// ============================================================
// TEST: CPU Load
// ============================================================

TEST_F(StressTest, CPULoad) {
    std::cout << "\n=== CPU Load Stress Test ===" << std::endl;
    std::cout << "Duration: " << config.duration_seconds / 2 << " seconds" << std::endl;
    
    const int NUM_THREADS = std::thread::hardware_concurrency();
    std::cout << "CPU threads: " << NUM_THREADS << std::endl;
    
    test_start = std::chrono::steady_clock::now();
    std::atomic<bool> running(true);
    std::atomic<int> iterations(0);
    
    // Create CPU-bound worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < NUM_THREADS; i++) {
        workers.emplace_back([&]() {
            while (running && shouldContinue()) {
                // Heavy computation
                volatile double result = 0;
                for (int j = 0; j < 100000; j++) {
                    result += std::sin(j * 0.001) * std::cos(j * 0.001);
                }
                iterations++;
                
                // Inject errors
                if (dist(gen) % 100 < config.error_injection_rate) {
                    // Simulate CPU stress induced error
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }
    
    // Monitor CPU usage
    int last_iterations = 0;
    while (shouldContinue()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        int current = iterations.load();
        int rate = current - last_iterations;
        std::cout << "  CPU iterations: " << current 
                  << " (" << rate << " per 5s)" << std::endl;
        last_iterations = current;
    }
    
    running = false;
    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    
    std::cout << "  CPU stress test completed. "
              << "Total iterations: " << iterations << std::endl;
}

// ============================================================
// TEST: Error Recovery
// ============================================================

TEST_F(StressTest, ErrorRecovery) {
    std::cout << "\n=== Error Recovery Stress Test ===" << std::endl;
    std::cout << "Error injection rate: " << config.error_injection_rate << "%" << std::endl;
    
    // Enable error injection for this test
    config.error_injection_rate = 10;
    
    // Initialize components
    VideoRecorder::VideoConfig video_config;
    video_config.width = 640;
    video_config.height = 480;
    video_config.framerate = 15;
    video_config.bitrate = 1000000;
    video_config.codec = VideoRecorder::Codec::H264;
    video_config.hardware_acceleration = false;
    
    AudioRecorder::AudioConfig audio_config;
    audio_config.sample_rate = 44100;
    audio_config.channels = 1;
    audio_config.bitrate = 64000;
    audio_config.codec = AudioRecorder::Codec::AAC;
    
    recording_manager = std::make_unique<RecordingManager>();
    EXPECT_TRUE(recording_manager->initialize(video_config, audio_config));
    recording_manager->setOutputDirectory(test_dir + "/recordings");
    recording_manager->setAutoCombine(true);
    
    test_start = std::chrono::steady_clock::now();
    int recovery_attempts = 0;
    int successful_recoveries = 0;
    
    while (shouldContinue()) {
        // Start recording
        bool started = recording_manager->startRecording("recovery_test");
        
        if (started) {
            // Record for a short period
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Simulate an error
            if (dist(gen) % 100 < config.error_injection_rate) {
                recording_manager->stopRecording();
                errors_detected++;
            }
            
            // Try to recover
            recovery_attempts++;
            if (!recording_manager->isRecording()) {
                // Attempt to restart
                started = recording_manager->startRecording("recovery_test");
                if (started) {
                    successful_recoveries++;
                }
            }
        }
        
        injectError();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    recording_manager->stopRecording();
    
    std::cout << "  Error recovery test completed." << std::endl;
    std::cout << "  Recovery attempts: " << recovery_attempts << std::endl;
    std::cout << "  Successful recoveries: " << successful_recoveries << std::endl;
    std::cout << "  Errors detected: " << errors_detected << std::endl;
    
    EXPECT_GT(successful_recoveries, 0);
}

// ============================================================
// TEST: Stress Test Summary
// ============================================================

TEST_F(StressTest, StressTestSummary) {
    std::cout << "\n=== Stress Test Summary ===" << std::endl;
    std::cout << "Platform: Raspberry Pi 5 (ARM Cortex-A76)" << std::endl;
    std::cout << "Test duration: " << config.duration_seconds << " seconds" << std::endl;
    std::cout << std::endl;
    
    // Run all stress tests
    LongDurationRecording();
    ConcurrentOperations();
    MemoryPressure();
    CPULoad();
    ErrorRecovery();
    
    // Print summary
    std::cout << "\n=== Stress Test Results ===" << std::endl;
    std::cout << "Total errors: " << errors_detected << std::endl;
    
    // Generate report
    std::string report_path = test_dir + "/stress_report.txt";
    std::ofstream report(report_path);
    report << "Stress Test Report" << std::endl;
    report << "==================" << std::endl;
    report << "Platform: Raspberry Pi 5 (ARM Cortex-A76)" << std::endl;
    report << "Duration: " << config.duration_seconds << " seconds" << std::endl;
    report << "Errors: " << errors_detected << std::endl;
    report.close();
    
    std::cout << "Report saved to: " << report_path << std::endl;
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Enable stress test mode
    ::testing::GTEST_FLAG(filter) = "StressTest.*";
    ::testing::GTEST_FLAG(repeat) = 1;
    ::testing::GTEST_FLAG(break_on_failure) = false;
    
    return RUN_ALL_TESTS();
}
