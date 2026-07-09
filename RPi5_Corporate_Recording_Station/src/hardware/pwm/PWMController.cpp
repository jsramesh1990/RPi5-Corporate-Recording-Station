#include "PWMController.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

// GPIO mapping for PWM channels on Raspberry Pi 5
const std::map<PWMController::Channel, int> PWMController::channel_pin_map = {
    {Channel::CHANNEL_0, 12},
    {Channel::CHANNEL_1, 13},
    {Channel::CHANNEL_2, 18},
    {Channel::CHANNEL_3, 19},
    {Channel::CHANNEL_4, 40},
    {Channel::CHANNEL_5, 41},
    {Channel::CHANNEL_6, 42},
    {Channel::CHANNEL_7, 43}
};

const std::map<int, PWMController::Channel> PWMController::pin_channel_map = {
    {12, Channel::CHANNEL_0},
    {13, Channel::CHANNEL_1},
    {18, Channel::CHANNEL_2},
    {19, Channel::CHANNEL_3},
    {40, Channel::CHANNEL_4},
    {41, Channel::CHANNEL_5},
    {42, Channel::CHANNEL_6},
    {43, Channel::CHANNEL_7}
};

// Note frequencies (Hz)
const std::map<std::string, double> PWMController::note_frequencies = {
    {"C", 261.63}, {"C#", 277.18}, {"Db", 277.18},
    {"D", 293.66}, {"D#", 311.13}, {"Eb", 311.13},
    {"E", 329.63}, {"F", 349.23}, {"F#", 369.99},
    {"Gb", 369.99}, {"G", 392.00}, {"G#", 415.30},
    {"Ab", 415.30}, {"A", 440.00}, {"A#", 466.16},
    {"Bb", 466.16}, {"B", 493.88}
};

// ============================================
// SINGLETON
// ============================================

PWMController& PWMController::getInstance() {
    static PWMController instance;
    return instance;
}

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

PWMController::PWMController() {
    // Initialize default channel data
    for (int i = 0; i < 8; i++) {
        Channel ch = static_cast<Channel>(i);
        PWMChannelData data;
        data.config.channel = ch;
        data.config.pin = channelToPin(ch);
        data.config.enabled = false;
        data.config.frequency = 1000;
        data.config.duty_cycle = 50;
        data.config.range = 1024;
        data.enabled = false;
        data.running = false;
        data.thread_active = false;
        channels[ch] = data;
    }
}

PWMController::~PWMController() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool PWMController::initialize() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    // Map PWM registers
    int mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        notifyError("Failed to open /dev/gpiomem");
        return false;
    }
    
    pwm_memory = mmap(nullptr, PWM_SIZE, PROT_READ | PROT_WRITE, 
                      MAP_SHARED, mem_fd, PWM_BASE);
    close(mem_fd);
    
    if (pwm_memory == MAP_FAILED) {
        notifyError("Failed to map PWM registers");
        return false;
    }
    
    regs = static_cast<PWMRegisters*>(pwm_memory);
    
    initialized = true;
    std::cout << "PWM Controller initialized" << std::endl;
    return true;
}

void PWMController::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    // Disable all channels
    for (auto& [channel, data] : channels) {
        if (data.enabled) {
            disable(channel);
        }
        if (data.thread_active) {
            data.running = false;
            if (data.thread.joinable()) {
                data.thread.join();
            }
            data.thread_active = false;
        }
    }
    
    // Unmap registers
    if (pwm_memory != nullptr) {
        munmap(pwm_memory, PWM_SIZE);
        pwm_memory = nullptr;
        regs = nullptr;
    }
    
    initialized = false;
    std::cout << "PWM Controller shutdown" << std::endl;
}

// ============================================
// PWM CHANNEL MANAGEMENT
// ============================================

