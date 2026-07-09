#ifndef PCIE_DRIVER_HPP
#define PCIE_DRIVER_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <cstdint>
#include <chrono>

/**
 * @brief PCIe Driver for Raspberry Pi 5
 * 
 * Provides PCIe device management with support for NVMe SSDs,
 * SATA controllers, and other PCIe peripherals.
 * Optimized for BCM2712 on Raspberry Pi 5.
 */
class PCIeDriver {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class DeviceType {
        UNKNOWN,
        NVME_SSD,
        SATA_CONTROLLER,
        USB_CONTROLLER,
        NETWORK_CONTROLLER,
        AUDIO_CONTROLLER,
        VIDEO_CONTROLLER,
        PCIE_BRIDGE
    };
    
    enum class DeviceState {
        UNKNOWN,
        DETECTED,
        INITIALIZED,
        ACTIVE,
        ERROR,
        REMOVED
    };
    
    enum class Speed {
        GEN1 = 1,   // 2.5 GT/s
        GEN2 = 2,   // 5 GT/s
        GEN3 = 3,   // 8 GT/s
        GEN4 = 4,   // 16 GT/s
        GEN5 = 5    // 32 GT/s
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief PCIe device information
     */
    struct DeviceInfo {
        uint16_t vendor_id = 0;
        uint16_t device_id = 0;
        uint8_t bus = 0;
        uint8_t slot = 0;
        uint8_t function = 0;
        std::string vendor_name = "";
        std::string device_name = "";
        DeviceType type = DeviceType::UNKNOWN;
        DeviceState state = DeviceState::UNKNOWN;
        Speed speed = Speed::GEN1;
        uint32_t max_payload_size = 256;  // bytes
        uint32_t max_read_request_size = 512;  // bytes
        uint64_t memory_base = 0;
        uint64_t memory_limit = 0;
        std::vector<uint32_t> bar_addresses;  // Base Address Registers
        std::vector<uint32_t> bar_sizes;
        std::string driver = "";
        std::string device_path = "";
        bool is_active = false;
        uint64_t bytes_transferred = 0;
    };
    
    /**
     * @brief NVMe specific information
     */
    struct NVMeInfo {
        std::string model = "";
        std::string firmware_version = "";
        std::string serial_number = "";
        uint64_t total_capacity = 0;  // bytes
        uint64_t used_capacity = 0;   // bytes
        uint64_t free_capacity = 0;   // bytes
        uint32_t max_queues = 0;
        uint32_t queue_depth = 0;
        uint32_t sector_size = 512;
        uint32_t max_sectors = 0;
        uint32_t max_segments = 0;
        bool is_nvme = false;
        std::string mount_point = "";
        std::string filesystem = "";
        bool is_mounted = false;
        uint32_t temperature_celsius = 0;
        uint64_t power_on_hours = 0;
        uint64_t total_read_bytes = 0;
        uint64_t total_write_bytes = 0;
        uint32_t read_speed_mb_s = 0;
        uint32_t write_speed_mb_s = 0;
        uint32_t io_operations_per_second = 0;
        uint64_t lifetime_writes = 0;
        uint64_t lifetime_reads = 0;
        uint8_t percentage_used = 0;
        uint8_t critical_warning = 0;
        std::vector<std::string> smart_attributes;
    };
    
