cmake_minimum_required(VERSION 3.10)
project(RecordingStation VERSION 2.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Find packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Find dependencies
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(WEBSOCKETS REQUIRED libwebsockets)

# If not found, try to find with CMake
if(NOT FFMPEG_FOUND)
    find_package(FFmpeg REQUIRED)
endif()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Main executable
add_executable(recording-station
    src/main.cpp
)

# Link libraries
target_link_libraries(recording-station
    ${FFMPEG_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${WEBSOCKETS_LIBRARIES}
    Threads::Threads
)

# Include directories
target_include_directories(recording-station PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${WEBSOCKETS_INCLUDE_DIRS}
)

# Compiler flags
target_compile_options(recording-station PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -O2
    -march=armv8.2-a+crypto
    -mtune=cortex-a76
)

# Add version information
target_compile_definitions(recording-station PRIVATE
    VERSION="${PROJECT_VERSION}"
)

# Install
install(TARGETS recording-station DESTINATION bin)
install(DIRECTORY config/ DESTINATION /etc/recording-station)
install(DIRECTORY web/ DESTINATION /var/www/recording-station)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Generate config.h
configure_file(
    ${CMAKE_SOURCE_DIR}/include/config/Config.hpp.in
    ${CMAKE_BINARY_DIR}/include/config/Config.hpp
)