bool PWMController::configure(const PWMConfig& config) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        notifyError("PWM not initialized");
        return false;
    }
    
    auto it = channels.find(config.channel);
    if (it == channels.end()) {
        notifyError("Invalid channel");
        return false;
    }
    
    // Disable channel if enabled
    if (it->second.enabled) {
        disable(config.channel);
    }
    
    // Update configuration
    it->second.config = config;
    
    // Set pin function to ALT for PWM
    if (config.pin >= 0) {
        // GPIO pin function setting would go here
        // Using GPIOController would be the proper way
    }
    
    // Apply configuration
    switch (config.mode) {
        case Mode::PWM:
        case Mode::LED:
            setupHardwarePWM(config.channel);
            break;
        case Mode::SERVO:
            setupHardwarePWM(config.channel);
            // Set to servo frequency (50Hz)
            setFrequency(config.channel, 50);
            break;
        case Mode::TONE:
            // Tone uses software PWM for arbitrary frequencies
            setupSoftwarePWM(config.channel);
            break;
    }
    
    // Enable if specified
    if (config.enabled) {
        enable(config.channel);
    }
    
    return true;
}

bool PWMController::configure(Channel channel, int pin, int frequency, 
                             int duty_cycle, Mode mode) {
    PWMConfig config;
    config.channel = channel;
    config.pin = pin;
    config.frequency = frequency;
    config.duty_cycle = duty_cycle;
    config.mode = mode;
    return configure(config);
}

bool PWMController::enable(Channel channel) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return false;
    }
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    if (it->second.enabled) {
        return true;
    }
    
    // Setup hardware or software PWM
    if (it->second.config.mode == Mode::TONE) {
        setupSoftwarePWM(channel);
    } else {
        setupHardwarePWM(channel);
    }
    
    it->second.enabled = true;
    it->second.config.enabled = true;
    
    notifyStateChange(channel, true);
    return true;
}

bool PWMController::disable(Channel channel) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return false;
    }
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    if (!it->second.enabled) {
        return true;
    }
    
    // Stop software PWM thread
    if (it->second.thread_active) {
        it->second.running = false;
        if (it->second.thread.joinable()) {
            it->second.thread.join();
        }
        it->second.thread_active = false;
    }
    
    // Disable hardware PWM
    if (regs) {
        uint32_t ctl_reg = 0;
        switch (channel) {
            case Channel::CHANNEL_0:
            case Channel::CHANNEL_1:
                ctl_reg = regs->CTL & ~(1 << 0); // Disable PWM0
                regs->CTL = ctl_reg;
                break;
            case Channel::CHANNEL_2:
            case Channel::CHANNEL_3:
                ctl_reg = regs->CTL & ~(1 << 8); // Disable PWM1
                regs->CTL = ctl_reg;
                break;
            default:
                // Software PWM only
                break;
        }
    }
    
    it->second.enabled = false;
    it->second.config.enabled = false;
    
    notifyStateChange(channel, false);
    return true;
}

bool PWMController::isEnabled(Channel channel) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = channels.find(channel);
    return it != channels.end() && it->second.enabled;
}

PWMController::PWMConfig PWMController::getConfig(Channel channel) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = channels.find(channel);
    if (it != channels.end()) {
        return it->second.config;
    }
    return PWMConfig{};
}

std::vector<PWMController::PWMConfig> PWMController::getAllConfigs() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<PWMConfig> configs;
    for (const auto& [channel, data] : channels) {
        configs.push_back(data.config);
    }
    return configs;
}

// ============================================
// PWM CONTROL
// ============================================

bool PWMController::setFrequency(Channel channel, int frequency) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    if (it->second.enabled) {
        if (it->second.config.mode == Mode::TONE) {
            return setSoftwareFrequency(channel, frequency);
        } else {
            return setHardwareFrequency(channel, frequency);
        }
    }
    
    it->second.config.frequency = frequency;
    return true;
}

bool PWMController::setDutyCycle(Channel channel, int duty_cycle) {
    std::lock_guard<std::mutex> lock(mutex);
    
    duty_cycle = std::max(0, std::min(100, duty_cycle));
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    if (it->second.enabled) {
        if (it->second.config.mode == Mode::TONE) {
            return setSoftwareDutyCycle(channel, duty_cycle);
        } else {
            return setHardwareDutyCycle(channel, duty_cycle);
        }
    }
    
    it->second.config.duty_cycle = duty_cycle;
    return true;
}

