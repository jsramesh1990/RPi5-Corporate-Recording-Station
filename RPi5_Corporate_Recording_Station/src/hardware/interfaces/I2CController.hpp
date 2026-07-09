#ifndef I2C_CONTROLLER_HPP
#define I2C_CONTROLLER_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <cstdint>

/**
 * @brief I2C Controller for Raspberry Pi 5
 * 
 * Provides I2C bus management with support for multiple buses,
 * device detection, and advanced I2C operations.
 * Optimized for BCM2712 on Raspberry Pi 5.
 */
class I2CController {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Speed {
        STANDARD = 100000,    // 100 kHz
        FAST = 400000,        // 400 kHz
        FAST_PLUS = 1000000,  // 1 MHz
        HIGH_SPEED = 3400000  // 3.4 MHz
    };
    
    enum class BusState {
        CLOSED,
        OPEN,
        ERROR
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief I2C device information
     */
    struct DeviceInfo {
        uint8_t address = 0;
        std::string name = "";
        std::string description = "";
        bool detected = false;
        uint8_t manufacturer = 0;
        uint8_t device_id = 0;
        std::vector<uint8_t> capabilities;
    };
    
    /**
     * @brief I2C bus information
     */
    struct BusInfo {
        int bus_number = 0;
        BusState state = BusState::CLOSED;
        Speed speed = Speed::STANDARD;
        std::vector<DeviceInfo> devices;
        int error_count = 0;
        uint64_t bytes_transferred = 0;
    };
    
    /**
     * @brief I2C transaction
     */
    struct Transaction {
        uint8_t address = 0;
        std::vector<uint8_t> write_data;
        std::vector<uint8_t> read_data;
        bool success = false;
        int error_code = 0;
        uint64_t duration_us = 0;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using DeviceDetectedCallback = std::function<void(int bus, uint8_t address)>;
    using ErrorCallback = std::function<void(int bus, const std::string& error)>;
    using TransactionCallback = std::function<void(const Transaction&)>;
    
    // ============================================
    // SINGLETON
    // ============================================
    
    static I2CController& getInstance();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize I2C controller
     * @param default_bus Default bus number
     * @return true if successful
     */
    bool initialize(int default_bus = 1);
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Shutdown I2C controller
     */
    void shutdown();
    
    // ============================================
    // BUS MANAGEMENT
    // ============================================
    
    /**
     * @brief Open I2C bus
     * @param bus Bus number
     * @param speed Bus speed
     * @return true if successful
     */
    bool openBus(int bus, Speed speed = Speed::STANDARD);
    
    /**
     * @brief Close I2C bus
     * @param bus Bus number
     */
    void closeBus(int bus);
    
    /**
     * @brief Check if bus is open
     */
    bool isBusOpen(int bus) const;
    
    /**
     * @brief Get bus information
     */
    BusInfo getBusInfo(int bus) const;
    
    /**
     * @brief Get all bus information
     */
    std::vector<BusInfo> getAllBusInfo() const;
    
    /**
     * @brief Set bus speed
     */
    bool setBusSpeed(int bus, Speed speed);
    
    /**
     * @brief Get bus speed
     */
    Speed getBusSpeed(int bus) const;
    
    // ============================================
    // DEVICE DETECTION
    // ============================================
    
    /**
     * @brief Scan bus for devices
     * @param bus Bus number
     * @return Vector of device addresses
     */
    std::vector<uint8_t> scanBus(int bus);
    
    /**
     * @brief Scan all buses for devices
     * @return Map of bus to device addresses
     */
    std::map<int, std::vector<uint8_t>> scanAllBuses();
    
    /**
     * @brief Detect device on bus
     * @param bus Bus number
     * @param address Device address
     * @return true if device detected
     */
    bool detectDevice(int bus, uint8_t address);
    
    /**
     * @brief Get device information
     * @param bus Bus number
     * @param address Device address
     * @return Device information
     */
    DeviceInfo getDeviceInfo(int bus, uint8_t address) const;
    
    /**
     * @brief Identify device by address
     * @param address Device address
     * @return Device name or "Unknown"
     */
    std::string identifyDevice(uint8_t address) const;
    
    // ============================================
    // I2C OPERATIONS
    // ============================================
    
    /**
     * @brief Write data to I2C device
     * @param bus Bus number
     * @param address Device address
     * @param data Data to write
     * @param retry_count Number of retries on failure
     * @return true if successful
     */
    bool write(int bus, uint8_t address, const std::vector<uint8_t>& data, int retry_count = 0);
    
    /**
     * @brief Write single byte to I2C device
     * @param bus Bus number
     * @param address Device address
     * @param value Byte to write
     * @return true if successful
     */
    bool writeByte(int bus, uint8_t address, uint8_t value);
    
    /**
     * @brief Write to register
     * @param bus Bus number
     * @param address Device address
     * @param reg Register address
     * @param value Value to write
     * @return true if successful
     */
    bool writeRegister(int bus, uint8_t address, uint8_t reg, uint8_t value);
    
    /**
     * @brief Write to 16-bit register
     * @param bus Bus number
     * @param address Device address
     * @param reg Register address
     * @param value 16-bit value to write
     * @return true if successful
     */
    bool writeRegister16(int bus, uint8_t address, uint8_t reg, uint16_t value);
    
