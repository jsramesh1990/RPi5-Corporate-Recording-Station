#ifndef PWM_CONTROLLER_HPP
#define PWM_CONTROLLER_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>

/**
 * @brief PWM Controller for Raspberry Pi 5
 * 
 * Provides PWM (Pulse Width Modulation) control with support for
 * hardware PWM channels, software PWM, and servo control.
 * Optimized for BCM2712 on Raspberry Pi 5.
 */
class PWMController {
public:
    // ============================================
    // TYPES & ENUMS
    // ============================================
    
    enum class Channel {
        CHANNEL_0 = 0,
        CHANNEL_1 = 1,
        CHANNEL_2 = 2,
        CHANNEL_3 = 3,
        CHANNEL_4 = 4,
        CHANNEL_5 = 5,
        CHANNEL_6 = 6,
        CHANNEL_7 = 7
    };
    
    enum class Mode {
        PWM,        // Standard PWM
        SERVO,      // Servo control (50Hz)
        TONE,       // Tone generation
        LED         // LED dimming
    };
    
    enum class Polarity {
        NORMAL,     // High = on
        INVERTED    // Low = on
    };
    
    enum class ClockSource {
        OSCILLATOR, // 19.2 MHz oscillator
        PLLD,       // PLLD 500 MHz
        PLLC,       // PLLC 1000 MHz
        PLLA        // PLLA 500 MHz
    };
    
    // ============================================
    // STRUCTURES
    // ============================================
    
    /**
     * @brief PWM channel configuration
     */
    struct PWMConfig {
        Channel channel = Channel::CHANNEL_0;
        int pin = -1;
        Mode mode = Mode::PWM;
        Polarity polarity = Polarity::NORMAL;
        int frequency = 1000;        // Hz
        int duty_cycle = 50;         // Percentage (0-100)
        uint32_t range = 1024;       // PWM range
        uint32_t clock_divisor = 1;
        ClockSource clock_source = ClockSource::OSCILLATOR;
        bool enabled = false;
        bool inverted = false;
        std::string name = "";
    };
    
    /**
     * @brief PWM statistics
     */
    struct PWMStats {
        uint64_t total_cycles = 0;
        uint64_t on_cycles = 0;
        uint64_t off_cycles = 0;
        uint64_t overflows = 0;
        uint32_t last_frequency = 0;
        uint32_t last_duty_cycle = 0;
        double duty_cycle_actual = 0;
        double frequency_actual = 0;
        std::chrono::system_clock::time_point last_update;
        uint64_t total_updates = 0;
    };
    
    /**
     * @brief Servo configuration
     */
    struct ServoConfig {
        Channel channel = Channel::CHANNEL_0;
        int min_pulse_us = 500;      // Minimum pulse width (microseconds)
        int max_pulse_us = 2500;     // Maximum pulse width (microseconds)
        int center_pulse_us = 1500;  // Center pulse width (microseconds)
        int frequency = 50;          // Hz (20ms period)
        double min_angle = 0;        // Degrees
        double max_angle = 180;      // Degrees
        double current_angle = 90;   // Degrees
    };
    
    // ============================================
    // CALLBACKS
    // ============================================
    
    using ErrorCallback = std::function<void(const std::string&)>;
    using StateCallback = std::function<void(Channel, bool)>;
    using StatsCallback = std::function<void(Channel, const PWMStats&)>;
    
    // ============================================
    // SINGLETON
    // ============================================
    
    static PWMController& getInstance();
    
    // ============================================
    // INITIALIZATION
    // ============================================
    
    /**
     * @brief Initialize PWM controller
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Shutdown PWM controller
     */
    void shutdown();
    
    // ============================================
    // PWM CHANNEL MANAGEMENT
    // ============================================
    
    /**
     * @brief Configure PWM channel
     * @param config PWM configuration
     * @return true if successful
     */
    bool configure(const PWMConfig& config);
    
    /**
     * @brief Configure PWM with parameters
     * @param channel Channel
     * @param pin GPIO pin
     * @param frequency Frequency in Hz
     * @param duty_cycle Duty cycle (0-100)
     * @param mode PWM mode
     * @return true if successful
     */
    bool configure(Channel channel, int pin, int frequency = 1000, 
                   int duty_cycle = 50, Mode mode = Mode::PWM);
    
    /**
     * @brief Enable PWM channel
     * @param channel Channel
     * @return true if successful
     */
    bool enable(Channel channel);
    
    /**
     * @brief Disable PWM channel
     * @param channel Channel
     * @return true if successful
     */
    bool disable(Channel channel);
    
    /**
     * @brief Check if channel is enabled
     */
    bool isEnabled(Channel channel) const;
    
    /**
     * @brief Get channel configuration
     */
    PWMConfig getConfig(Channel channel) const;
    
    /**
     * @brief Get all channel configurations
     */
    std::vector<PWMConfig> getAllConfigs() const;
    
    // ============================================
    // PWM CONTROL
    // ============================================
    