bool PWMController::setPulseWidth(Channel channel, int pulse_width_us) {
    // Convert pulse width to duty cycle
    int period_us = 1000000 / getFrequency(channel);
    if (period_us <= 0) {
        return false;
    }
    
    int duty_cycle = (pulse_width_us * 100) / period_us;
    duty_cycle = std::max(0, std::min(100, duty_cycle));
    
    return setDutyCycle(channel, duty_cycle);
}

bool PWMController::setRange(Channel channel, uint32_t range) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    if (it->second.enabled) {
        return setHardwareRange(channel, range);
    }
    
    it->second.config.range = range;
    return true;
}

bool PWMController::setPolarity(Channel channel, Polarity polarity) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    it->second.config.polarity = polarity;
    // Hardware polarity setting would go here
    return true;
}

bool PWMController::setMode(Channel channel, Mode mode) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    if (it->second.enabled) {
        disable(channel);
    }
    
    it->second.config.mode = mode;
    return configure(it->second.config);
}

int PWMController::getFrequency(Channel channel) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = channels.find(channel);
    return it != channels.end() ? it->second.config.frequency : 0;
}

int PWMController::getDutyCycle(Channel channel) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = channels.find(channel);
    return it != channels.end() ? it->second.config.duty_cycle : 0;
}

PWMController::Mode PWMController::getMode(Channel channel) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = channels.find(channel);
    return it != channels.end() ? it->second.config.mode : Mode::PWM;
}

// ============================================
// HARDWARE PWM
// ============================================

bool PWMController::setupHardwarePWM(Channel channel) {
    if (!regs) {
        return false;
    }
    
    uint32_t ctl_reg = regs->CTL;
    uint32_t pwm_ctl = 0;
    uint32_t pwm_rng = 0;
    uint32_t pwm_dat = 0;
    
    int channel_index = static_cast<int>(channel);
    bool is_pwm1 = (channel_index >= 2 && channel_index <= 3);
    
    // Calculate PWM settings
    // For 19.2MHz oscillator
    const uint32_t clock_freq = 19200000;
    uint32_t divisor = clock_freq / (getFrequency(channel) * 1024);
    if (divisor < 1) divisor = 1;
    if (divisor > 4095) divisor = 4095;
    
    uint32_t range = 1024;
    uint32_t duty = (getDutyCycle(channel) * range) / 100;
    
    if (is_pwm1) {
        // PWM1 (channels 2-3)
        pwm_ctl = (divisor << 16) | 1; // Enable PWM1
        pwm_rng = range;
        pwm_dat = duty;
        
        regs->RNG2 = pwm_rng;
        regs->DAT2 = pwm_dat;
        regs->CTL = (ctl_reg & ~0xFF00) | (pwm_ctl << 8);
    } else {
        // PWM0 (channels 0-1)
        pwm_ctl = (divisor << 16) | 1; // Enable PWM0
        pwm_rng = range;
        pwm_dat = duty;
        
        regs->RNG1 = pwm_rng;
        regs->DAT1 = pwm_dat;
        regs->CTL = (ctl_reg & ~0xFF) | pwm_ctl;
    }
    
    return true;
}

bool PWMController::setupSoftwarePWM(Channel channel) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    // Start software PWM thread
    if (!it->second.thread_active) {
        it->second.running = true;
        it->second.thread_active = true;
        it->second.thread = std::thread(&PWMController::pwmThreadFunction, this, channel);
    }
    
    return true;
}

void PWMController::shutdownPWM(Channel channel) {
    auto it = channels.find(channel);
    if (it != channels.end() && it->second.thread_active) {
        it->second.running = false;
        if (it->second.thread.joinable()) {
            it->second.thread.join();
        }
        it->second.thread_active = false;
    }
}

