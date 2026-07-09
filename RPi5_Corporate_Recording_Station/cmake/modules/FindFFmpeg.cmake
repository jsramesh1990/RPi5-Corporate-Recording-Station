# Find FFmpeg libraries
# Uses pkg-config for discovery

find_package(PkgConfig REQUIRED)

# Check for each FFmpeg component
set(FFMPEG_COMPONENTS avcodec avformat avutil swscale swresample)

foreach(COMPONENT ${FFMPEG_COMPONENTS})
    pkg_check_modules(FFMPEG_${COMPONENT} REQUIRED lib${COMPONENT})
    if(FFMPEG_${COMPONENT}_FOUND)
        set(FFMPEG_FOUND TRUE)
        list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${COMPONENT}_LIBRARIES})
        list(APPEND FFMPEG_INCLUDE_DIRS ${FFMPEG_${COMPONENT}_INCLUDE_DIRS})
        message(STATUS "Found FFmpeg ${COMPONENT}")
    endif()
endforeach()

if(FFMPEG_FOUND)
    list(REMOVE_DUPLICATES FFMPEG_LIBRARIES)
    list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
    message(STATUS "FFmpeg found successfully")
else()
    message(WARNING "FFmpeg not found - some features may be disabled")
endif()

# Version check
if(FFMPEG_avcodec_FOUND)
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --modversion libavcodec
        OUTPUT_VARIABLE FFMPEG_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "FFmpeg version: ${FFMPEG_VERSION}")
endif()
