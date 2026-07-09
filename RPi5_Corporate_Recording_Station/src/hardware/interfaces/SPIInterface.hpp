#ifndef SPI_INTERFACE_HPP
#define SPI_INTERFACE_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <cstdint>

/**
 * @brief SPI Interface for Raspberry Pi 5
 * 
 * Provides SPI bus management with support for multiple buses,
 * chip selects, and advanced SPI operations.
 * Optimized for BCM2712 on Raspberry Pi 5.
 */
class SPIInterface {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Mode {
        MODE_0 = 0,  // CPOL=0, CPHA=0
        MODE_1 = 1,  // CPOL=0, CPHA=1
        MODE_2 = 2,  // CPOL=1, CPHA=0
        MODE_3 = 3   // CPOL=1, CPHA=1
    };
    
    enum class BitOrder {
        MSB_FIRST,
        LSB_FIRST
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
     * @brief SPI configuration
     */
    struct SPIConfig {
        int bus = 0;
        int chip_select = 0;
        Mode mode = Mode::MODE_0;
        int speed_hz = 1000000;      // 1 MHz
        int bits_per_word = 8;
        BitOrder bit_order = BitOrder::MSB_FIRST;
        bool lsb_first = false;
        int delay_us = 0;
        int timeout_ms = 1000;
    };
    
    /**
     * @brief SPI transfer
     */
    struct SPITransfer {
        std::vector<uint8_t> tx_data;
        std::vector<uint8_t> rx_data;
        bool success = false;
        int error_code = 0;
        uint64_t duration_us = 0;
    };
    
    /**
     * @brief Bus information
     */
    struct BusInfo {
        int bus = 0;
        BusState state = BusState::CLOSED;
        SPIConfig config;
        int error_count = 0;
        uint64_t bytes_transferred = 0;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using TransferCallback = std::function<void(const SPITransfer&)>;
    using ErrorCallback = std::function<void(int bus, const std::string& error)>;
    
    // ============================================
    // SINGLETON
    // ============================================
    
    static SPIInterface& getInstance();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize SPI interface
     * @param default_bus Default bus number
     * @return true if successful
     */
    bool initialize(int default_bus = 0);
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Shutdown SPI interface
     */
    void shutdown();
    
    // ============================================
    // BUS MANAGEMENT
    // ============================================
    
    /**
     * @brief Open SPI bus
     * @param config SPI configuration
     * @return true if successful
     */
    bool openBus(const SPIConfig& config);
    
    /**
     * @brief Open SPI bus with parameters
     * @param bus Bus number
     * @param chip_select Chip select pin
     * @param speed Speed in Hz
     * @param mode SPI mode
     * @return true if successful
     */
    bool openBus(int bus, int chip_select = 0, int speed = 1000000, Mode mode = Mode::MODE_0);
    
    /**
     * @brief Close SPI bus
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
     * @brief Update bus configuration
     */
    bool updateConfig(int bus, const SPIConfig& config);
    
    // ============================================
    // SPI OPERATIONS
    // ============================================
    
    /**
     * @brief Transfer data over SPI (full duplex)
     * @param bus Bus number
     * @param tx_data Data to send
     * @return Received data
     */
    std::vector<uint8_t> transfer(int bus, const std::vector<uint8_t>& tx_data);
    
    /**
     * @brief Transfer data with configuration
     * @param bus Bus number
     * @param tx_data Data to send
     * @param config SPI configuration (overrides bus config)
     * @return Received data
     */
    std::vector<uint8_t> transfer(int bus, const std::vector<uint8_t>& tx_data, const SPIConfig& config);
    
    /**
     * @brief Write data over SPI
     * @param bus Bus number
     * @param data Data to write
     * @return true if successful
     */
    bool write(int bus, const std::vector<uint8_t>& data);
    
    /**
     * @brief Read data over SPI
     * @param bus Bus number
     * @param length Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> read(int bus, size_t length);
    
    /**
     * @brief Write then read (combined operation)
     * @param bus Bus number
     * @param write_data Data to write
     * @param read_length Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> writeRead(int bus, const std::vector<uint8_t>& write_data, size_t read_length);
    
    /**
     * @brief Transfer with multiple buffers (scatter-gather)
     * @param bus Bus number
     * @param tx_buffers Vector of data buffers to send
     * @return Vector of received data buffers
     */
    std::vector<std::vector<uint8_t>> transferMultiple(int bus, const std::vector<std::vector<uint8_t>>& tx_buffers);
    
    // ============================================
    // CHIP SELECT CONTROL
    // ============================================
    
    /**
     * @brief Select chip
     * @param bus Bus number
     * @param chip_select Chip select pin
     */
    void selectChip(int bus, int chip_select);
    
    /**
     * @brief Deselect chip
     * @param bus Bus number
     * @param chip_select Chip select pin
     */
    void deselectChip(int bus, int chip_select);
    
    /**
     * @brief Check if chip is selected
     */
    bool isChipSelected(int bus) const;
    
    /**
     * @brief Get current chip select
     */
    int getCurrentChipSelect(int bus) const;
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnTransfer(TransferCallback callback);
    void setOnError(ErrorCallback callback);
    
    // ============================================
    // STATISTICS
    // ============================================
    
    struct Stats {
        uint64_t total_transfers = 0;
        uint64_t successful_transfers = 0;
        uint64_t failed_transfers = 0;
        uint64_t bytes_written = 0;
        uint64_t bytes_read = 0;
        uint64_t error_count = 0;
        double average_transfer_time_us = 0.0;
    };
    
    Stats getStats(int bus) const;
    Stats getTotalStats() const;
    void resetStats(int bus);
    void resetAllStats();
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief Get mode name
     */
    std::string getModeName(Mode mode) const;
    
    /**
     * @brief Get state name
     */
    std::string getStateName(BusState state) const;
    
    /**
     * @brief Test SPI device
     * @param bus Bus number
     * @param test_data Test data to send
     * @return true if test passes
     */
    bool testDevice(int bus, const std::vector<uint8_t>& test_data = {0x55, 0xAA, 0xFF, 0x00});
    
    /**
     * @brief Get maximum speed supported
     */
    int getMaxSpeed(int bus) const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    SPIInterface();
    ~SPIInterface();
    SPIInterface(const SPIInterface&) = delete;
    SPIInterface& operator=(const SPIInterface&) = delete;
    
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    struct BusHandle {
        int bus;
        int fd;
        SPIConfig config;
        BusState state;
        int current_cs;
        Stats stats;
    };
    
    std::map<int, BusHandle> buses;
    
    TransferCallback on_transfer;
    ErrorCallback on_error;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    bool openDevice(int bus, int chip_select);
    void closeDevice(int bus);
    bool configureDevice(int bus, const SPIConfig& config);
    bool setChipSelect(int bus, int chip_select, bool select);
    std::vector<uint8_t> transferImpl(int bus, const std::vector<uint8_t>& tx_data, const SPIConfig* config = nullptr);
    void recordTransfer(int bus, const SPITransfer& transfer);
    void notifyError(int bus, const std::string& error);
    
    // Helper functions
    std::string getDevicePath(int bus, int chip_select) const;
    int getSPIFd(int bus) const;
    bool isBusValid(int bus) const;
    void delayMicroseconds(int us);
};

#endif // SPI_INTERFACE_HPP
