# Find WebSocket library (libwebsockets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBSOCKETS REQUIRED libwebsockets)

if(WEBSOCKETS_FOUND)
    message(STATUS "Found libwebsockets: ${WEBSOCKETS_VERSION}")
    message(STATUS "  Includes: ${WEBSOCKETS_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${WEBSOCKETS_LIBRARIES}")
else()
    # Fallback search
    find_path(WEBSOCKETS_INCLUDE_DIR NAMES libwebsockets.h)
    find_library(WEBSOCKETS_LIBRARY NAMES websockets)
    
    if(WEBSOCKETS_INCLUDE_DIR AND WEBSOCKETS_LIBRARY)
        set(WEBSOCKETS_FOUND TRUE)
        set(WEBSOCKETS_INCLUDE_DIRS ${WEBSOCKETS_INCLUDE_DIR})
        set(WEBSOCKETS_LIBRARIES ${WEBSOCKETS_LIBRARY})
        message(STATUS "Found libwebsockets (manual)")
    else()
        message(WARNING "libwebsockets not found - WebSocket features disabled")
    endif()
endif()