    /**
     * @brief Set frequency
     * @param channel Channel
     * @param frequency Frequency in Hz
     * @return true if successful
     */
    bool setFrequency(Channel channel, int frequency);
    
    /**
     * @brief Set duty cycle
     * @param channel Channel
     * @param duty_cycle Duty cycle (0-100)
     * @return true if successful
     */
    bool setDutyCycle(Channel channel, int duty_cycle);
    
    /**
     * @brief Set duty cycle by pulse width
     * @param channel Channel
     * @param pulse_width_us Pulse width in microseconds
     * @return true if successful
     */
    bool setPulseWidth(Channel channel, int pulse_width_us);
    
    /**
     * @brief Set range
     * @param channel Channel
     * @param range PWM range
     * @return true if successful
     */
    bool setRange(Channel channel, uint32_t range);
    
    /**
     * @brief Set polarity
     * @param channel Channel
     * @param polarity Polarity
     * @return true if successful
     */
    bool setPolarity(Channel channel, Polarity polarity);
    
    /**
     * @brief Set mode
     * @param channel Channel
     * @param mode PWM mode
     * @return true if successful
     */
    bool setMode(Channel channel, Mode mode);
    
    /**
     * @brief Get frequency
     */
    int getFrequency(Channel channel) const;
    
    /**
     * @brief Get duty cycle
     */
    int getDutyCycle(Channel channel) const;
    
    /**
     * @brief Get mode
     */
    Mode getMode(Channel channel) const;
    
    // ============================================
    // SERVO CONTROL
    // ============================================
    
    /**
     * @brief Configure servo
     * @param config Servo configuration
     * @return true if successful
     */
    bool configureServo(const ServoConfig& config);
    
    /**
     * @brief Configure servo with parameters
     * @param channel Channel
     * @param min_pulse Minimum pulse width (us)
     * @param max_pulse Maximum pulse width (us)
     * @param center_pulse Center pulse width (us)
     * @return true if successful
     */
    bool configureServo(Channel channel, int min_pulse = 500, 
                        int max_pulse = 2500, int center_pulse = 1500);
    
    /**
     * @brief Set servo angle
     * @param channel Channel
     * @param angle Angle in degrees
     * @return true if successful
     */
    bool setServoAngle(Channel channel, double angle);
    
    /**
     * @brief Set servo pulse width
     * @param channel Channel
     * @param pulse_width_us Pulse width in microseconds
     * @return true if successful
     */
    bool setServoPulse(Channel channel, int pulse_width_us);
    
    /**
     * @brief Get servo configuration
     */
    ServoConfig getServoConfig(Channel channel) const;
    
    /**
     * @brief Get servo angle
     */
    double getServoAngle(Channel channel) const;
    
    /**
     * @brief Center servo
     */
    bool centerServo(Channel channel);
    
    /**
     * @brief Sweep servo
     * @param channel Channel
     * @param start_angle Start angle
     * @param end_angle End angle
     * @param duration_ms Sweep duration
     * @return true if successful
     */
    bool sweepServo(Channel channel, double start_angle, double end_angle, int duration_ms);
    
    // ============================================
    // TONE GENERATION
    // ============================================
    
    /**
     * @brief Play a tone
     * @param channel Channel
     * @param frequency Frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param volume Volume (0-100)
     * @return true if successful
     */
    bool playTone(Channel channel, int frequency, int duration_ms, int volume = 50);
    
    /**
     * @brief Play a note
     * @param channel Channel
     * @param note Note name (A-G with optional # or b)
     * @param octave Octave number
     * @param duration_ms Duration in milliseconds
     * @param volume Volume (0-100)
     * @return true if successful
     */
    bool playNote(Channel channel, const std::string& note, int octave, 
                  int duration_ms, int volume = 50);
    
    /**
     * @brief Stop tone
     * @param channel Channel
     */
    void stopTone(Channel channel);
    
    /**
     * @brief Play melody (blocking)
     * @param channel Channel
     * @param notes Vector of notes
     * @param durations_ms Vector of durations
     * @return true if successful
     */
    bool playMelody(Channel channel, const std::vector<std::string>& notes, 
                    const std::vector<int>& durations_ms);
    
    // ============================================
    // LED DIMMING
    // ============================================
    
    /**
     * @brief Set LED brightness
     * @param channel Channel
     * @param brightness Brightness (0-100)
     * @return true if successful
     */
    bool setLEDBrightness(Channel channel, int brightness);
    
    /**
     * @brief Fade LED
     * @param channel Channel
     * @param start_brightness Starting brightness
     * @param end_brightness Ending brightness
     * @param duration_ms Fade duration
     * @return true if successful
     */
    bool fadeLED(Channel channel, int start_brightness, int end_brightness, int duration_ms);
    
