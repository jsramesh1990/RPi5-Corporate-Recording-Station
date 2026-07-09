#ifndef BUILD_CONFIG_HPP
#define BUILD_CONFIG_HPP

// ============================================
// BUILD INFORMATION
// ============================================

#define PROJECT_NAME "RPi5 Corporate Recording Station"
#define PROJECT_VERSION_MAJOR 2
#define PROJECT_VERSION_MINOR 0
#define PROJECT_VERSION_PATCH 0
#define PROJECT_VERSION "2.0.0"

// ============================================
// COMPILER INFORMATION
// ============================================

#ifdef __GNUC__
    #define COMPILER_NAME "GCC"
    #define COMPILER_VERSION __VERSION__
#elif defined(__clang__)
    #define COMPILER_NAME "Clang"
    #define COMPILER_VERSION __clang_version__
#elif defined(_MSC_VER)
    #define COMPILER_NAME "MSVC"
    #define COMPILER_VERSION _MSC_VER
#else
    #define COMPILER_NAME "Unknown"
    #define COMPILER_VERSION "Unknown"
#endif

// ============================================
// PLATFORM DETECTION
// ============================================

#ifdef __linux__
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
    #define PLATFORM_DARWIN 1
    #define PLATFORM_NAME "macOS"
#elif defined(_WIN32)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_NAME "Windows"
#else
    #define PLATFORM_NAME "Unknown"
#endif

#ifdef __aarch64__
    #define PLATFORM_ARM64 1
    #define PLATFORM_ARCH "ARM64"
#elif defined(__arm__)
    #define PLATFORM_ARM 1
    #define PLATFORM_ARCH "ARM"
#elif defined(__x86_64__)
    #define PLATFORM_X86_64 1
    #define PLATFORM_ARCH "x86_64"
#else
    #define PLATFORM_ARCH "Unknown"
#endif

// ============================================
// RASPBERRY PI DETECTION
// ============================================

#ifdef RASPBERRY_PI
    #define PLATFORM_RASPBERRY_PI 1
    #define PLATFORM_RPI_VERSION RASPBERRY_PI
#else
    #define PLATFORM_RASPBERRY_PI 0
#endif

// ============================================
// BUILD CONFIGURATION
// ============================================

#ifdef DEBUG
    #define BUILD_TYPE "Debug"
    #define DEBUG_MODE 1
#elif defined(NDEBUG)
    #define BUILD_TYPE "Release"
    #define DEBUG_MODE 0
#else
    #define BUILD_TYPE "Unknown"
    #define DEBUG_MODE 0
#endif

// ============================================
// FEATURE FLAGS
// ============================================

#ifdef ENABLE_GPIO
    #define FEATURE_GPIO 1
#else
    #define FEATURE_GPIO 0
#endif

#ifdef ENABLE_UART
    #define FEATURE_UART 1
#else
    #define FEATURE_UART 0
#endif

#ifdef ENABLE_I2C
    #define FEATURE_I2C 1
#else
    #define FEATURE_I2C 0
#endif

#ifdef ENABLE_SPI
    #define FEATURE_SPI 1
#else
    #define FEATURE_SPI 0
#endif

#ifdef ENABLE_PWM
    #define FEATURE_PWM 1
#else
    #define FEATURE_PWM 0
#endif

#ifdef ENABLE_CAMERA
    #define FEATURE_CAMERA 1
#else
    #define FEATURE_CAMERA 0
#endif

#ifdef ENABLE_AUDIO
    #define FEATURE_AUDIO 1
#else
    #define FEATURE_AUDIO 0
#endif

#ifdef ENABLE_CLOUD
    #define FEATURE_CLOUD 1
#else
    #define FEATURE_CLOUD 0
#endif

#ifdef ENABLE_PROFILING
    #define FEATURE_PROFILING 1
#else
    #define FEATURE_PROFILING 0
#endif

// ============================================
// STRING HELPERS
// ============================================

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// ============================================
// BUILD INFO STRUCTURE
// ============================================

struct BuildInfo {
    static constexpr const char* project_name = PROJECT_NAME;
    static constexpr const char* project_version = PROJECT_VERSION;
    static constexpr int version_major = PROJECT_VERSION_MAJOR;
    static constexpr int version_minor = PROJECT_VERSION_MINOR;
    static constexpr int version_patch = PROJECT_VERSION_PATCH;
    static constexpr const char* build_type = BUILD_TYPE;
    static constexpr const char* compiler_name = COMPILER_NAME;
    static constexpr const char* compiler_version = COMPILER_VERSION;
    static constexpr const char* platform = PLATFORM_NAME;
    static constexpr const char* architecture = PLATFORM_ARCH;
    static constexpr const char* build_date = __DATE__;
    static constexpr const char* build_time = __TIME__;
    
    static constexpr bool is_debug = DEBUG_MODE != 0;
    static constexpr bool is_raspberry_pi = PLATFORM_RASPBERRY_PI != 0;
    static constexpr int rpi_version = PLATFORM_RPI_VERSION;
    
    // Feature flags
    static constexpr bool has_gpio = FEATURE_GPIO != 0;
    static constexpr bool has_uart = FEATURE_UART != 0;
    static constexpr bool has_i2c = FEATURE_I2C != 0;
    static constexpr bool has_spi = FEATURE_SPI != 0;
    static constexpr bool has_pwm = FEATURE_PWM != 0;
    static constexpr bool has_camera = FEATURE_CAMERA != 0;
    static constexpr bool has_audio = FEATURE_AUDIO != 0;
    static constexpr bool has_cloud = FEATURE_CLOUD != 0;
    static constexpr bool has_profiling = FEATURE_PROFILING != 0;
    
    /**
     * @brief Get build information as string
     */
    static std::string getInfo() {
        std::string info;
        info += "Project: " + std::string(project_name) + "\n";
        info += "Version: " + std::string(project_version) + "\n";
        info += "Build: " + std::string(build_type) + "\n";
        info += "Platform: " + std::string(platform) + " (" + std::string(architecture) + ")\n";
        info += "Compiler: " + std::string(compiler_name) + " " + std::string(compiler_version) + "\n";
        info += "Built: " + std::string(build_date) + " " + std::string(build_time) + "\n";
        info += "Features: ";
        if (has_gpio) info += "GPIO ";
        if (has_uart) info += "UART ";
        if (has_i2c) info += "I2C ";
        if (has_spi) info += "SPI ";
        if (has_pwm) info += "PWM ";
        if (has_camera) info += "Camera ";
        if (has_audio) info += "Audio ";
        if (has_cloud) info += "Cloud ";
        if (has_profiling) info += "Profiling";
        info += "\n";
        return info;
    }
};

#endif // BUILD_CONFIG_HPP
