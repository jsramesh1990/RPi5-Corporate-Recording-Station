# CMake Options Configuration
# RPi5 Corporate Recording Station

# Build options
option(BUILD_TESTS "Build unit and integration tests" ON)
option(BUILD_DOCS "Build documentation with Doxygen" OFF)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(BUILD_EXAMPLES "Build example applications" OFF)

# Feature options
option(ENABLE_GPIO "Enable GPIO support" ON)
option(ENABLE_UART "Enable UART support" ON)
option(ENABLE_I2C "Enable I2C support" ON)
option(ENABLE_SPI "Enable SPI support" ON)
option(ENABLE_PWM "Enable PWM support" ON)
option(ENABLE_CAMERA "Enable Camera support" ON)
option(ENABLE_AUDIO "Enable Audio support" ON)
option(ENABLE_PCIE "Enable PCIe support" ON)
option(ENABLE_NETWORK "Enable Network support" ON)
option(ENABLE_CLOUD "Enable Cloud upload support" ON)

# Performance options
option(ENABLE_PROFILING "Enable performance profiling" OFF)
option(ENABLE_COVERAGE "Enable code coverage tracking" OFF)
option(ENABLE_SANITIZERS "Enable memory sanitizers" OFF)
option(ENABLE_OPTIMIZATIONS "Enable aggressive optimizations" ON)

# Hardware specific
option(USE_HARDWARE_ENCODING "Use hardware video encoding" ON)
option(USE_ZRAM "Enable ZRAM compression" ON)
option(USE_MMU "Enable MMU memory protection" ON)
option(USE_PAGING "Enable memory paging" ON)

# Debug options
option(ENABLE_VERBOSE_LOGGING "Enable detailed logging" OFF)
option(ENABLE_DEBUG_CONSOLE "Enable debug console" OFF)
option(ENABLE_TRACE "Enable function tracing" OFF)

# Platform detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(PLATFORM_ARM64 TRUE)
    message(STATUS "Building for ARM64 (Raspberry Pi 5)")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l|armhf")
    set(PLATFORM_ARMHF TRUE)
    message(STATUS "Building for ARMHF")
else()
    set(PLATFORM_X86 TRUE)
    message(STATUS "Building for x86_64 (development only)")
endif()

# Build type specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -ggdb3")
    message(STATUS "Debug build enabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DNDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
    message(STATUS "Release build enabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")
    message(STATUS "Release with debug info enabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_definitions(-DNDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -DNDEBUG")
    message(STATUS "Minimal size build enabled")
endif()

# Compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wno-unused-parameter")

# Architecture specific optimizations
if(PLATFORM_ARM64)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.2-a+crypto -mtune=cortex-a76")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8.2-a+crypto -mtune=cortex-a76")
    
    # ARM NEON optimizations
    if(ENABLE_OPTIMIZATIONS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftree-vectorize -fopenmp -ffast-math")
    endif()
endif()

# Sanitizers
if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -fsanitize=leak")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fsanitize=leak")
endif()

# Coverage
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} --coverage")
endif()

# Profiling
if(ENABLE_PROFILING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -g")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -pg")
endif()

# Verbose logging
if(ENABLE_VERBOSE_LOGGING)
    add_definitions(-DVERBOSE_LOGGING)
endif()

# Debug console
if(ENABLE_DEBUG_CONSOLE)
    add_definitions(-DDEBUG_CONSOLE)
endif()

# Trace
if(ENABLE_TRACE)
    add_definitions(-DTRACE_ENABLED)
endif()

# Print configuration summary
message(STATUS "Configuration Summary:")
message(STATUS "  Platform: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Build Docs: ${BUILD_DOCS}")
message(STATUS "  GPIO Support: ${ENABLE_GPIO}")
message(STATUS "  UART Support: ${ENABLE_UART}")
message(STATUS "  I2C Support: ${ENABLE_I2C}")
message(STATUS "  SPI Support: ${ENABLE_SPI}")
message(STATUS "  PWM Support: ${ENABLE_PWM}")
message(STATUS "  Camera Support: ${ENABLE_CAMERA}")
message(STATUS "  Audio Support: ${ENABLE_AUDIO}")
message(STATUS "  PCIe Support: ${ENABLE_PCIE}")
message(STATUS "  Cloud Support: ${ENABLE_CLOUD}")
message(STATUS "  Hardware Encoding: ${USE_HARDWARE_ENCODING}")
message(STATUS "  ZRAM Compression: ${USE_ZRAM}")
message(STATUS "  MMU Protection: ${USE_MMU}")
