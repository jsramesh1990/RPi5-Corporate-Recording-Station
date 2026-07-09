#include <iostream>
#include <memory>
#include <signal.h>

// Core components
#include "core/MemoryManager.hpp"
#include "core/StorageMonitor.hpp"

// Hardware abstraction
#include "hardware/gpio/GPIOController.hpp"
#include "hardware/uart/PL011UART.hpp"
#include "hardware/interfaces/I2CController.hpp"
#include "hardware/interfaces/SPIInterface.hpp"
#include "hardware/camera/CameraInterface.hpp"
#include "hardware/audio/AudioInterface.hpp"
#include "hardware/pcie/PCIeDriver.hpp"

// Recording
#include "recording/RecordingManager.hpp"
#include "recording/encoders/H264Encoder.hpp"
#include "recording/encoders/AACEncoder.hpp"

// Cloud
#include "cloud/SyncManager.hpp"
#include "cloud/providers/NextcloudClient.hpp"

// UI
#include "ui/web/WebServer.hpp"
#include "ui/console/ConsoleUI.hpp"

// Network
#include "network/UARTConsole.hpp"

// Utils
#include "utils/Logger.hpp"
#include "utils/ConfigParser.hpp"

class RecordingStation {
public:
    RecordingStation() : running(true) {
        logger = std::make_unique<Logger>("RecordingStation");
        logger->info("Initializing Recording Station...");
    }
    
    bool initialize() {
        logger->info("Loading configuration...");
        auto& config = ConfigParser::getInstance();
        if (!config.load("config/app/recording-station.conf")) {
            logger->error("Failed to load configuration");
            return false;
        }
        
        // 1. Initialize hardware in order
        if (!initializeHardware()) return false;
        
        // 2. Initialize application components
        if (!initializeApplication()) return false;
        
        // 3. Start services
        if (!startServices()) return false;
        
        logger->info("✅ All systems initialized successfully!");
        logger->info("🌐 Web Dashboard: http://localhost:8080");
        logger->info("🔌 UART Console: /dev/ttyAMA0 at 115200 baud");
        
        return true;
    }
    
