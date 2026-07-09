/**
 * @file benchmark.cpp
 * @brief Performance Benchmark Tests for RPi5 Recording Station
 * 
 * Benchmarks system performance including:
 * - Memory allocation speed
 * - File I/O performance
 * - Video encoding/decoding
 * - Audio processing
 * - Network throughput
 * - Storage operations
 * - CPU performance
 * - GPU acceleration
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <iomanip>

#include "../../src/core/MemoryManager.hpp"
#include "../../src/core/StorageMonitor.hpp"
#include "../../src/recording/VideoRecorder.hpp"
#include "../../src/recording/AudioRecorder.hpp"
#include "../../src/recording/encoders/H264Encoder.hpp"
#include "../../src/recording/encoders/AACEncoder.hpp"
#include "../../src/network/NetworkManager.hpp"
#include "../../src/hardware/uart/PL011UART.hpp"
#include "../../src/utils/Logger.hpp"
#include "../../src/utils/TimeUtils.hpp"

namespace fs = std::filesystem;

// ============================================================
// BENCHMARK RESULT STRUCTURES
// ============================================================

struct BenchmarkResult {
    std::string name;
    double duration_ms;
    double throughput;
    std::string unit;
    double cpu_usage;
    double memory_usage_mb;
    std::map<std::string, double> metrics;
    
    std::string toString() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << name << ": " << duration_ms << "ms";
        if (throughput > 0) {
            ss << ", " << throughput << " " << unit;
        }
        if (cpu_usage > 0) {
            ss << ", CPU: " << cpu_usage << "%";
        }
        if (memory_usage_mb > 0) {
            ss << ", Memory: " << memory_usage_mb << "MB";
        }
        for (const auto& [key, value] : metrics) {
            ss << ", " << key << ": " << std::fixed << std::setprecision(2) << value;
        }
        return ss.str();
    }
};

// ============================================================
// BENCHMARK FIXTURE
// ============================================================

class PerformanceBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/benchmark_test_" + std::to_string(time(nullptr));
        fs::create_directories(test_dir);
        fs::create_directories(test_dir + "/recordings");
        fs::create_directories(test_dir + "/temp");
        
        // Initialize logger (quiet mode)
        Logger::Config log_config;
        log_config.level = Logger::Level::ERROR;
        log_config.console_enabled = false;
        logger.initialize(log_config);
        
        // Get system info
        sys_pages = sysconf(_SC_PHYS_PAGES);
        sys_page_size = sysconf(_SC_PAGE_SIZE);
        total_ram_mb = (sys_pages * sys_page_size) / (1024 * 1024);
        
        logger.info("Benchmark setup complete. RAM: " + std::to_string(total_ram_mb) + "MB");
    }
    
    void TearDown() override {
        fs::remove_all(test_dir);
        logger.info("Benchmark cleanup complete");
    }
    
    // Benchmark helper functions
    template<typename Func>
    BenchmarkResult measure(const std::string& name, Func func, int iterations = 1) {
        BenchmarkResult result;
        result.name = name;
        
        // Warm up
        func();
        
        // Measure
        auto start_time = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; i++) {
            func();
        }
        auto end_time = std::chrono::steady_clock::now();
        
        result.duration_ms = std::chrono::duration<double, std::milli>(
            end_time - start_time).count() / iterations;
        
        return result;
    }
    
    template<typename Func>
    BenchmarkResult measureThroughput(const std::string& name, Func func, 
                                      size_t data_size, int iterations = 1) {
        auto result = measure(name, func, iterations);
        result.throughput = (data_size * iterations) / (result.duration_ms / 1000.0);
        result.unit = "MB/s";
        if (result.throughput > 1024) {
            result.throughput /= 1024;
            result.unit = "GB/s";
        }
        return result;
    }
    
    std::string test_dir;
    Logger logger;
    long sys_pages;
    long sys_page_size;
    size_t total_ram_mb;
    std::vector<BenchmarkResult> results;
    
    // Benchmark results storage
    void addResult(const BenchmarkResult& result) {
        results.push_back(result);
        std::cout << "  " << result.toString() << std::endl;
    }
};

// ============================================================
// TEST: Memory Allocation Benchmarks
// ============================================================

TEST_F(PerformanceBenchmark, MemoryAllocationBenchmark) {
    std::cout << "\n=== Memory Allocation Benchmarks ===" << std::endl;
    
    MemoryManager& mem_mgr = MemoryManager::getInstance();
    mem_mgr.reset_stats();
    
    // Test small allocations (1KB)
    auto small_result = measureThroughput("Small allocations (1KB)", [&]() {
        void* ptr = mem_mgr.allocate(1024);
        mem_mgr.deallocate(ptr);
    }, 1024, 10000);
    addResult(small_result);
    
    // Test medium allocations (1MB)
    auto medium_result = measureThroughput("Medium allocations (1MB)", [&]() {
        void* ptr = mem_mgr.allocate(1024 * 1024);
        mem_mgr.deallocate(ptr);
    }, 1024 * 1024, 100);
    addResult(medium_result);
    
    // Test large allocations (10MB)
    auto large_result = measureThroughput("Large allocations (10MB)", [&]() {
        void* ptr = mem_mgr.allocate(10 * 1024 * 1024);
        mem_mgr.deallocate(ptr);
    }, 10 * 1024 * 1024, 10);
    addResult(large_result);
    
    // Test aligned allocations
    auto aligned_result = measureThroughput("Aligned allocations (4KB)", [&]() {
        void* ptr = mem_mgr.allocate_aligned(4096, 4096);
        mem_mgr.deallocate(ptr);
    }, 4096, 10000);
    addResult(aligned_result);
    
    // Test memory pool allocations
    int pool_id = 1;
    mem_mgr.create_memory_pool(10 * 1024 * 1024, pool_id);
    
    auto pool_result = measureThroughput("Pool allocations (1KB)", [&]() {
        void* ptr = mem_mgr.allocate_from_pool(1024, pool_id);
        mem_mgr.free_to_pool(ptr, pool_id);
    }, 1024, 10000);
    addResult(pool_result);
    
    mem_mgr.destroy_memory_pool(pool_id);
    
    // Test ZRAM compression ratio
    if (mem_mgr.is_zram_enabled()) {
        size_t original_size = 10 * 1024 * 1024;
        void* ptr = mem_mgr.allocate(original_size);
        memset(ptr, 0xAA, original_size); // Fill with pattern
        
        auto stats = mem_mgr.get_stats();
        double compression_ratio = (double)original_size / stats.current_usage;
        
        BenchmarkResult zram_result;
        zram_result.name = "ZRAM compression ratio";
        zram_result.throughput = compression_ratio;
        zram_result.unit = "x";
        addResult(zram_result);
        
        mem_mgr.deallocate(ptr);
    }
}

// ============================================================
// TEST: File I/O Benchmarks
// ============================================================

TEST_F(PerformanceBenchmark, FileIOBenchmark) {
    std::cout << "\n=== File I/O Benchmarks ===" << std::endl;
    
    const size_t TEST_SIZE = 100 * 1024 * 1024; // 100MB
    std::string test_file = test_dir + "/test_file.bin";
    
    // Generate test data
    std::vector<uint8_t> test_data(TEST_SIZE);
    for (size_t i = 0; i < TEST_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Write benchmark
    auto write_result = measureThroughput("Sequential write", [&]() {
        std::ofstream file(test_file, std::ios::binary);
        file.write(reinterpret_cast<const char*>(test_data.data()), TEST_SIZE);
        file.close();
    }, TEST_SIZE);
    addResult(write_result);
    
    // Read benchmark
    auto read_result = measureThroughput("Sequential read", [&]() {
        std::vector<uint8_t> buffer(TEST_SIZE);
        std::ifstream file(test_file, std::ios::binary);
        file.read(reinterpret_cast<char*>(buffer.data()), TEST_SIZE);
        file.close();
    }, TEST_SIZE);
    addResult(read_result);
    
    // Random read benchmark
    auto random_read_result = measureThroughput("Random read (4KB blocks)", [&]() {
        std::ifstream file(test_file, std::ios::binary);
        const int BLOCK_SIZE = 4096;
        std::vector<uint8_t> buffer(BLOCK_SIZE);
        
        for (int i = 0; i < 1000; i++) {
            size_t offset = (rand() % (TEST_SIZE / BLOCK_SIZE)) * BLOCK_SIZE;
            file.seekg(offset);
            file.read(reinterpret_cast<char*>(buffer.data()), BLOCK_SIZE);
        }
        file.close();
    }, 1000 * 4096, 10);
    addResult(random_read_result);
    
    // Delete test file
    fs::remove(test_file);
}

// ============================================================
// TEST: Video Encoding Benchmarks
// ============================================================

TEST_F(PerformanceBenchmark, VideoEncodingBenchmark) {
    std::cout << "\n=== Video Encoding Benchmarks ===" << std::endl;
    
    const int WIDTH = 640;
    const int HEIGHT = 480;
    const int FRAMES = 100;
    const int FRAMERATE = 30;
    
    VideoRecorder recorder;
    VideoRecorder::VideoConfig config;
    config.width = WIDTH;
    config.height = HEIGHT;
    config.framerate = FRAMERATE;
    config.bitrate = 2000000;
    config.codec = VideoRecorder::Codec::H264;
    config.container = VideoRecorder::Container::MP4;
    config.hardware_acceleration = false;
    
    EXPECT_TRUE(recorder.initialize(config));
    
    // Generate test frames
    std::vector<std::vector<uint8_t>> frames(FRAMES);
    for (int i = 0; i < FRAMES; i++) {
        frames[i].assign(WIDTH * HEIGHT * 3 / 2, 128);
        // Add some pattern variation
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                int idx = y * WIDTH + x;
                frames[i][idx] = (x + i * 2) % 256;
            }
        }
    }
    
    // Benchmark encoding
    auto encode_result = measureThroughput("H.264 encoding", [&]() {
        std::string output = test_dir + "/recordings/benchmark.mp4";
        recorder.startRecording(output);
        
        for (int i = 0; i < FRAMES; i++) {
            VideoRecorder::VideoFrame frame;
            frame.data = frames[i];
            frame.width = WIDTH;
            frame.height = HEIGHT;
            frame.pts = i * 1000000 / FRAMERATE;
            frame.is_keyframe = (i % 30 == 0);
            recorder.writeFrame(frame);
        }
        
        recorder.stopRecording();
    }, FRAMES * WIDTH * HEIGHT * 3 / 2);
    addResult(encode_result);
    
    // Test with hardware acceleration
    config.hardware_acceleration = true;
    VideoRecorder hw_recorder;
    
    if (hw_recorder.initialize(config)) {
        auto hw_result = measureThroughput("H.264 encoding (HW)", [&]() {
            std::string output = test_dir + "/recordings/benchmark_hw.mp4";
            hw_recorder.startRecording(output);
            
            for (int i = 0; i < FRAMES; i++) {
                VideoRecorder::VideoFrame frame;
                frame.data = frames[i];
                frame.width = WIDTH;
                frame.height = HEIGHT;
                frame.pts = i * 1000000 / FRAMERATE;
                frame.is_keyframe = (i % 30 == 0);
                hw_recorder.writeFrame(frame);
            }
            
            hw_recorder.stopRecording();
        }, FRAMES * WIDTH * HEIGHT * 3 / 2);
        addResult(hw_result);
        hw_recorder.shutdown();
    } else {
        std::cout << "  Hardware acceleration not available" << std::endl;
    }
    
    recorder.shutdown();
    
    // Benchmark H.264 encoder directly
    H264Encoder encoder;
    H264Encoder::Config enc_config;
    enc_config.width = WIDTH;
    enc_config.height = HEIGHT;
    enc_config.framerate = FRAMERATE;
    enc_config.bitrate = 2000000;
    enc_config.preset = H264Encoder::Preset::FAST;
    enc_config.hardware_acceleration = false;
    
    if (encoder.initialize(enc_config)) {
        auto enc_result = measureThroughput("H.264 raw encoding", [&]() {
            for (int i = 0; i < FRAMES; i++) {
                encoder.encodeFrame(frames[i], i * 1000000 / FRAMERATE);
            }
            encoder.flush();
        }, FRAMES * WIDTH * HEIGHT * 3 / 2);
        addResult(enc_result);
        encoder.shutdown();
    }
}

// ============================================================
// TEST: Audio Encoding Benchmarks
// ============================================================

TEST_F(PerformanceBenchmark, AudioEncodingBenchmark) {
    std::cout << "\n=== Audio Encoding Benchmarks ===" << std::endl;
    
    const int SAMPLE_RATE = 48000;
    const int CHANNELS = 2;
    const int DURATION = 10; // seconds
    const int TOTAL_SAMPLES = SAMPLE_RATE * CHANNELS * DURATION;
    
    // Generate test audio
    std::vector<int16_t> audio_data(TOTAL_SAMPLES);
    for (int i = 0; i < TOTAL_SAMPLES; i++) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float sample = 16384.0f * std::sin(2.0f * M_PI * 440.0f * t);
        audio_data[i] = static_cast<int16_t>(sample);
    }
    
    AudioRecorder recorder;
    AudioRecorder::AudioConfig config;
    config.sample_rate = SAMPLE_RATE;
    config.channels = CHANNELS;
    config.bitrate = 128000;
    config.codec = AudioRecorder::Codec::AAC;
    config.container = AudioRecorder::Container::MP4;
    
    EXPECT_TRUE(recorder.initialize(config));
    
    // Benchmark encoding
    auto encode_result = measureThroughput("AAC encoding", [&]() {
        std::string output = test_dir + "/recordings/audio_benchmark.mp4";
        recorder.startRecording(output);
        
        const int CHUNK_SIZE = 1024 * CHANNELS;
        for (int i = 0; i < TOTAL_SAMPLES; i += CHUNK_SIZE) {
            int remaining = std::min(CHUNK_SIZE, TOTAL_SAMPLES - i);
            std::vector<int16_t> chunk(audio_data.begin() + i, 
                                       audio_data.begin() + i + remaining);
            recorder.writeAudio(chunk);
        }
        
        recorder.stopRecording();
    }, TOTAL_SAMPLES * sizeof(int16_t));
    addResult(encode_result);
    
    recorder.shutdown();
    
    // Benchmark AAC encoder directly
    AACEncoder aac_encoder;
    AACEncoder::Config aac_config;
    aac_config.sample_rate = SAMPLE_RATE;
    aac_config.channels = CHANNELS;
    aac_config.bitrate = 128000;
    aac_config.profile = AACEncoder::Profile::AAC_LC;
    
    if (aac_encoder.initialize(aac_config)) {
        auto aac_result = measureThroughput("AAC raw encoding", [&]() {
            const int CHUNK = 1024;
            for (int i = 0; i < TOTAL_SAMPLES; i += CHUNK) {
                int remaining = std::min(CHUNK, TOTAL_SAMPLES - i);
                std::vector<int16_t> chunk(audio_data.begin() + i, 
                                           audio_data.begin() + i + remaining);
                aac_encoder.encodeAudio(chunk, i / SAMPLE_RATE);
            }
            aac_encoder.flush();
        }, TOTAL_SAMPLES * sizeof(int16_t));
        addResult(aac_result);
        aac_encoder.shutdown();
    }
}

// ============================================================
// TEST: Network Benchmarks
// ============================================================

TEST_F(PerformanceBenchmark, NetworkBenchmark) {
    std::cout << "\n=== Network Benchmarks ===" << std::endl;
    
    NetworkManager& nm = NetworkManager::getInstance();
    nm.initialize();
    
    auto interfaces = nm.getInterfaces();
    if (interfaces.empty()) {
        std::cout << "  No network interfaces found" << std::endl;
        return;
    }
    
    auto default_iface = nm.getDefaultInterface();
    if (!default_iface.is_up || !default_iface.has_ip) {
        std::cout << "  Default interface not available" << std::endl;
        return;
    }
    
    // Test latency to various hosts
    std::vector<std::string> hosts = {"8.8.8.8", "1.1.1.1", "4.4.4.4"};
    std::vector<double> latencies;
    
    for (const auto& host : hosts) {
        auto start = std::chrono::steady_clock::now();
        bool success = nm.ping(host, 1, 1000);
        auto end = std::chrono::steady_clock::now();
        
        if (success) {
            double latency_ms = std::chrono::duration<double, std::milli>(
                end - start).count();
            latencies.push_back(latency_ms);
        }
    }
    
    if (!latencies.empty()) {
        double avg_latency = 0;
        for (double l : latencies) avg_latency += l;
        avg_latency /= latencies.size();
        
        BenchmarkResult latency_result;
        latency_result.name = "Network latency";
        latency_result.throughput = avg_latency;
        latency_result.unit = "ms";
        addResult(latency_result);
    }
    
    // Test connection quality
    auto quality = nm.getConnectionQuality();
    
    BenchmarkResult quality_result;
    quality_result.name = "Connection quality";
    quality_result.throughput = quality.quality_percent;
    quality_result.unit = "%";
    quality_result.metrics["latency"] = quality.latency_ms;
    quality_result.metrics["packet_loss"] = quality.packet_loss;
    quality_result.metrics["jitter"] = quality.jitter_ms;
    addResult(quality_result);
}

// ============================================================
// TEST: Storage Benchmarks
// ============================================================

TEST_F(PerformanceBenchmark, StorageBenchmark) {
    std::cout << "\n=== Storage Benchmarks ===" << std::endl;
    
    StorageMonitor storage_monitor(test_dir);
    
    // Create many small files
    const int NUM_FILES = 1000;
    auto create_start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < NUM_FILES; i++) {
        std::string path = test_dir + "/file_" + std::to_string(i) + ".txt";
        std::ofstream file(path);
        file << "Test content " << i << std::endl;
        file.close();
    }
    
    auto create_end = std::chrono::steady_clock::now();
    double create_ms = std::chrono::duration<double, std::milli>(
        create_end - create_start).count();
    
    BenchmarkResult create_result;
    create_result.name = "File creation (" + std::to_string(NUM_FILES) + " files)";
    create_result.duration_ms = create_ms;
    create_result.throughput = NUM_FILES / (create_ms / 1000.0);
    create_result.unit = "files/s";
    addResult(create_result);
    
    // Scan benchmark
    auto scan_result = measure("Directory scan", [&]() {
        storage_monitor.scan_directory();
    });
    scan_result.unit = "files";
    scan_result.throughput = storage_monitor.get_file_count();
    addResult(scan_result);
    
    // Export benchmark
    auto export_result = measure("Storage export (JSON)", [&]() {
        storage_monitor.export_to_json(test_dir + "/export.json");
    });
    addResult(export_result);
    
    // Clean up
    fs::remove_all(test_dir + "/export.json");
}

// ============================================================
// TEST: CPU Performance
// ============================================================

TEST_F(PerformanceBenchmark, CPUBenchmark) {
    std::cout << "\n=== CPU Performance Benchmarks ===" << std::endl;
    
    // Integer arithmetic benchmark
    auto int_result = measureThroughput("Integer arithmetic (100M ops)", [&]() {
        volatile int sum = 0;
        for (int i = 0; i < 100000000; i++) {
            sum += i;
        }
    }, 100000000 * sizeof(int));
    addResult(int_result);
    
    // Floating point benchmark
    auto float_result = measureThroughput("Floating point (10M ops)", [&]() {
        volatile double sum = 0.0;
        for (int i = 0; i < 10000000; i++) {
            sum += std::sin(i * 0.001);
        }
    }, 10000000 * sizeof(double));
    addResult(float_result);
    
    // Memory copy benchmark
    const size_t COPY_SIZE = 100 * 1024 * 1024;
    std::vector<uint8_t> src(COPY_SIZE, 0xAA);
    std::vector<uint8_t> dst(COPY_SIZE);
    
    auto copy_result = measureThroughput("Memory copy (100MB)", [&]() {
        memcpy(dst.data(), src.data(), COPY_SIZE);
    }, COPY_SIZE, 10);
    addResult(copy_result);
    
    // Memory bandwidth (read)
    auto read_result = measureThroughput("Memory read (100MB)", [&]() {
        volatile uint8_t sum = 0;
        for (size_t i = 0; i < COPY_SIZE; i++) {
            sum += src[i];
        }
    }, COPY_SIZE, 10);
    addResult(read_result);
}

// ============================================================
// TEST: GPU Performance (if available)
// ============================================================

TEST_F(PerformanceBenchmark, GPUAccelerationBenchmark) {
    std::cout << "\n=== GPU Acceleration Benchmarks ===" << std::endl;
    
    // Check for hardware acceleration support
    VideoRecorder recorder;
    VideoRecorder::VideoConfig config;
    config.width = 640;
    config.height = 480;
    config.framerate = 30;
    config.bitrate = 2000000;
    config.codec = VideoRecorder::Codec::H264;
    config.hardware_acceleration = true;
    
    bool hw_available = recorder.initialize(config);
    
    if (!hw_available) {
        std::cout << "  Hardware acceleration not available" << std::endl;
        return;
    }
    
    // Generate test frames
    const int FRAMES = 50;
    std::vector<std::vector<uint8_t>> frames(FRAMES);
    for (int i = 0; i < FRAMES; i++) {
        frames[i].assign(640 * 480 * 3 / 2, 128);
    }
    
    // Benchmark hardware encoding
    auto hw_result = measureThroughput("GPU H.264 encoding", [&]() {
        std::string output = test_dir + "/recordings/gpu_test.mp4";
        recorder.startRecording(output);
        
        for (int i = 0; i < FRAMES; i++) {
            VideoRecorder::VideoFrame frame;
            frame.data = frames[i];
            frame.width = 640;
            frame.height = 480;
            frame.pts = i * 1000000 / 30;
            frame.is_keyframe = (i % 30 == 0);
            recorder.writeFrame(frame);
        }
        
        recorder.stopRecording();
    }, FRAMES * 640 * 480 * 3 / 2);
    addResult(hw_result);
    
    // Compare with software
    recorder.shutdown();
    
    VideoRecorder sw_recorder;
    config.hardware_acceleration = false;
    
    if (sw_recorder.initialize(config)) {
        auto sw_result = measureThroughput("Software H.264 encoding", [&]() {
            std::string output = test_dir + "/recordings/sw_test.mp4";
            sw_recorder.startRecording(output);
            
            for (int i = 0; i < FRAMES; i++) {
                VideoRecorder::VideoFrame frame;
                frame.data = frames[i];
                frame.width = 640;
                frame.height = 480;
                frame.pts = i * 1000000 / 30;
                frame.is_keyframe = (i % 30 == 0);
                sw_recorder.writeFrame(frame);
            }
            
            sw_recorder.stopRecording();
        }, FRAMES * 640 * 480 * 3 / 2);
        addResult(sw_result);
        sw_recorder.shutdown();
    }
}

// ============================================================
// TEST: System Performance Summary
// ============================================================

TEST_F(PerformanceBenchmark, PerformanceSummary) {
    std::cout << "\n=== System Performance Summary ===" << std::endl;
    std::cout << "Platform: Raspberry Pi 5 (ARM Cortex-A76)" << std::endl;
    std::cout << "Total RAM: " << total_ram_mb << " MB" << std::endl;
    std::cout << std::endl;
    
    // Run all benchmarks
    MemoryAllocationBenchmark();
    FileIOBenchmark();
    VideoEncodingBenchmark();
    AudioEncodingBenchmark();
    NetworkBenchmark();
    StorageBenchmark();
    CPUBenchmark();
    GPUAccelerationBenchmark();
    
    // Print summary
    std::cout << "\n=== Summary Results ===" << std::endl;
    for (const auto& result : results) {
        std::cout << "  " << result.toString() << std::endl;
    }
    
    // Generate report file
    std::string report_path = test_dir + "/benchmark_report.txt";
    std::ofstream report(report_path);
    report << "Performance Benchmark Report" << std::endl;
    report << "=============================" << std::endl;
    report << "Platform: Raspberry Pi 5 (ARM Cortex-A76)" << std::endl;
    report << "Total RAM: " << total_ram_mb << " MB" << std::endl;
    report << "Date: " << TimeUtils::format(TimeUtils::now()) << std::endl;
    report << std::endl;
    
    for (const auto& result : results) {
        report << result.toString() << std::endl;
    }
    report.close();
    
    std::cout << "\nReport saved to: " << report_path << std::endl;
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