    /**
     * @brief Breathe LED (smooth pulse)
     * @param channel Channel
     * @param duration_ms Cycle duration
     * @param min_brightness Minimum brightness
     * @param max_brightness Maximum brightness
     * @return true if successful
     */
    bool breatheLED(Channel channel, int duration_ms = 2000, 
                    int min_brightness = 0, int max_brightness = 100);
    
    // ============================================
    // STATISTICS
    // ============================================
    
    /**
     * @brief Get channel statistics
     */
    PWMStats getStats(Channel channel) const;
    
    /**
     * @brief Get all channel statistics
     */
    std::map<Channel, PWMStats> getAllStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats(Channel channel);
    
    /**
     * @brief Reset all statistics
     */
    void resetAllStats();
    
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
     * @brief Get channel name
     */
    std::string getChannelName(Channel channel) const;
    
    /**
     * @brief Get mode name
     */
    std::string getModeName(Mode mode) const;
    
    /**
     * @brief Get polarity name
     */
    std::string getPolarityName(Polarity polarity) const;
    
    /**
     * @brief Get clock source name
     */
    std::string getClockSourceName(ClockSource source) const;
    
    /**
     * @brief Check if channel is available
     */
    bool isChannelAvailable(Channel channel) const;
    
    /**
     * @brief Get available pins for channel
     */
    std::vector<int> getAvailablePins(Channel channel) const;
    
    /**
     * @brief Get pin for channel
     */
    int getPinForChannel(Channel channel) const;
    
    /**
     * @brief Channel to pin mapping
     */
    int channelToPin(Channel channel) const;
    
    /**
     * @brief Pin to channel mapping
     */
    Channel pinToChannel(int pin) const;
    
    /**
     * @brief Test PWM output
     */
    bool testPWM(Channel channel, int duration_ms = 1000);
    
    /**
     * @brief Test servo
     */
    bool testServo(Channel channel, int duration_ms = 2000);
    
private:
    // ============================================
    // PRIVATE MEMBERS
    // ============================================
    
    PWMController();
    ~PWMController();
    PWMController(const PWMController&) = delete;
    PWMController& operator=(const PWMController&) = delete;
    
    bool initialized = false;
    mutable std::mutex mutex;
    mutable std::mutex stats_mutex;
    
    struct PWMChannelData {
        PWMConfig config;
        PWMStats stats;
        bool enabled = false;
        ServoConfig servo_config;
        std::thread thread;
        std::atomic<bool> running;
        std::atomic<bool> thread_active;
        int current_value = 0;
        double target_value = 0;
        std::chrono::steady_clock::time_point last_update;
    };
    
    std::map<Channel, PWMChannelData> channels;
    
    ErrorCallback on_error;
    StateCallback on_state_change;
    StatsCallback on_stats;
    
    // GPIO mapping for PWM channels on Raspberry Pi 5
    static const std::map<Channel, int> channel_pin_map;
    static const std::map<int, Channel> pin_channel_map;
    
    // ============================================
    // PRIVATE METHODS
    // ============================================
    
    bool setupHardwarePWM(Channel channel);
    bool setupSoftwarePWM(Channel channel);
    void shutdownPWM(Channel channel);
    void pwmThreadFunction(Channel channel);
    void updatePWMOutput(Channel channel);
    
    bool setHardwareFrequency(Channel channel, int frequency);
    bool setHardwareDutyCycle(Channel channel, int duty_cycle);
    bool setHardwareRange(Channel channel, uint32_t range);
    
    bool setSoftwareFrequency(Channel channel, int frequency);
    bool setSoftwareDutyCycle(Channel channel, int duty_cycle);
    
    void updateStats(Channel channel);
    void notifyStats(Channel channel);
    void notifyError(const std::string& error);
    void notifyStateChange(Channel channel, bool enabled);
    
    int dutyToValue(int duty_cycle, uint32_t range) const;
    int valueToDuty(int value, uint32_t range) const;
    int angleToPulse(double angle, const ServoConfig& config) const;
    double pulseToAngle(int pulse, const ServoConfig& config) const;
    
    // PWM register definitions for BCM2712
    static constexpr uint32_t PWM_BASE = 0xFE20C000;
    static constexpr uint32_t PWM_SIZE = 0x1000;
    
    struct PWMRegisters {
        volatile uint32_t CTL;
        volatile uint32_t STA;
        volatile uint32_t DMAC;
        volatile uint32_t RNG1;
        volatile uint32_t DAT1;
        volatile uint32_t FIF1;
        volatile uint32_t RNG2;
        volatile uint32_t DAT2;
        volatile uint32_t FIF2;
    };
    
    PWMRegisters* regs = nullptr;
    void* pwm_memory = nullptr;
    
    // Software PWM timing
    static constexpr int SOFTWARE_PWM_PERIOD_US = 100; // 10kHz max
    std::chrono::microseconds getPWMInterval(Channel channel) const;
    
    // Note frequencies (Hz)
    static const std::map<std::string, double> note_frequencies;
    double getNoteFrequency(const std::string& note, int octave) const;
};

#endif // PWM_CONTROLLER_HPP
