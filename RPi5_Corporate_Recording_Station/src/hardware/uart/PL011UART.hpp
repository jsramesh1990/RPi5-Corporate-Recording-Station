#ifndef PL011_UART_HPP
#define PL011_UART_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <cstdint>

/**
 * @brief PL011 UART Driver for Raspberry Pi 5
 * 
 * Provides UART communication with support for PL011 UART
 * on Raspberry Pi 5 (ttyAMA0, ttyAMA10, etc.).
 * Optimized for ARM Cortex-A76.
 */
class PL011UART {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Parity {
        NONE,
        EVEN,
        ODD,
        MARK,
        SPACE
    };
    
    enum class StopBits {
        ONE,
        TWO
    };
    
    enum class DataBits {
        FIVE = 5,
        SIX = 6,
        SEVEN = 7,
        EIGHT = 8
    };
    
    enum class FlowControl {
        NONE,
        RTS_CTS,
        XON_XOFF
    };
    
    enum class UARTState {
        CLOSED,
        OPEN,
        CONFIGURED,
        RUNNING,
        ERROR
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief UART configuration
     */
    struct UARTConfig {
        std::string device = "/dev/ttyAMA0";
        int baud_rate = 115200;
        DataBits data_bits = DataBits::EIGHT;
        StopBits stop_bits = StopBits::ONE;
        Parity parity = Parity::NONE;
        FlowControl flow_control = FlowControl::NONE;
        int read_timeout_ms = 1000;
        int write_timeout_ms = 1000;
        bool nonblocking = true;
        int buffer_size = 4096;
        bool hardware_flow_control = false;
    };
    
    /**
     * @brief UART statistics
     */
    struct UARTStats {
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t send_errors = 0;
        uint64_t receive_errors = 0;
        uint64_t overrun_errors = 0;
        uint64_t parity_errors = 0;
        uint64_t framing_errors = 0;
        uint64_t breaks = 0;
        double baud_rate_actual = 0;
        double throughput_sent = 0;
        double throughput_received = 0;
        uint64_t total_transactions = 0;
        std::chrono::system_clock::time_point last_activity;
    };
    
    /**
     * @brief UART device information
     */
    struct DeviceInfo {
        std::string device_path;
        std::string driver;
        bool is_available;
        bool is_pl011;
        std::vector<int> supported_baud_rates;
        std::vector<DataBits> supported_data_bits;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using StateCallback = std::function<void(UARTState)>;
    using StatsCallback = std::function<void(const UARTStats&)>;
    
    // ============================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================
    
    PL011UART();
    ~PL011UART();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize UART
     * @param config UART configuration
     * @return true if successful
     */
    bool initialize(const UARTConfig& config = UARTConfig());
    
    /**
     * @brief Initialize UART with parameters
     * @param device Device path
     * @param baud_rate Baud rate
     * @param data_bits Data bits
     * @param stop_bits Stop bits
     * @param parity Parity
     * @return true if successful
     */
    bool initialize(const std::string& device, int baud_rate = 115200,
                   DataBits data_bits = DataBits::EIGHT,
                   StopBits stop_bits = StopBits::ONE,
                   Parity parity = Parity::NONE);
    
    /**
     * @brief Initialize UART directly (bare-metal)
     * @param base_addr Base address of UART
     * @param baud_rate Baud rate
     * @return true if successful
     */
    bool initializeDirect(uint32_t base_addr, int baud_rate = 115200);
    
    /**
     * @brief Shutdown UART
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return state != UARTState::CLOSED; }
    
    // ============================================
    // CONFIGURATION
    // ============================================
    
    /**
     * @brief Get current configuration
     */
    UARTConfig getConfig() const { return config; }
    
    /**
     * @brief Set baud rate
     */
    bool setBaudRate(int baud_rate);
    
    /**
     * @brief Set data bits
     */
    bool setDataBits(DataBits data_bits);
    
    /**
     * @brief Set stop bits
     */
    bool setStopBits(StopBits stop_bits);
    
    /**
     * @brief Set parity
     */
    bool setParity(Parity parity);
    
    /**
     * @brief Set flow control
     */
    bool setFlowControl(FlowControl flow_control);
    
    /**
     * @brief Set read timeout
     */
    bool setReadTimeout(int timeout_ms);
    
    /**
     * @brief Get current baud rate
     */
    int getBaudRate() const { return config.baud_rate; }
    
    /**
     * @brief Get current state
     */
    UARTState getState() const { return state; }
    
    /**
     * @brief Get state name
     */
    std::string getStateName() const;
    
