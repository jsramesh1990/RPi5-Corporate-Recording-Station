# Find libcurl
# For HTTP/HTTPS cloud uploads

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

if(CURL_FOUND)
    message(STATUS "Found libcurl: ${CURL_VERSION}")
    message(STATUS "  Includes: ${CURL_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${CURL_LIBRARIES}")
else()
    find_path(CURL_INCLUDE_DIR NAMES curl/curl.h)
    find_library(CURL_LIBRARY NAMES curl)
    
    if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
        set(CURL_FOUND TRUE)
        set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
        set(CURL_LIBRARIES ${CURL_LIBRARY})
        message(STATUS "Found libcurl (manual)")
    else()
        message(WARNING "libcurl not found - cloud uploads disabled")
    endif()
endif()