bool PWMController::setHardwareFrequency(Channel channel, int frequency) {
    if (!regs) {
        return false;
    }
    
    // Recalculate PWM settings
    const uint32_t clock_freq = 19200000;
    uint32_t range = 1024;
    uint32_t divisor = clock_freq / (frequency * range);
    if (divisor < 1) divisor = 1;
    if (divisor > 4095) divisor = 4095;
    
    int channel_index = static_cast<int>(channel);
    bool is_pwm1 = (channel_index >= 2 && channel_index <= 3);
    
    if (is_pwm1) {
        uint32_t ctl = regs->CTL;
        ctl = (ctl & ~0xFF00) | (divisor << 24);
        regs->CTL = ctl;
    } else {
        uint32_t ctl = regs->CTL;
        ctl = (ctl & ~0xFF) | (divisor << 16);
        regs->CTL = ctl;
    }
    
    auto it = channels.find(channel);
    if (it != channels.end()) {
        it->second.config.frequency = frequency;
    }
    
    return true;
}

bool PWMController::setHardwareDutyCycle(Channel channel, int duty_cycle) {
    if (!regs) {
        return false;
    }
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    uint32_t range = it->second.config.range;
    uint32_t duty = (duty_cycle * range) / 100;
    
    int channel_index = static_cast<int>(channel);
    bool is_pwm1 = (channel_index >= 2 && channel_index <= 3);
    
    if (is_pwm1) {
        regs->DAT2 = duty;
    } else {
        regs->DAT1 = duty;
    }
    
    it->second.config.duty_cycle = duty_cycle;
    return true;
}

bool PWMController::setHardwareRange(Channel channel, uint32_t range) {
    if (!regs) {
        return false;
    }
    
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    int channel_index = static_cast<int>(channel);
    bool is_pwm1 = (channel_index >= 2 && channel_index <= 3);
    
    if (is_pwm1) {
        regs->RNG2 = range;
    } else {
        regs->RNG1 = range;
    }
    
    it->second.config.range = range;
    return true;
}

bool PWMController::setSoftwareFrequency(Channel channel, int frequency) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    it->second.config.frequency = frequency;
    return true;
}

bool PWMController::setSoftwareDutyCycle(Channel channel, int duty_cycle) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    it->second.config.duty_cycle = duty_cycle;
    return true;
}

// ============================================
// PWM THREAD
// ============================================

void PWMController::pwmThreadFunction(Channel channel) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return;
    }
    
    int pin = it->second.config.pin;
    int frequency = it->second.config.frequency;
    int duty_cycle = it->second.config.duty_cycle;
    
    // Software PWM timing
    int period_us = 1000000 / frequency;
    int on_us = (duty_cycle * period_us) / 100;
    int off_us = period_us - on_us;
    
    it->second.running = true;
    
    while (it->second.running) {
        // Check for configuration changes
        if (it->second.config.frequency != frequency) {
            frequency = it->second.config.frequency;
            period_us = 1000000 / frequency;
            on_us = (it->second.config.duty_cycle * period_us) / 100;
            off_us = period_us - on_us;
        }
        
        if (it->second.config.duty_cycle != duty_cycle) {
            duty_cycle = it->second.config.duty_cycle;
            on_us = (duty_cycle * period_us) / 100;
            off_us = period_us - on_us;
        }
        
        // Toggle pin (simplified - would use GPIOController)
        // This is a placeholder - actual GPIO control would be here
        
        // Sleep for on time
        std::this_thread::sleep_for(std::chrono::microseconds(on_us));
        
        // Sleep for off time
        std::this_thread::sleep_for(std::chrono::microseconds(off_us));
        
        updateStats(channel);
    }
}

// ============================================
// SERVO CONTROL
// ============================================

bool PWMController::configureServo(const ServoConfig& config) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = channels.find(config.channel);
    if (it == channels.end()) {
        return false;
    }
    
    // Store servo configuration
    it->second.servo_config = config;
    
    // Configure PWM for servo
    PWMConfig pwm_config;
    pwm_config.channel = config.channel;
    pwm_config.pin = channelToPin(config.channel);
    pwm_config.mode = Mode::SERVO;
    pwm_config.frequency = config.frequency;
    pwm_config.duty_cycle = 50; // Will be updated
    pwm_config.enabled = true;
    
    configure(pwm_config);
    
    // Set center position
    setServoPulse(config.channel, config.center_pulse_us);
    
    return true;
}