    // ============================================
    // DATA TRANSMISSION
    // ============================================
    
    /**
     * @brief Write data
     * @param data Data to write
     * @return true if successful
     */
    bool writeData(const std::vector<uint8_t>& data);
    
    /**
     * @brief Write string
     * @param data String to write
     * @return true if successful
     */
    bool writeData(const std::string& data);
    
    /**
     * @brief Write byte
     * @param byte Byte to write
     * @return true if successful
     */
    bool writeByte(uint8_t byte);
    
    /**
     * @brief Write line (adds newline)
     * @param line Line to write
     * @return true if successful
     */
    bool writeLine(const std::string& line);
    
    /**
     * @brief Write formatted string
     * @param format Format string
     * @param ... Arguments
     * @return true if successful
     */
    bool writeFormatted(const char* format, ...);
    
    /**
     * @brief Write hex dump
     * @param data Data to dump
     * @param prefix Prefix for each line
     * @return true if successful
     */
    bool writeHexDump(const std::vector<uint8_t>& data, const std::string& prefix = "");
    
    // ============================================
    // DATA RECEPTION
    // ============================================
    
    /**
     * @brief Read data
     * @param length Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> readData(size_t length);
    
    /**
     * @brief Read line
     * @return Line read
     */
    std::string readLine();
    
    /**
     * @brief Read all available data
     * @return All available data
     */
    std::string readAll();
    
    /**
     * @brief Read byte
     * @return Byte read
     */
    uint8_t readByte();
    
    /**
     * @brief Check if data available
     */
    bool dataAvailable() const;
    
    /**
     * @brief Get available bytes count
     */
    size_t bytesAvailable() const;
    
    /**
     * @brief Flush input buffer
     */
    void flushInput();
    
    /**
     * @brief Flush output buffer
     */
    void flushOutput();
    
    /**
     * @brief Flush both buffers
     */
    void flushBoth();
    
    // ============================================
    // ASYNCHRONOUS OPERATIONS
    // ============================================
    
    /**
     * @brief Start async read
     * @param callback Function called with received data
     * @return true if successful
     */
    bool startAsyncRead(DataCallback callback);
    
    /**
     * @brief Stop async read
     */
    void stopAsyncRead();
    
    /**
     * @brief Check if async read is running
     */
    bool isAsyncReadRunning() const { return async_read_running; }
    
    /**
     * @brief Start async write queue
     * @return true if successful
     */
    bool startAsyncWrite();
    
    /**
     * @brief Stop async write queue
     */
    void stopAsyncWrite();
    
    /**
     * @brief Queue data for async write
     * @param data Data to queue
     * @return true if successful
     */
    bool queueData(const std::vector<uint8_t>& data);
    
    /**
     * @brief Queue string for async write
     * @param data String to queue
     * @return true if successful
     */
    bool queueData(const std::string& data);
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get statistics
     */
    UARTStats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    /**
     * @brief Get connection status
     */
    bool isConnected() const;
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    void setOnError(ErrorCallback callback);
    void setOnStateChange(StateCallback callback);
    void setOnStats(StatsCallback callback);
    
    // ============================================
    // UTILITY
    // ============================================
    
    /**
     * @brief List available UART devices
     */
    std::vector<DeviceInfo> listDevices() const;
    
    /**
     * @brief Get device path
     */
    std::string getDevicePath() const { return config.device; }
    
    /**
     * @brief Test UART (loopback test)
     * @param test_data Data to send
     * @return true if test passes
     */
    bool testLoopback(const std::vector<uint8_t>& test_data = {0x55, 0xAA, 0xFF, 0x00});
    
    /**
     * @brief Get baud rate name
     */
    std::string getBaudRateName(int baud) const;
    
    /**
     * @brief Get parity name
     */
    std::string getParityName(Parity parity) const;
    
    /**
     * @brief Get stop bits name
     */
    std::string getStopBitsName(StopBits stop_bits) const;
    
    /**
     * @brief Get data bits name
     */
    std::string getDataBitsName(DataBits data_bits) const;
    
    /**
     * @brief Get flow control name
     */
    std::string getFlowControlName(FlowControl flow_control) const;
    
    /**
     * @brief Send break signal
     */
    bool sendBreak(int duration_ms = 100);
    
    /**
     * @brief Set DTR (Data Terminal Ready)
     */
    bool setDTR(bool enable);
    
    /**
     * @brief Set RTS (Request To Send)
     */
    bool setRTS(bool enable);
    
    /**
     * @brief Get DSR (Data Set Ready)
     */
    bool getDSR() const;
    
    /**
     * @brief Get CTS (Clear To Send)
     */
    bool getCTS() const;
    