    /**
     * @brief PCIe statistics
     */
    struct PCIeStats {
        uint64_t total_bytes_read = 0;
        uint64_t total_bytes_written = 0;
        uint64_t total_transfers = 0;
        uint64_t read_operations = 0;
        uint64_t write_operations = 0;
        uint64_t read_errors = 0;
        uint64_t write_errors = 0;
        uint64_t retry_count = 0;
        double read_speed_mb_s = 0;
        double write_speed_mb_s = 0;
        double average_latency_us = 0;
        uint64_t power_on_hours = 0;
        std::chrono::system_clock::time_point last_activity;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using DeviceDetectedCallback = std::function<void(const DeviceInfo&)>;
    using DeviceRemovedCallback = std::function<void(const DeviceInfo&)>;
    using DeviceChangedCallback = std::function<void(const DeviceInfo&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using SpeedTestCallback = std::function<void(const DeviceInfo&, double read_speed, double write_speed)>;
    
    // ============================================
    // SINGLETON
    // ============================================
    
    static PCIeDriver& getInstance();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize PCIe driver
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Shutdown PCIe driver
     */
    void shutdown();
    
    // ============================================
    // DEVICE MANAGEMENT
    // ============================================
    
    /**
     * @brief Scan for PCIe devices
     * @return Vector of device information
     */
    std::vector<DeviceInfo> scanDevices();
    
    /**
     * @brief Get device information by address
     * @param bus Bus number
     * @param slot Slot number
     * @param function Function number
     * @return Device information
     */
    DeviceInfo getDeviceInfo(uint8_t bus, uint8_t slot, uint8_t function) const;
    
    /**
     * @brief Get device information by path
     * @param device_path Device path (e.g., "/dev/nvme0n1")
     * @return Device information
     */
    DeviceInfo getDeviceInfo(const std::string& device_path) const;
    
    /**
     * @brief Get all devices
     */
    std::vector<DeviceInfo> getAllDevices() const;
    
    /**
     * @brief Get devices by type
     */
    std::vector<DeviceInfo> getDevicesByType(DeviceType type) const;
    
    /**
     * @brief Get NVMe devices
     */
    std::vector<NVMeInfo> getNVMeDevices() const;
    
    /**
     * @brief Get NVMe device by path
     */
    NVMeInfo getNVMeDevice(const std::string& device_path) const;
    
    // ============================================
    // NVMe OPERATIONS
    // ============================================
    
    /**
     * @brief Initialize NVMe device
     * @param device_path Device path (e.g., "/dev/nvme0n1")
     * @return true if successful
     */
    bool initializeNVMe(const std::string& device_path);
    
    /**
     * @brief Mount NVMe device
     * @param device_path Device path
     * @param mount_point Mount point
     * @param filesystem Filesystem type (default: ext4)
     * @return true if successful
     */
    bool mountNVMe(const std::string& device_path, 
                   const std::string& mount_point,
                   const std::string& filesystem = "ext4");
    
    /**
     * @brief Unmount NVMe device
     * @param device_path Device path
     * @return true if successful
     */
    bool unmountNVMe(const std::string& device_path);
    
    /**
     * @brief Format NVMe device
     * @param device_path Device path
     * @param filesystem Filesystem type (default: ext4)
     * @param label Volume label
     * @return true if successful
     */
    bool formatNVMe(const std::string& device_path,
                    const std::string& filesystem = "ext4",
                    const std::string& label = "");
    
    /**
     * @brief Create partition on NVMe
     * @param device_path Device path
     * @param partition_size Size in bytes (0 = all remaining space)
     * @return true if successful
     */
    bool createPartition(const std::string& device_path, uint64_t partition_size = 0);
    
    /**
     * @brief Get NVMe SMART data
     * @param device_path Device path
     * @return SMART data as string
     */
    std::string getNVMeSMART(const std::string& device_path) const;
    
    /**
     * @brief Get NVMe temperature
     * @param device_path Device path
     * @return Temperature in Celsius
     */
    uint32_t getNVMeTemperature(const std::string& device_path) const;
    
    /**
     * @brief Get NVMe health
     * @param device_path Device path
     * @return Health percentage (0-100)
     */
    uint8_t getNVMeHealth(const std::string& device_path) const;
    
    // ============================================
    // PERFORMANCE
    // ============================================
    
    /**
     * @brief Run speed test on device
     * @param device_path Device path
     * @param size_mb Test size in MB
     * @return Speed test results
     */
    struct SpeedTestResult {
        double read_speed_mb_s = 0;
        double write_speed_mb_s = 0;
        double random_read_iops = 0;
        double random_write_iops = 0;
        double latency_us = 0;
        uint64_t total_bytes = 0;
        double duration_seconds = 0;
    };
    
    SpeedTestResult runSpeedTest(const std::string& device_path, size_t size_mb = 1024);
    
    /**
     * @brief Benchmark device
     * @param device_path Device path
     * @param iterations Number of iterations
     * @return Benchmark results
     */
    struct BenchmarkResult {
        double sequential_read_mb_s = 0;
        double sequential_write_mb_s = 0;
        double random_read_mb_s = 0;
        double random_write_mb_s = 0;
        double mixed_workload_mb_s = 0;
        double latency_avg_us = 0;
        double latency_p99_us = 0;
        double iops_read = 0;
        double iops_write = 0;
        std::map<std::string, double> metrics;
    };
    
    BenchmarkResult benchmark(const std::string& device_path, int iterations = 5);
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get PCIe statistics
     */
    PCIeStats getStats(const std::string& device_path) const;
    
    /**
     * @brief Get total PCIe statistics
     */
    PCIeStats getTotalStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats(const std::string& device_path);
    
    /**
     * @brief Reset all statistics
     */
    void resetAllStats();
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnDeviceDetected(DeviceDetectedCallback callback);
    void setOnDeviceRemoved(DeviceRemovedCallback callback);
    void setOnDeviceChanged(DeviceChangedCallback callback);
    void setOnError(ErrorCallback callback);
    void setOnSpeedTest(SpeedTestCallback callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get device type name
     */
    std::string getDeviceTypeName(DeviceType type) const;
    
    /**
     * @brief Get speed name
     */
    std::string getSpeedName(Speed speed) const;
    
    /**
     * @brief Get state name
     */
    std::string getStateName(DeviceState state) const;
    
    /**
     * @brief Check if device is NVMe
     */
    bool isNVMe(const std::string& device_path) const;
    
    /**
     * @brief Check if device is mounted
     */
    bool isMounted(const std::string& device_path) const;
    
    /**
     * @brief Get device capacity in human-readable format
     */
    std::string formatCapacity(uint64_t bytes) const;
    
    /**
     * @brief Get device mount point
     */
    std::string getMountPoint(const std::string& device_path) const;
    
    /**
     * @brief Get device filesystem
     */
    std::string getFilesystem(const std::string& device_path) const;
    
    /**
     * @brief Verify device health
     */
    bool verifyHealth(const std::string& device_path) const;
    
    /**
     * @brief Run diagnostic tests
     */
    struct DiagnosticResult {
        bool passed = false;
        std::string device_path;
        std::string error_message;
        std::map<std::string, bool> tests;
        std::map<std::string, std::string> details;
    };
    
    DiagnosticResult runDiagnostics(const std::string& device_path);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    PCIeDriver();
    ~PCIeDriver();
    PCIeDriver(const PCIeDriver&) = delete;
    PCIeDriver& operator=(const PCIeDriver&) = delete;
    
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    std::map<std::string, DeviceInfo> devices;
    std::map<std::string, PCIeStats> device_stats;
    std::map<std::string, NVMeInfo> nvme_devices;
    
    DeviceDetectedCallback on_device_detected;
    DeviceRemovedCallback on_device_removed;
    DeviceChangedCallback on_device_changed;
    ErrorCallback on_error;
    SpeedTestCallback on_speed_test;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    void scanPCIeBus();
    void scanNVMeDevices();
    DeviceInfo scanPCIDevice(uint8_t bus, uint8_t slot, uint8_t function);
    DeviceType determineDeviceType(uint16_t vendor_id, uint16_t device_id);
    std::string getVendorName(uint16_t vendor_id) const;
    std::string getDeviceName(uint16_t vendor_id, uint16_t device_id) const;
    void parseNVMeInfo(const std::string& device_path, NVMeInfo& info);
    void updateDeviceStats(const std::string& device_path);
    void recordError(const std::string& error);
    void notifyDeviceDetected(const DeviceInfo& info);
    void notifyDeviceRemoved(const DeviceInfo& info);
    void notifyDeviceChanged(const DeviceInfo& info);
    
    // System calls
    bool executeCommand(const std::string& command, std::string& output);
    bool pathExists(const std::string& path) const;
    uint64_t getFileSize(const std::string& path) const;
    bool readFile(const std::string& path, std::string& content) const;
    bool writeFile(const std::string& path, const std::string& content) const;
    std::vector<std::string> listDirectory(const std::string& path) const;
    
    // Device paths
    std::string getSysPath(uint8_t bus, uint8_t slot, uint8_t function) const;
    std::string getNVMePath(const std::string& device) const;
    std::vector<std::string> getBlockDevices() const;
    std::vector<std::string> getNVMeDevicesFromSys() const;
    
    // Helper functions
    bool isPCIeAvailable() const;
    std::string getPCIBusPath() const;
    void delayMicroseconds(int us);
    std::string formatSize(uint64_t bytes) const;
    double calculateAverage(const std::vector<double>& values) const;
    double calculatePercentile(const std::vector<double>& values, double percentile) const;
    
    // Vendor/Device ID database
    struct VendorDevice {
        uint16_t vendor_id;
        uint16_t device_id;
        std::string vendor_name;
        std::string device_name;
        DeviceType type;
    };
    
    static const std::vector<VendorDevice> device_database;
};

#endif // PCIE_DRIVER_HPP