bool PWMController::configureServo(Channel channel, int min_pulse, 
                                   int max_pulse, int center_pulse) {
    ServoConfig config;
    config.channel = channel;
    config.min_pulse_us = min_pulse;
    config.max_pulse_us = max_pulse;
    config.center_pulse_us = center_pulse;
    return configureServo(config);
}

bool PWMController::setServoAngle(Channel channel, double angle) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    const ServoConfig& config = it->second.servo_config;
    
    // Clamp angle
    angle = std::max(config.min_angle, std::min(config.max_angle, angle));
    
    // Convert angle to pulse width
    int pulse = angleToPulse(angle, config);
    it->second.servo_config.current_angle = angle;
    
    return setServoPulse(channel, pulse);
}

bool PWMController::setServoPulse(Channel channel, int pulse_width_us) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    // Convert pulse width to duty cycle
    int frequency = getFrequency(channel);
    if (frequency <= 0) {
        return false;
    }
    
    int period_us = 1000000 / frequency;
    int duty_cycle = (pulse_width_us * 100) / period_us;
    duty_cycle = std::max(0, std::min(100, duty_cycle));
    
    return setDutyCycle(channel, duty_cycle);
}

PWMController::ServoConfig PWMController::getServoConfig(Channel channel) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = channels.find(channel);
    if (it != channels.end()) {
        return it->second.servo_config;
    }
    return ServoConfig{};
}

double PWMController::getServoAngle(Channel channel) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = channels.find(channel);
    if (it != channels.end()) {
        return it->second.servo_config.current_angle;
    }
    return 0;
}

bool PWMController::centerServo(Channel channel) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    return setServoPulse(channel, it->second.servo_config.center_pulse_us);
}

bool PWMController::sweepServo(Channel channel, double start_angle, double end_angle, int duration_ms) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    const ServoConfig& config = it->second.servo_config;
    
    start_angle = std::max(config.min_angle, std::min(config.max_angle, start_angle));
    end_angle = std::max(config.min_angle, std::min(config.max_angle, end_angle));
    
    int steps = 50;
    int step_duration = duration_ms / steps;
    double step = (end_angle - start_angle) / steps;
    
    for (int i = 0; i <= steps; i++) {
        double angle = start_angle + step * i;
        setServoAngle(channel, angle);
        std::this_thread::sleep_for(std::chrono::milliseconds(step_duration));
    }
    
    return true;
}

// ============================================
// TONE GENERATION
// ============================================

bool PWMController::playTone(Channel channel, int frequency, int duration_ms, int volume) {
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return false;
    }
    
    // Configure PWM for tone
    PWMConfig config;
    config.channel = channel;
    config.pin = channelToPin(channel);
    config.mode = Mode::TONE;
    config.frequency = frequency;
    config.duty_cycle = std::max(0, std::min(100, volume));
    config.enabled = true;
    
    configure(config);
    enable(channel);
    
    // Play for duration
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    
    // Stop
    disable(channel);
    
    return true;
}

bool PWMController::playNote(Channel channel, const std::string& note, 
                            int octave, int duration_ms, int volume) {
    double frequency = getNoteFrequency(note, octave);
    if (frequency <= 0) {
        return false;
    }
    return playTone(channel, static_cast<int>(frequency), duration_ms, volume);
}

void PWMController::stopTone(Channel channel) {
    disable(channel);
}

