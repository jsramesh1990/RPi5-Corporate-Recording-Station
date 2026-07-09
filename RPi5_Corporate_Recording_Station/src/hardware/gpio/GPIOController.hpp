#ifndef GPIO_CONTROLLER_HPP
#define GPIO_CONTROLLER_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>
#include <atomic>

/**
 * @brief GPIO Controller for Raspberry Pi 5
 * 
 * Provides hardware-level control of GPIO pins with
 * interrupt support, PWM, and I2C/SPI interfaces.
 * Optimized for BCM2712 on Raspberry Pi 5.
 */
class GPIOController {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class PinDirection {
        INPUT,
        OUTPUT,
        ALT0,
        ALT1,
        ALT2,
        ALT3,
        ALT4,
        ALT5
    };
    
    enum class PullMode {
        NONE,
        PULL_UP,
        PULL_DOWN
    };
    
    enum class DriveStrength {
        DRIVE_2MA,
        DRIVE_4MA,
        DRIVE_6MA,
        DRIVE_8MA,
        DRIVE_10MA,
        DRIVE_12MA,
        DRIVE_14MA,
        DRIVE_16MA
    };
    
    enum class InterruptMode {
        EDGE_NONE,
        EDGE_RISING,
        EDGE_FALLING,
        EDGE_BOTH,
        LEVEL_HIGH,
        LEVEL_LOW
    };
    
    enum class PinState {
        LOW = 0,
        HIGH = 1,
        TOGGLE = 2
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief GPIO pin configuration
     */
    struct PinConfig {
        int pin_number = -1;
        std::string name = "";
        PinDirection direction = PinDirection::INPUT;
        PullMode pull = PullMode::NONE;
        DriveStrength drive = DriveStrength::DRIVE_8MA;
        bool interrupt_enabled = false;
        InterruptMode interrupt_mode = InterruptMode::EDGE_NONE;
        int initial_value = 0;
        bool invert = false;
        int debounce_ms = 0;
    };
    
    /**
     * @brief GPIO pin state information
     */
    struct PinInfo {
        int pin = -1;
        std::string name;
        PinDirection direction;
        PullMode pull;
        bool value;
        bool interrupt_enabled;
        InterruptMode interrupt_mode;
        uint64_t last_change_time = 0;
        uint64_t change_count = 0;
    };
    