    /**
     * @brief Read data from I2C device
     * @param bus Bus number
     * @param address Device address
     * @param length Number of bytes to read
     * @param retry_count Number of retries on failure
     * @return Read data
     */
    std::vector<uint8_t> read(int bus, uint8_t address, size_t length, int retry_count = 0);
    
    /**
     * @brief Read single byte from I2C device
     * @param bus Bus number
     * @param address Device address
     * @return Byte read (0 on error)
     */
    uint8_t readByte(int bus, uint8_t address);
    
    /**
     * @brief Read from register
     * @param bus Bus number
     * @param address Device address
     * @param reg Register address
     * @return Value read (0 on error)
     */
    uint8_t readRegister(int bus, uint8_t address, uint8_t reg);
    
    /**
     * @brief Read from 16-bit register
     * @param bus Bus number
     * @param address Device address
     * @param reg Register address
     * @return 16-bit value read (0 on error)
     */
    uint16_t readRegister16(int bus, uint8_t address, uint8_t reg);
    
    /**
     * @brief Read multiple registers
     * @param bus Bus number
     * @param address Device address
     * @param start_reg Starting register address
     * @param count Number of registers to read
     * @return Vector of register values
     */
    std::vector<uint8_t> readRegisters(int bus, uint8_t address, uint8_t start_reg, size_t count);
    
    /**
     * @brief Write then read (combined transaction)
     * @param bus Bus number
     * @param address Device address
     * @param write_data Data to write
     * @param read_length Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> writeRead(int bus, uint8_t address, 
                                   const std::vector<uint8_t>& write_data,
                                   size_t read_length);
    
    // ============================================
    // BULK OPERATIONS
    // ============================================
    
    /**
     * @brief Perform multiple I2C transactions
     * @param bus Bus number
     * @param transactions Vector of transactions
     * @return Vector of transaction results
     */
    std::vector<Transaction> bulkTransactions(int bus, const std::vector<Transaction>& transactions);
    
    /**
     * @brief Broadcast to multiple devices
     * @param bus Bus number
     * @param addresses Vector of device addresses
     * @param data Data to send
     * @return Number of successful writes
     */
    size_t broadcast(int bus, const std::vector<uint8_t>& addresses, const std::vector<uint8_t>& data);
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnDeviceDetected(DeviceDetectedCallback callback);
    void setOnError(ErrorCallback callback);
    void setOnTransaction(TransactionCallback callback);
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get transaction statistics
     */
    struct Stats {
        uint64_t total_transactions = 0;
        uint64_t successful_transactions = 0;
        uint64_t failed_transactions = 0;
        uint64_t bytes_written = 0;
        uint64_t bytes_read = 0;
        uint64_t error_count = 0;
        double average_transaction_time_us = 0.0;
    };
    
    Stats getStats(int bus) const;
    Stats getTotalStats() const;
    void resetStats(int bus);
    void resetAllStats();
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Check if device exists at address
     */
    bool deviceExists(int bus, uint8_t address);
    
    /**
     * @brief Get device name from address
     */
    std::string getDeviceName(uint8_t address) const;
    
    /**
     * @brief Format address as hex string
     */
    std::string addressToString(uint8_t address) const;
    
    /**
     * @brief Get speed name
     */
    std::string getSpeedName(Speed speed) const;
    
    /**
     * @brief Get bus state name
     */
    std::string getStateName(BusState state) const;
    
    /**
     * @brief Dump device registers
     * @param bus Bus number
     * @param address Device address
     * @param start_reg Starting register
     * @param count Number of registers
     * @return String with register dump
     */
    std::string dumpRegisters(int bus, uint8_t address, uint8_t start_reg = 0, size_t count = 256);
    
    /**
     * @brief Test I2C device
     * @param bus Bus number
     * @param address Device address
     * @return true if test passes
     */
    bool testDevice(int bus, uint8_t address);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    I2CController();
    ~I2CController();
    I2CController(const I2CController&) = delete;
    I2CController& operator=(const I2CController&) = delete;
    
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    struct BusHandle {
        int bus;
        int fd;
        Speed speed;
        BusState state;
        Stats stats;
        std::vector<DeviceInfo> devices;
    };
    
    std::map<int, BusHandle> buses;
    
    DeviceDetectedCallback on_device_detected;
    ErrorCallback on_error;
    TransactionCallback on_transaction;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    bool openDevice(int bus);
    void closeDevice(int bus);
    bool setSlaveAddress(int bus, uint8_t address);
    void updateDeviceInfo(int bus, uint8_t address);
    void recordTransaction(int bus, const Transaction& transaction);
    void notifyError(int bus, const std::string& error);
    void notifyDeviceDetected(int bus, uint8_t address);
    
    // Device identification database
    struct DeviceDB {
        uint8_t address;
        std::string name;
        std::string description;
        uint8_t manufacturer;
        uint8_t device_id;
    };
    
    static const std::vector<DeviceDB> device_database;
    std::string identifyDeviceFromDB(uint8_t address) const;
    
    // Helper functions
    std::string getDevicePath(int bus) const;
    int getI2CFd(int bus) const;
    bool isBusValid(int bus) const;
    bool isAddressValid(uint8_t address) const;
    void delayMicroseconds(int us);
};

#endif // I2C_CONTROLLER_HPP
