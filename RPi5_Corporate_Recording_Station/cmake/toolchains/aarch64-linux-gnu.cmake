# CMake Toolchain File for ARM64 Cross-Compilation
# Target: Raspberry Pi 5 (ARM Cortex-A76)
# Host: x86_64 Linux

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Set cross-compilation prefix
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_AR aarch64-linux-gnu-ar)
set(CMAKE_LINKER aarch64-linux-gnu-ld)
set(CMAKE_NM aarch64-linux-gnu-nm)
set(CMAKE_OBJCOPY aarch64-linux-gnu-objcopy)
set(CMAKE_OBJDUMP aarch64-linux-gnu-objdump)
set(CMAKE_RANLIB aarch64-linux-gnu-ranlib)
set(CMAKE_STRIP aarch64-linux-gnu-strip)

# Set compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8.2-a+crypto -mtune=cortex-a76")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.2-a+crypto -mtune=cortex-a76 -std=c++17")

# Set linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")

# Set sysroot (if using a Raspberry Pi OS root filesystem)
# set(CMAKE_SYSROOT /path/to/raspberry-pi-rootfs)

# Search paths for libraries
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)

# Tell CMake to search for libraries in the target system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Threading
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_PTHREADS_INIT 1)

# Hard float ABI for Raspberry Pi
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard")

# Additional optimization flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftree-vectorize -fopenmp")

# Debug symbols
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -ggdb3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()

# Raspberry Pi specific defines
add_definitions(-DRASPBERRY_PI=5)
add_definitions(-DARM64=1)
add_definitions(-D__linux__=1)

message(STATUS "Cross-compiling for ARM64 (Raspberry Pi 5)")