    /**
     * @brief PWM configuration
     */
    struct PWMConfig {
        int pin = -1;
        int frequency = 1000;      // Hz
        int duty_cycle = 50;       // Percentage (0-100)
        bool enabled = false;
        int range = 1024;          // PWM range
        int clock_divisor = 1;
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using InterruptCallback = std::function<void(int pin, bool value)>;
    using EdgeCallback = std::function<void(int pin, bool rising)>;
    using StateCallback = std::function<void(const PinInfo&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    // ============================================
    // SINGLETON
    // ============================================
    
    static GPIOController& getInstance();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize GPIO controller
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Shutdown GPIO controller
     */
    void shutdown();
    
    // ============================================
    // PIN CONFIGURATION
    // ============================================
    
    /**
     * @brief Configure a GPIO pin
     * @param config Pin configuration
     * @return true if successful
     */
    bool configurePin(const PinConfig& config);
    
    /**
     * @brief Configure a GPIO pin (simplified)
     * @param pin Pin number
     * @param direction Pin direction
     * @param initial_value Initial value (for output)
     * @return true if successful
     */
    bool configurePin(int pin, PinDirection direction, int initial_value = 0);
    
    /**
     * @brief Get pin configuration
     */
    PinConfig getPinConfig(int pin) const;
    
    /**
     * @brief Get pin information
     */
    PinInfo getPinInfo(int pin) const;
    
    /**
     * @brief Get all pin information
     */
    std::vector<PinInfo> getAllPinInfo() const;
    
    /**
     * @brief Set pin name
     */
    bool setPinName(int pin, const std::string& name);
    
    /**
     * @brief Get pin name
     */
    std::string getPinName(int pin) const;
    
    /**
     * @brief Get pin by name
     */
    int getPinByName(const std::string& name) const;
    
    // ============================================
    // DIGITAL I/O
    // ============================================
    
    /**
     * @brief Write value to pin
     * @param pin Pin number
     * @param value Value (true=HIGH, false=LOW)
     * @return true if successful
     */
    bool writePin(int pin, bool value);
    
    /**
     * @brief Write value to pin with state
     * @param pin Pin number
     * @param state Pin state
     * @return true if successful
     */
    bool writePin(int pin, PinState state);
    
    /**
     * @brief Read value from pin
     * @param pin Pin number
     * @return true=HIGH, false=LOW
     */
    bool readPin(int pin) const;
    
    /**
     * @brief Toggle pin value
     * @param pin Pin number
     * @return true if successful
     */
    bool togglePin(int pin);
    
    /**
     * @brief Read pin value as integer
     * @param pin Pin number
     * @return 0=LOW, 1=HIGH, -1=error
     */
    int readPinValue(int pin) const;
    
    /**
     * @brief Write multiple pins
     * @param pins Vector of pin numbers
     * @param values Vector of values
     * @return Number of pins successfully written
     */
    size_t writePins(const std::vector<int>& pins, const std::vector<bool>& values);
    
    /**
     * @brief Read multiple pins
     * @param pins Vector of pin numbers
     * @return Map of pin to value
     */
    std::map<int, bool> readPins(const std::vector<int>& pins) const;
    
    /**
     * @brief Set pin group (for parallel operations)
     * @param pins Vector of pin numbers
     * @param value Bitmask value
     * @return true if successful
     */
    bool setPinGroup(const std::vector<int>& pins, uint32_t value);
    
    /**
     * @brief Get pin group value
     * @param pins Vector of pin numbers
     * @return Bitmask value
     */
    uint32_t getPinGroup(const std::vector<int>& pins) const;
    
    // ============================================
    // PWM (Pulse Width Modulation)
    // ============================================
    
    /**
     * @brief Enable PWM on pin
     * @param pin Pin number (must support PWM)
     * @param frequency Frequency in Hz
     * @param duty_cycle Duty cycle (0-100)
     * @return true if successful
     */
    bool enablePWM(int pin, int frequency = 1000, int duty_cycle = 50);
    
    /**
     * @brief Disable PWM on pin
     * @param pin Pin number
     * @return true if successful
     */
    bool disablePWM(int pin);
    
    /**
     * @brief Set PWM frequency
     * @param pin Pin number
     * @param frequency Frequency in Hz
     * @return true if successful
     */
    bool setPWMFrequency(int pin, int frequency);
    
    /**
     * @brief Set PWM duty cycle
     * @param pin Pin number
     * @param duty_cycle Duty cycle (0-100)
     * @return true if successful
     */
    bool setPWMDutyCycle(int pin, int duty_cycle);
    
    /**
     * @brief Get PWM configuration
     */
    PWMConfig getPWMConfig(int pin) const;
    
    /**
     * @brief Check if PWM is enabled on pin
     */
    bool isPWMEnabled(int pin) const;
    
    // ============================================
    // INTERRUPTS
    // ============================================
    
    /**
     * @brief Enable interrupt on pin
     * @param pin Pin number
     * @param callback Callback function
     * @param mode Interrupt mode
     * @return true if successful
     */
    bool enableInterrupt(int pin, InterruptCallback callback, 
                        InterruptMode mode = InterruptMode::EDGE_BOTH);
    
    /**
     * @brief Enable edge interrupt on pin
     * @param pin Pin number
     * @param callback Callback function
     * @param rising true for rising edge, false for falling
     * @return true if successful
     */
    bool enableEdgeInterrupt(int pin, EdgeCallback callback, bool rising = true);
    
    /**
     * @brief Disable interrupt on pin
     * @param pin Pin number
     * @return true if successful
     */
    bool disableInterrupt(int pin);
    
    /**
     * @brief Check if interrupt is enabled on pin
     */
    bool isInterruptEnabled(int pin) const;
    
    /**
     * @brief Clear interrupt on pin
     * @param pin Pin number
     */
    void clearInterrupt(int pin);
    
    /**
     * @brief Wait for interrupt on pin
     * @param pin Pin number
     * @param timeout_ms Timeout in milliseconds (-1 = infinite)
     * @return true if interrupt occurred
     */
    bool waitForInterrupt(int pin, int timeout_ms = -1);
    
    // ============================================
    // I2C INTERFACE
    // ============================================
    
    /**
     * @brief Initialize I2C bus
     * @param bus I2C bus number
     * @param speed Speed in Hz (default: 100000)
     * @return true if successful
     */
    bool i2cInit(int bus = 1, int speed = 100000);
    
    /**
     * @brief Write to I2C device
     * @param bus I2C bus number
     * @param address Device address
     * @param data Data to write
     * @return true if successful
     */
    bool i2cWrite(int bus, uint8_t address, const std::vector<uint8_t>& data);
    
    /**
     * @brief Read from I2C device
     * @param bus I2C bus number
     * @param address Device address
     * @param length Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> i2cRead(int bus, uint8_t address, size_t length);
    
    /**
     * @brief Write to and read from I2C device
     * @param bus I2C bus number
     * @param address Device address
     * @param writeData Data to write
     * @param readData Buffer for read data
     * @param readLength Number of bytes to read
     * @return true if successful
     */
    bool i2cWriteRead(int bus, uint8_t address, const std::vector<uint8_t>& writeData,
                     std::vector<uint8_t>& readData, size_t readLength);
    
    /**
     * @brief Scan I2C bus for devices
     * @param bus I2C bus number
     * @return Vector of device addresses
     */
    std::vector<uint8_t> i2cScan(int bus = 1);
    
    // ============================================
    // SPI INTERFACE
    // ============================================
    
    /**
     * @brief Initialize SPI bus
     * @param bus SPI bus number
     * @param speed Speed in Hz (default: 1000000)
     * @param mode SPI mode (0-3)
     * @return true if successful
     */
    bool spiInit(int bus = 0, int speed = 1000000, int mode = 0);
    
    /**
     * @brief Select SPI chip
     * @param chipSelect Chip select pin (0 or 1)
     */
    void spiSelect(int chipSelect);
    
    /**
     * @brief Deselect SPI chip
     * @param chipSelect Chip select pin
     */
    void spiDeselect(int chipSelect);
    
    /**
     * @brief Transfer SPI data
     * @param data Data to send
     * @return Received data
     */
    std::vector<uint8_t> spiTransfer(const std::vector<uint8_t>& data);
    
    /**
     * @brief Read SPI data
     * @param length Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> spiRead(size_t length);
    
    /**
     * @brief Write SPI data
     * @param data Data to write
     * @return true if successful
     */
    bool spiWrite(const std::vector<uint8_t>& data);
    
    /**
     * @brief Write and read SPI data simultaneously
     * @param writeData Data to write
     * @param readData Buffer for read data
     * @return true if successful
     */
    bool spiWriteRead(const std::vector<uint8_t>& writeData,
                     std::vector<uint8_t>& readData);
    
    // ============================================
    // UTILITY FUNCTIONS
    // ============================================
    
    /**
     * @brief Get pin state as string
     */
    std::string getPinStateString(int pin) const;
    
    /**
     * @brief Print pin state
     */
    void printPinState(int pin) const;
    
    /**
     * @brief Print all pin states
     */
    void printAllPinStates() const;
    
    /**
     * @brief Export pin for sysfs (legacy)
     */
    bool exportPin(int pin);
    
    /**
     * @brief Unexport pin from sysfs
     */
    bool unexportPin(int pin);
    
    /**
     * @brief Get direction name
     */
    std::string getDirectionName(PinDirection direction) const;
    
    /**
     * @brief Get pull mode name
     */
    std::string getPullModeName(PullMode pull) const;
    
    /**
     * @brief Get interrupt mode name
     */
    std::string getInterruptModeName(InterruptMode mode) const;
    
    // ============================================
    // ERROR HANDLING
    // ============================================
    
    void setOnError(ErrorCallback callback);
    void clearError();
    std::string getLastError() const;
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    GPIOController();
    ~GPIOController();
    GPIOController(const GPIOController&) = delete;
    GPIOController& operator=(const GPIOController&) = delete;
    
    bool initialized = false;
    bool interrupt_running = false;
    void* gpio_memory = nullptr;
    
    std::map<int, PinConfig> pin_configs;
    std::map<int, PinInfo> pin_info;
    std::map<int, PWMConfig> pwm_configs;
    std::map<int, InterruptCallback> interrupt_callbacks;
    std::map<int, EdgeCallback> edge_callbacks;
    std::map<int, std::thread> interrupt_threads;
    
    mutable std::mutex gpio_mutex;
    mutable std::mutex interrupt_mutex;
    mutable std::mutex error_mutex;
    
    std::string last_error;
    ErrorCallback on_error;
    
    // GPIO register definitions for BCM2712
    static constexpr uint32_t GPIO_BASE = 0xFE200000;
    static constexpr uint32_t GPIO_SIZE = 0x1000;
    static constexpr int MAX_PINS = 54;
    
    // Register offsets
    static constexpr uint32_t GPFSEL0 = 0x00;
    static constexpr uint32_t GPFSEL1 = 0x04;
    static constexpr uint32_t GPFSEL2 = 0x08;
    static constexpr uint32_t GPFSEL3 = 0x0C;
    static constexpr uint32_t GPFSEL4 = 0x10;
    static constexpr uint32_t GPFSEL5 = 0x14;
    
    static constexpr uint32_t GPSET0 = 0x1C;
    static constexpr uint32_t GPSET1 = 0x20;
    static constexpr uint32_t GPCLR0 = 0x28;
    static constexpr uint32_t GPCLR1 = 0x2C;
    static constexpr uint32_t GPLEV0 = 0x34;
    static constexpr uint32_t GPLEV1 = 0x38;
    static constexpr uint32_t GPEDS0 = 0x40;
    static constexpr uint32_t GPEDS1 = 0x44;
    static constexpr uint32_t GPREN0 = 0x4C;
    static constexpr uint32_t GPREN1 = 0x50;
    static constexpr uint32_t GPFEN0 = 0x58;
    static constexpr uint32_t GPFEN1 = 0x5C;
    static constexpr uint32_t GPHEN0 = 0x64;
    static constexpr uint32_t GPHEN1 = 0x68;
    static constexpr uint32_t GPLEN0 = 0x70;
    static constexpr uint32_t GPLEN1 = 0x74;
    static constexpr uint32_t GPAREN0 = 0x7C;
    static constexpr uint32_t GPAREN1 = 0x80;
    static constexpr uint32_t GPAFEN0 = 0x88;
    static constexpr uint32_t GPAFEN1 = 0x8C;
    static constexpr uint32_t GPPUD = 0x94;
    static constexpr uint32_t GPPUDCLK0 = 0x98;
    static constexpr uint32_t GPPUDCLK1 = 0x9C;
    
    struct GPIORegisters {
        volatile uint32_t GPFSEL[6];
        volatile uint32_t GPSET[2];
        volatile uint32_t GPCLR[2];
        volatile uint32_t GPLEV[2];
        volatile uint32_t GPEDS[2];
        volatile uint32_t GPREN[2];
        volatile uint32_t GPFEN[2];
        volatile uint32_t GPHEN[2];
        volatile uint32_t GPLEN[2];
        volatile uint32_t GPAREN[2];
        volatile uint32_t GPAFEN[2];
        volatile uint32_t GPPUD;
        volatile uint32_t GPPUDCLK[2];
    };
    
    GPIORegisters* regs = nullptr;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    // Memory mapping
    void* mapPeripheral(uint32_t base, size_t size);
    void unmapPeripheral();
    
    // Register access
    uint32_t readRegister(uint32_t offset) const;
    void writeRegister(uint32_t offset, uint32_t value);
    void setRegisterBits(uint32_t offset, uint32_t mask);
    void clearRegisterBits(uint32_t offset, uint32_t mask);
    
    // Pin functions
    void setPinFunction(int pin, int function);
    int getPinFunction(int pin) const;
    void setPullMode(int pin, int mode);
    int getPullMode(int pin) const;
    
    // Interrupt handling
    void processInterrupts();
    void interruptHandler(int pin);
    void setupInterrupt(int pin, InterruptMode mode);
    
    // PWM
    bool setupPWM(int pin);
    void updatePWM(int pin);
    
    // I2C
    struct I2CInterface {
        int bus;
        int speed;
        int fd;
    };
    std::map<int, I2CInterface> i2c_interfaces;
    bool i2cOpen(int bus);
    void i2cClose(int bus);
    
    // SPI
    struct SPIInterface {
        int bus;
        int speed;
        int mode;
        int fd;
    };
    std::map<int, SPIInterface> spi_interfaces;
    int current_spi_cs = -1;
    bool spiOpen(int bus);
    void spiClose(int bus);
    
    // Utility
    bool isPinValid(int pin) const;
    bool isPinPWM(int pin) const;
    bool isPinSPI(int pin) const;
    bool isPinI2C(int pin) const;
    void delayMicroseconds(int us);
    void setError(const std::string& error);
    void notifyError(const std::string& error);
    uint64_t getCurrentTimeMicros() const;
    
    // Pin info update
    void updatePinInfo(int pin);
    void updateAllPinInfo();
};

#endif // GPIO_CONTROLLER_HPP
