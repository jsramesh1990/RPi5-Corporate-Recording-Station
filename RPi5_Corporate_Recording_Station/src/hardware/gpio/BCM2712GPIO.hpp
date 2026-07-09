#ifndef BCM2712_GPIO_HPP
#define BCM2712_GPIO_HPP

// This file is a wrapper/extension for GPIOController
// It provides BCM2712-specific functionality for Raspberry Pi 5

#include "GPIOController.hpp"

/**
 * @brief BCM2712 GPIO Extensions for Raspberry Pi 5
 * 
 * This class extends GPIOController with BCM2712-specific features
 * including GPIO event handling and enhanced pin mapping.
 */
class BCM2712GPIO {
public:
    static BCM2712GPIO& getInstance();
    
    // ============================================
    // GPIO EVENT HANDLING
    // ============================================
    
    enum class GPIOEvent {
        NONE,
        RISING_EDGE,
        FALLING_EDGE,
        LEVEL_HIGH,
        LEVEL_LOW,
        ASYNC_RISING,
        ASYNC_FALLING
    };
    
    /**
     * @brief Wait for GPIO event
     * @param pin Pin number
     * @param event Event type
     * @param timeout_ms Timeout in milliseconds (-1 = infinite)
     * @return true if event occurred
     */
    bool waitForEvent(int pin, GPIOEvent event, int timeout_ms = -1);
    
    /**
     * @brief Get event status
     */
    bool getEventStatus(int pin) const;
    
    /**
     * @brief Clear event status
     */
    void clearEventStatus(int pin);
    
    // ============================================
    // PIN MAPPING
    // ============================================
    
    /**
     * @brief Get physical pin number from GPIO number
     */
    int gpioToPhysical(int gpio) const;
    
    /**
     * @brief Get GPIO number from physical pin
     */
    int physicalToGPIO(int physical) const;
    
    /**
     * @brief Get pin name from GPIO number
     */
    std::string getGPIOPinName(int gpio) const;
    
    // ============================================
    // REGISTER ACCESS (for advanced users)
    // ============================================
    
    /**
     * @brief Read GPIO register
     */
    uint32_t readRegister(uint32_t offset) const;
    
    /**
     * @brief Write GPIO register
     */
    void writeRegister(uint32_t offset, uint32_t value);
    
    /**
     * @brief Set GPIO register bits
     */
    void setRegisterBits(uint32_t offset, uint32_t mask);
    
    /**
     * @brief Clear GPIO register bits
     */
    void clearRegisterBits(uint32_t offset, uint32_t mask);
    
    // ============================================
    // STATUS
    // ============================================
    
    /**
     * @brief Check if GPIO is initialized
     */
    bool isInitialized() const;
    
    /**
     * @brief Get last error
     */
    std::string getLastError() const;
    
private:
    BCM2712GPIO();
    ~BCM2712GPIO();
    BCM2712GPIO(const BCM2712GPIO&) = delete;
    BCM2712GPIO& operator=(const BCM2712GPIO&) = delete;
    
    GPIOController& controller;
    
    // GPIO to physical pin mapping
    static const std::map<int, int> gpio_to_physical;
    static const std::map<int, int> physical_to_gpio;
    static const std::map<int, std::string> gpio_pin_names;
};

#endif // BCM2712GPIO_HPP
