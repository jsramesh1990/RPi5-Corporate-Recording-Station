#include "BCM2712GPIO.hpp"

#include <iostream>
#include <map>
#include <chrono>
#include <thread>

// GPIO to physical pin mapping for Raspberry Pi 5 40-pin header
const std::map<int, int> BCM2712GPIO::gpio_to_physical = {
    {2, 3}, {3, 5}, {4, 7}, {5, 29}, {6, 31}, {7, 26}, {8, 24}, {9, 21},
    {10, 19}, {11, 23}, {12, 32}, {13, 33}, {14, 8}, {15, 10}, {16, 36},
    {17, 11}, {18, 12}, {19, 35}, {20, 38}, {21, 40}, {22, 15}, {23, 16},
    {24, 18}, {25, 22}, {26, 37}, {27, 13}
};

// Physical pin to GPIO mapping
const std::map<int, int> BCM2712GPIO::physical_to_gpio = {
    {3, 2}, {5, 3}, {7, 4}, {29, 5}, {31, 6}, {26, 7}, {24, 8}, {21, 9},
    {19, 10}, {23, 11}, {32, 12}, {33, 13}, {8, 14}, {10, 15}, {36, 16},
    {11, 17}, {12, 18}, {35, 19}, {38, 20}, {40, 21}, {15, 22}, {16, 23},
    {18, 24}, {22, 25}, {37, 26}, {13, 27}
};

// GPIO pin names
const std::map<int, std::string> BCM2712GPIO::gpio_pin_names = {
    {2, "SDA1"}, {3, "SCL1"}, {4, "GPIO4"}, {5, "GPIO5"}, {6, "GPIO6"},
    {7, "SPI0_CE1"}, {8, "SPI0_CE0"}, {9, "SPI0_MISO"}, {10, "SPI0_MOSI"},
    {11, "SPI0_SCLK"}, {12, "PWM0"}, {13, "PWM1"}, {14, "UART0_TX"},
    {15, "UART0_RX"}, {16, "GPIO16"}, {17, "GPIO17"}, {18, "PCM_CLK"},
    {19, "PCM_FS"}, {20, "PCM_DIN"}, {21, "PCM_DOUT"}, {22, "GPIO22"},
    {23, "GPIO23"}, {24, "GPIO24"}, {25, "GPIO25"}, {26, "GPIO26"},
    {27, "GPIO27"}
};

BCM2712GPIO& BCM2712GPIO::getInstance() {
    static BCM2712GPIO instance;
    return instance;
}

BCM2712GPIO::BCM2712GPIO() : controller(GPIOController::getInstance()) {
    // Ensure GPIO controller is initialized
    if (!controller.isInitialized()) {
        controller.initialize();
    }
}

BCM2712GPIO::~BCM2712GPIO() {
    // Don't shutdown - controller is singleton
}

bool BCM2712GPIO::waitForEvent(int pin, GPIOEvent event, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    uint64_t last_value = controller.readPin(pin) ? 1 : 0;
    
    while (true) {
        uint64_t current_value = controller.readPin(pin) ? 1 : 0;
        bool event_triggered = false;
        
        switch (event) {
            case GPIOEvent::RISING_EDGE:
                event_triggered = (current_value == 1 && last_value == 0);
                break;
            case GPIOEvent::FALLING_EDGE:
                event_triggered = (current_value == 0 && last_value == 1);
                break;
            case GPIOEvent::LEVEL_HIGH:
                event_triggered = (current_value == 1);
                break;
            case GPIOEvent::LEVEL_LOW:
                event_triggered = (current_value == 0);
                break;
            case GPIOEvent::ASYNC_RISING:
            case GPIOEvent::ASYNC_FALLING:
                // Async events are handled by hardware interrupts
                // For now, use same as edge detection
                if (event == GPIOEvent::ASYNC_RISING) {
                    event_triggered = (current_value == 1 && last_value == 0);
                } else {
                    event_triggered = (current_value == 0 && last_value == 1);
                }
                break;
            default:
                return false;
        }
        
        if (event_triggered) {
            return true;
        }
        
        if (timeout_ms >= 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_ms) {
                return false;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

bool BCM2712GPIO::getEventStatus(int pin) const {
    // Read event detect status
    // This is a simplified implementation
    return false;
}

void BCM2712GPIO::clearEventStatus(int pin) {
    // Clear event detect status
    // This is a simplified implementation
}

int BCM2712GPIO::gpioToPhysical(int gpio) const {
    auto it = gpio_to_physical.find(gpio);
    if (it != gpio_to_physical.end()) {
        return it->second;
    }
    return -1;
}

int BCM2712GPIO::physicalToGPIO(int physical) const {
    auto it = physical_to_gpio.find(physical);
    if (it != physical_to_gpio.end()) {
        return it->second;
    }
    return -1;
}

std::string BCM2712GPIO::getGPIOPinName(int gpio) const {
    auto it = gpio_pin_names.find(gpio);
    if (it != gpio_pin_names.end()) {
        return it->second;
    }
    return "GPIO" + std::to_string(gpio);
}

uint32_t BCM2712GPIO::readRegister(uint32_t offset) const {
    // This would read from GPIO registers directly
    // For now, return 0
    return 0;
}

void BCM2712GPIO::writeRegister(uint32_t offset, uint32_t value) {
    // This would write to GPIO registers directly
}

void BCM2712GPIO::setRegisterBits(uint32_t offset, uint32_t mask) {
    // This would set bits in GPIO registers
}

void BCM2712GPIO::clearRegisterBits(uint32_t offset, uint32_t mask) {
    // This would clear bits in GPIO registers
}

bool BCM2712GPIO::isInitialized() const {
    return controller.isInitialized();
}

std::string BCM2712GPIO::getLastError() const {
    return controller.getLastError();
}
