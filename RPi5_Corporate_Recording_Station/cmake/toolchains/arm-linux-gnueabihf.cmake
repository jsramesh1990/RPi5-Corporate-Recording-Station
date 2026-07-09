# CMake Toolchain File for ARMHF Cross-Compilation
# Target: Older Raspberry Pi models
# Host: x86_64 Linux

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_AR arm-linux-gnueabihf-ar)
set(CMAKE_LINKER arm-linux-gnueabihf-ld)
set(CMAKE_NM arm-linux-gnueabihf-nm)
set(CMAKE_OBJCOPY arm-linux-gnueabihf-objcopy)
set(CMAKE_OBJDUMP arm-linux-gnueabihf-objdump)
set(CMAKE_RANLIB arm-linux-gnueabihf-ranlib)
set(CMAKE_STRIP arm-linux-gnueabihf-strip)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -std=c++17")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")

set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_PTHREADS_INIT 1)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -ggdb3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()

add_definitions(-DRASPBERRY_PI=4)
add_definitions(-DARMHF=1)
add_definitions(-D__linux__=1)

message(STATUS "Cross-compiling for ARMHF (Raspberry Pi 4)")
