# Find jsoncpp library
# For JSON configuration and API handling

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

if(JSONCPP_FOUND)
    message(STATUS "Found jsoncpp: ${JSONCPP_VERSION}")
    message(STATUS "  Includes: ${JSONCPP_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${JSONCPP_LIBRARIES}")
else()
    find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h)
    find_library(JSONCPP_LIBRARY NAMES jsoncpp)
    
    if(JSONCPP_INCLUDE_DIR AND JSONCPP_LIBRARY)
        set(JSONCPP_FOUND TRUE)
        set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})
        set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
        message(STATUS "Found jsoncpp (manual)")
    else()
        message(WARNING "jsoncpp not found - JSON features disabled")
    endif()
endif()
