# Find OpenSSL
# For HTTPS and encryption support

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)

if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
    message(STATUS "  Includes: ${OPENSSL_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${OPENSSL_LIBRARIES}")
else()
    # Fallback to manual search
    find_path(OPENSSL_INCLUDE_DIR NAMES openssl/ssl.h)
    find_library(OPENSSL_SSL_LIBRARY NAMES ssl)
    find_library(OPENSSL_CRYPTO_LIBRARY NAMES crypto)
    
    if(OPENSSL_INCLUDE_DIR AND OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
        set(OPENSSL_FOUND TRUE)
        set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
        set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
        message(STATUS "Found OpenSSL (manual)")
    else()
        message(WARNING "OpenSSL not found - cloud features disabled")
    endif()
endif()