    /**
     * @brief Get DCD (Data Carrier Detect)
     */
    bool getDCD() const;
    
    /**
     * @brief Get RI (Ring Indicator)
     */
    bool getRI() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    UARTConfig config;
    UARTState state = UARTState::CLOSED;
    UARTStats stats;
    int uart_fd = -1;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    mutable std::mutex queue_mutex;
    
    // Bare-metal mode
    void* uart_memory = nullptr;
    bool baremetal_mode = false;
    uint32_t uart_base = 0;
    
    // Async read
    std::thread async_read_thread;
    std::atomic<bool> async_read_running;
    DataCallback data_callback;
    
    // Async write queue
    std::thread async_write_thread;
    std::atomic<bool> async_write_running;
    std::queue<std::vector<uint8_t>> write_queue;
    
    // Callbacks
    ErrorCallback on_error;
    StateCallback on_state_change;
    StatsCallback on_stats;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // Device I/O
    bool openDevice();
    void closeDevice();
    bool configureDevice();
    bool setDeviceParameters();
    void applyFlowControl();
    void setTerminalSettings();
    
    // Bare-metal
    bool setupBaremetal();
    void shutdownBaremetal();
    bool configureBaremetal(uint32_t baud);
    void writeBaremetal(const uint8_t* data, size_t length);
    size_t readBaremetal(uint8_t* buffer, size_t max_length);
    bool dataAvailableBaremetal() const;
    void flushBaremetal();
    
    // Async threads
    void asyncReadLoop();
    void asyncWriteLoop();
    
    // Statistics
    void updateStatsSent(size_t bytes);
    void updateStatsReceived(size_t bytes);
    void notifyStats();
    
    // Error handling
    void notifyError(const std::string& error);
    void setState(UARTState new_state);
    void notifyStateChange();
    
    // Helper functions
    int getBaudFlag(int baud) const;
    std::string getDevicePath() const;
    bool isDeviceAvailable() const;
    bool isDevicePL011() const;
    std::vector<int> getSupportedBaudRates() const;
    void delayMicroseconds(int us);
    std::string getCurrentTimeStr() const;
    
    // PL011 register definitions
    static constexpr uint32_t PL011_DR = 0x00;
    static constexpr uint32_t PL011_RSR = 0x04;
    static constexpr uint32_t PL011_FR = 0x18;
    static constexpr uint32_t PL011_IBRD = 0x24;
    static constexpr uint32_t PL011_FBRD = 0x28;
    static constexpr uint32_t PL011_LCRH = 0x2C;
    static constexpr uint32_t PL011_CR = 0x30;
    static constexpr uint32_t PL011_IFLS = 0x34;
    static constexpr uint32_t PL011_IMSC = 0x38;
    static constexpr uint32_t PL011_RIS = 0x3C;
    static constexpr uint32_t PL011_MIS = 0x40;
    static constexpr uint32_t PL011_ICR = 0x44;
    static constexpr uint32_t PL011_DMACR = 0x48;
    
    static constexpr uint32_t PL011_FR_TXFF = 0x20;
    static constexpr uint32_t PL011_FR_RXFE = 0x10;
    static constexpr uint32_t PL011_FR_BUSY = 0x08;
    static constexpr uint32_t PL011_FR_DCD = 0x04;
    static constexpr uint32_t PL011_FR_DSR = 0x02;
    static constexpr uint32_t PL011_FR_CTS = 0x01;
    
    static constexpr uint32_t PL011_CR_UARTEN = 0x01;
    static constexpr uint32_t PL011_CR_TXE = 0x100;
    static constexpr uint32_t PL011_CR_RXE = 0x200;
    static constexpr uint32_t PL011_CR_RTS = 0x800;
    static constexpr uint32_t PL011_CR_DTR = 0x400;
    
    static constexpr uint32_t PL011_LCRH_FEN = 0x10;
    static constexpr uint32_t PL011_LCRH_WLEN_8 = 0x60;
    static constexpr uint32_t PL011_LCRH_WLEN_7 = 0x40;
    static constexpr uint32_t PL011_LCRH_WLEN_6 = 0x20;
    static constexpr uint32_t PL011_LCRH_WLEN_5 = 0x00;
    static constexpr uint32_t PL011_LCRH_STP2 = 0x08;
    static constexpr uint32_t PL011_LCRH_EPS = 0x04;
    static constexpr uint32_t PL011_LCRH_PEN = 0x02;
    static constexpr uint32_t PL011_LCRH_BRK = 0x01;
};

#endif // PL011_UART_HPP