bool PWMController::playMelody(Channel channel, const std::vector<std::string>& notes, 
                              const std::vector<int>& durations_ms) {
    if (notes.size() != durations_ms.size()) {
        return false;
    }
    
    for (size_t i = 0; i < notes.size(); i++) {
        // Parse note
        std::string note_str = notes[i];
        int octave = 4;
        
        // Extract octave if present
        if (note_str.length() > 1 && isdigit(note_str.back())) {
            octave = note_str.back() - '0';
            note_str = note_str.substr(0, note_str.length() - 1);
        }
        
        playNote(channel, note_str, octave, durations_ms[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(durations_ms[i] / 10));
    }
    
    return true;
}

// ============================================
// LED DIMMING
// ============================================

bool PWMController::setLEDBrightness(Channel channel, int brightness) {
    brightness = std::max(0, std::min(100, brightness));
    return setDutyCycle(channel, brightness);
}

bool PWMController::fadeLED(Channel channel, int start_brightness, 
                           int end_brightness, int duration_ms) {
    int steps = 100;
    int step_duration = duration_ms / steps;
    double step = (end_brightness - start_brightness) / steps;
    
    for (int i = 0; i <= steps; i++) {
        int brightness = start_brightness + step * i;
        setLEDBrightness(channel, brightness);
        std::this_thread::sleep_for(std::chrono::milliseconds(step_duration));
    }
    
    return true;
}

bool PWMController::breatheLED(Channel channel, int duration_ms, 
                              int min_brightness, int max_brightness) {
    // Run in background thread
    std::thread([this, channel, duration_ms, min_brightness, max_brightness]() {
        while (true) {
            // Fade in
            fadeLED(channel, min_brightness, max_brightness, duration_ms / 2);
            // Fade out
            fadeLED(channel, max_brightness, min_brightness, duration_ms / 2);
        }
    }).detach();
    
    return true;
}

// ============================================
// STATISTICS
// ============================================

PWMController::PWMStats PWMController::getStats(Channel channel) const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    auto it = channels.find(channel);
    if (it != channels.end()) {
        return it->second.stats;
    }
    return PWMStats{};
}

std::map<PWMController::Channel, PWMController::PWMStats> PWMController::getAllStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    std::map<Channel, PWMStats> all_stats;
    for (const auto& [channel, data] : channels) {
        all_stats[channel] = data.stats;
    }
    return all_stats;
}

void PWMController::resetStats(Channel channel) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    auto it = channels.find(channel);
    if (it != channels.end()) {
        it->second.stats = PWMStats{};
    }
}

void PWMController::resetAllStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    for (auto& [channel, data] : channels) {
        data.stats = PWMStats{};
    }
}

void PWMController::updateStats(Channel channel) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    auto it = channels.find(channel);
    if (it == channels.end()) {
        return;
    }
    
    PWMStats& stats = it->second.stats;
    stats.total_cycles++;
    stats.last_frequency = it->second.config.frequency;
    stats.last_duty_cycle = it->second.config.duty_cycle;
    stats.frequency_actual = it->second.config.frequency;
    stats.duty_cycle_actual = it->second.config.duty_cycle;
    stats.last_update = std::chrono::system_clock::now();
    stats.total_updates++;
}

void PWMController::notifyStats(Channel channel) {
    if (on_stats) {
        on_stats(channel, getStats(channel));
    }
}

// ============================================
// CALLBACKS
// ============================================

void PWMController::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void PWMController::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void PWMController::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

void PWMController::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "PWM Error: " << error << std::endl;
}

void PWMController::notifyStateChange(Channel channel, bool enabled) {
    if (on_state_change) {
        on_state_change(channel, enabled);
    }
}

// ============================================
// UTILITY
// ============================================

std::string PWMController::getChannelName(Channel channel) const {
    switch (channel) {
        case Channel::CHANNEL_0: return "PWM0 (GPIO12)";
        case Channel::CHANNEL_1: return "PWM1 (GPIO13)";
        case Channel::CHANNEL_2: return "PWM2 (GPIO18)";
        case Channel::CHANNEL_3: return "PWM3 (GPIO19)";
        case Channel::CHANNEL_4: return "PWM4 (GPIO40)";
        case Channel::CHANNEL_5: return "PWM5 (GPIO41)";
        case Channel::CHANNEL_6: return "PWM6 (GPIO42)";
        case Channel::CHANNEL_7: return "PWM7 (GPIO43)";
        default: return "Unknown";
    }
}