    void run() {
        logger->info("🚀 Recording Station running...");
        
        // Main event loop
        while (running) {
            // Check for UART commands
            if (uart_console->hasData()) {
                auto cmd = uart_console->readLine();
                handleUARTCommand(cmd);
            }
            
            // Check for hardware events
            auto& gpio = GPIOController::getInstance();
            gpio.processInterrupts();
            
            // Update storage statistics every 5 seconds
            static auto last_update = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count() >= 5) {
                updateStatus();
                last_update = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void shutdown() {
        logger->info("🧹 Shutting down...");
        running = false;
        
        // Stop services
        if (recording_manager) recording_manager->stopRecording();
        if (web_server) web_server->stop();
        if (uart_console) uart_console->disable();
        
        // Shutdown hardware
        auto& gpio = GPIOController::getInstance();
        gpio.shutdown();
        
        logger->info("✅ Shutdown complete");
    }
    
private:
    bool running;
    std::unique_ptr<Logger> logger;
    
    // Hardware components
    std::unique_ptr<GPIOController> gpio;
    std::unique_ptr<PL011UART> uart;
    std::unique_ptr<CameraInterface> camera;
    std::unique_ptr<AudioInterface> audio;
    std::unique_ptr<PCIeDriver> pcie;
    
    // Application components
    std::unique_ptr<MemoryManager> memory_manager;
    std::unique_ptr<StorageMonitor> storage_monitor;
    std::unique_ptr<RecordingManager> recording_manager;
    std::unique_ptr<SyncManager> sync_manager;
    std::unique_ptr<WebServer> web_server;
    std::unique_ptr<UARTConsole> uart_console;
    
    bool initializeHardware() {
        logger->info("🔧 Initializing hardware...");
        
        // GPIO
        logger->info("  📟 GPIO...");
        gpio = std::make_unique<GPIOController>();
        if (!gpio->initialize()) {
            logger->error("Failed to initialize GPIO");
            return false;
        }
        
        // Configure GPIO pins
        GPIOController::PinConfig config;
        config.function = GPIOController::OUTPUT;
        config.pull = GPIOController::PULL_NONE;
        config.drive = GPIOController::DRIVE_8MA;
        gpio->configurePin(17, config);  // System ready LED
        gpio->configurePin(27, config);  // Recording LED
        
        // UART (PL011)
        logger->info("  🔌 UART...");
        uart = std::make_unique<PL011UART>();
        if (!uart->initialize("/dev/ttyAMA0", 115200)) {
            logger->error("Failed to initialize UART");
            return false;
        }
        
        // Camera
        logger->info("  📸 Camera...");
        camera = std::make_unique<CameraInterface>();
        CameraInterface::CameraConfig cam_config;
        cam_config.width = 1920;
        cam_config.height = 1080;
        cam_config.framerate = 30;
        if (!camera->initialize("/dev/video0", cam_config)) {
            logger->warn("Camera initialization failed (using USB capture)");
        }
        
        // Audio
        logger->info("  🎤 Audio...");
        audio = std::make_unique<AudioInterface>();
        AudioInterface::AudioConfig audio_config;
        audio_config.sample_rate = 48000;
        audio_config.channels = 2;
        if (!audio->initialize("hw:0,0", audio_config)) {
            logger->warn("Audio initialization failed (using USB audio)");
        }
        
        // PCIe (NVMe)
        logger->info("  💾 PCIe...");
        pcie = std::make_unique<PCIeDriver>();
        if (!pcie->initialize()) {
            logger->warn("PCIe initialization failed (NVMe not detected)");
        }
        
        return true;
    }
    
    bool initializeApplication() {
        logger->info("🚀 Initializing application components...");
        
        // Memory Manager
        memory_manager = std::make_unique<MemoryManager>();
        memory_manager->enablePaging();
        memory_manager->enableZRAM();
        memory_manager->setSwappiness(80);
        
        // Storage Monitor
        storage_monitor = std::make_unique<StorageMonitor>("/var/lib/recording-station");
        storage_monitor->setOnFileAdded([this](const FileMetadata& file) {
            this->onFileAdded(file);
        });
        
        // Recording Manager
        recording_manager = std::make_unique<RecordingManager>();
        recording_manager->setVideoEncoder(std::make_unique<H264Encoder>());
        recording_manager->setAudioEncoder(std::make_unique<AACEncoder>());
        recording_manager->setStoragePath("/var/lib/recording-station/recordings");
        recording_manager->setOnRecordingStarted([this]() {
            this->onRecordingStarted();
        });
        recording_manager->setOnRecordingStopped([this](const std::string& file) {
            this->onRecordingStopped(file);
        });
        
        // Cloud Sync Manager
        sync_manager = std::make_unique<SyncManager>();
        sync_manager->addProvider(std::make_unique<NextcloudClient>());
        sync_manager->setOnSyncComplete([this](const std::string& file) {
            this->onSyncComplete(file);
        });
        
        // UART Console
        uart_console = std::make_unique<UARTConsole>(uart.get());
        uart_console->setCommandHandler([this](const std::string& cmd) {
            return this->handleUARTCommand(cmd);
        });
        
        return true;
    }
    
    bool startServices() {
        logger->info("▶️ Starting services...");
        
        // Start storage monitoring
        storage_monitor->startMonitoring(5);
        
        // Start web server
        web_server = std::make_unique<WebServer>();
        web_server->setStorageMonitor(storage_monitor.get());
        web_server->setRecordingManager(recording_manager.get());
        web_server->setSyncManager(sync_manager.get());
        web_server->start(8080);
        
        // Start UART console
        uart_console->enable();
        uart_console->startProcessing();
        
        // Start camera capture
        camera->startCapture();
        camera->setOnFrameCallback([this](const std::vector<uint8_t>& frame) {
            this->onCameraFrame(frame);
        });
        
        // Start audio capture
        audio->startCapture();
        audio->setOnAudioCallback([this](const std::vector<int16_t>& audio_data) {
            this->onAudioData(audio_data);
        });
        
        // Start PCIe monitoring
        pcie->startMonitoring();
        
        return true;
    }
    
    void onCameraFrame(const std::vector<uint8_t>& frame) {
        if (recording_manager->isRecording()) {
            recording_manager->writeVideoFrame(frame);
        }
    }
    
    void onAudioData(const std::vector<int16_t>& audio_data) {
        if (recording_manager->isRecording()) {
            recording_manager->writeAudioData(audio_data);
        }
    }
    
    void onRecordingStarted() {
        gpio->writePin(27, true);  // Turn on recording LED
        uart_console->writeLine("🎬 Recording started");
        logger->info("Recording started");
    }
    
    void onRecordingStopped(const std::string& file) {
        gpio->writePin(27, false);  // Turn off recording LED
        uart_console->writeLine("⏹️ Recording stopped: " + file);
        logger->info("Recording stopped: " + file);
        
        // Upload to cloud
        sync_manager->uploadFile(file);
    }
    
    void onFileAdded(const FileMetadata& file) {
        logger->info("📄 New file: " + file.filename);
        uart_console->writeLine("📄 New file: " + file.filename);
        
        // Auto-upload new files
        if (file.file_type == ".mp4" || file.file_type == ".wav") {
            sync_manager->uploadFile(file.path);
        }
    }
    
    void onSyncComplete(const std::string& file) {
        logger->info("☁️ Synced: " + file);
        uart_console->writeLine("☁️ Synced: " + file);
    }
    
    std::string handleUARTCommand(const std::string& cmd) {
        if (cmd == "status") {
            return getStatus();
        } else if (cmd == "record") {
            recording_manager->startRecording();
            return "Recording started";
        } else if (cmd == "stop") {
            recording_manager->stopRecording();
            return "Recording stopped";
        } else if (cmd == "upload") {
            sync_manager->syncAll();
            return "Upload started";
        } else if (cmd == "memory") {
            return getMemoryStatus();
        } else if (cmd == "storage") {
            return getStorageStatus();
        } else if (cmd == "help") {
            return getHelp();
        }
        return "Unknown command: " + cmd;
    }
    
    std::string getStatus() {
        return "System: Running, Recording: " + 
               std::string(recording_manager->isRecording() ? "Active" : "Idle");
    }
    
    std::string getMemoryStatus() {
        auto stats = memory_manager->getStats();
        return "Memory: " + std::to_string(stats.current_usage / (1024*1024)) + 
               "MB used, " + std::to_string(stats.peak_usage / (1024*1024)) + "MB peak";
    }
    
    std::string getStorageStatus() {
        auto stats = storage_monitor->getStatistics();
        return "Storage: " + std::to_string(stats.used_bytes / (1024*1024*1024)) + 
               "GB used, " + std::to_string(stats.free_bytes / (1024*1024*1024)) + "GB free";
    }
    
    std::string getHelp() {
        return "Commands: status, record, stop, upload, memory, storage, help";
    }
    
    void updateStatus() {
        // Update system ready LED (blink to indicate health)
        static bool led_state = false;
        led_state = !led_state;
        gpio->writePin(17, led_state);
    }
};

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, [](int) { g_station->shutdown(); });
    signal(SIGTERM, [](int) { g_station->shutdown(); });
    
    // Create and run station
    RecordingStation station;
    g_station = &station;
    
    if (!station.initialize()) {
        return 1;
    }
    
    station.run();
    return 0;
}