std::string PWMController::getModeName(Mode mode) const {
    switch (mode) {
        case Mode::PWM: return "PWM";
        case Mode::SERVO: return "Servo";
        case Mode::TONE: return "Tone";
        case Mode::LED: return "LED Dimming";
        default: return "Unknown";
    }
}

std::string PWMController::getPolarityName(Polarity polarity) const {
    switch (polarity) {
        case Polarity::NORMAL: return "Normal";
        case Polarity::INVERTED: return "Inverted";
        default: return "Unknown";
    }
}

std::string PWMController::getClockSourceName(ClockSource source) const {
    switch (source) {
        case ClockSource::OSCILLATOR: return "Oscillator (19.2 MHz)";
        case ClockSource::PLLD: return "PLLD (500 MHz)";
        case ClockSource::PLLC: return "PLLC (1000 MHz)";
        case ClockSource::PLLA: return "PLLA (500 MHz)";
        default: return "Unknown";
    }
}

bool PWMController::isChannelAvailable(Channel channel) const {
    return channels.find(channel) != channels.end();
}

std::vector<int> PWMController::getAvailablePins(Channel channel) const {
    std::vector<int> pins;
    auto it = channel_pin_map.find(channel);
    if (it != channel_pin_map.end()) {
        pins.push_back(it->second);
    }
    return pins;
}

int PWMController::getPinForChannel(Channel channel) const {
    return channelToPin(channel);
}

int PWMController::channelToPin(Channel channel) const {
    auto it = channel_pin_map.find(channel);
    if (it != channel_pin_map.end()) {
        return it->second;
    }
    return -1;
}

PWMController::Channel PWMController::pinToChannel(int pin) const {
    auto it = pin_channel_map.find(pin);
    if (it != pin_channel_map.end()) {
        return it->second;
    }
    return Channel::CHANNEL_0;
}

bool PWMController::testPWM(Channel channel, int duration_ms) {
    std::cout << "Testing PWM on channel " << getChannelName(channel) << std::endl;
    
    PWMConfig config;
    config.channel = channel;
    config.pin = channelToPin(channel);
    config.frequency = 1000;
    config.duty_cycle = 50;
    config.enabled = true;
    
    configure(config);
    enable(channel);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    
    disable(channel);
    std::cout << "PWM test complete" << std::endl;
    return true;
}

bool PWMController::testServo(Channel channel, int duration_ms) {
    std::cout << "Testing servo on channel " << getChannelName(channel) << std::endl;
    
    configureServo(channel);
    
    // Sweep from 0 to 180 degrees
    sweepServo(channel, 0, 180, duration_ms / 2);
    sweepServo(channel, 180, 0, duration_ms / 2);
    
    centerServo(channel);
    std::cout << "Servo test complete" << std::endl;
    return true;
}

// ============================================
// HELPER FUNCTIONS
// ============================================

int PWMController::dutyToValue(int duty_cycle, uint32_t range) const {
    return (duty_cycle * range) / 100;
}

int PWMController::valueToDuty(int value, uint32_t range) const {
    return (value * 100) / range;
}

int PWMController::angleToPulse(double angle, const ServoConfig& config) const {
    double normalized = (angle - config.min_angle) / (config.max_angle - config.min_angle);
    return config.min_pulse_us + normalized * (config.max_pulse_us - config.min_pulse_us);
}

double PWMController::pulseToAngle(int pulse, const ServoConfig& config) const {
    double normalized = (pulse - config.min_pulse_us) / 
                       (config.max_pulse_us - config.min_pulse_us);
    return config.min_angle + normalized * (config.max_angle - config.min_angle);
}

double PWMController::getNoteFrequency(const std::string& note, int octave) const {
    auto it = note_frequencies.find(note);
    if (it == note_frequencies.end()) {
        return 0;
    }
    
    double freq = it->second;
    // Octave adjustment (A4 = 440Hz)
    int octave_diff = octave - 4;
    return freq * pow(2, octave_diff);
}
