cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VERSION_RAW)
string(STRIP "${VERSION_RAW}" PROJECT_VERSION_CLEAN)

project(Metis_Exterminator_Plus VERSION ${PROJECT_VERSION_CLEAN} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_SUPPRESS_REGENERATION true)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.hpp.in"
    "${CMAKE_BINARY_DIR}/version.hpp"
    @ONLY
)

# OpenSSL prebuilt selection
if(WIN32)
    set(OPENSSL_DIR "${CMAKE_SOURCE_DIR}/openssl-prebuilt/windows")
elseif(APPLE)
    set(OPENSSL_DIR "${CMAKE_SOURCE_DIR}/openssl-prebuilt/macos")
else()
    set(OPENSSL_DIR "${CMAKE_SOURCE_DIR}/openssl-prebuilt/linux")
endif()

find_library(SSL_LIB    NAMES ssl    libssl    PATHS "${OPENSSL_DIR}/lib" NO_DEFAULT_PATH)
find_library(CRYPTO_LIB NAMES crypto libcrypto PATHS "${OPENSSL_DIR}/lib" NO_DEFAULT_PATH)

# Locate OpenSSL headers - try four locations in order:
#   1. Bundled prebuilt include (populated if user copies headers in)
#   2. CLion bundled MinGW OpenSSL headers (Windows)
#   3. Homebrew / system paths (macOS/Linux)
#   4. Hard error with a helpful message
set(OPENSSL_INCLUDE_DIR "")

if(EXISTS "${OPENSSL_DIR}/include/openssl/ssl.h")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_DIR}/include")
    message(STATUS "OpenSSL headers: bundled prebuilt")
endif()

if(NOT OPENSSL_INCLUDE_DIR AND WIN32)
    # CLion ships MinGW-w64; OpenSSL headers are alongside the compiler
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(MINGW_ROOT    "${MINGW_BIN_DIR}" DIRECTORY)
    set(_candidates
        "${MINGW_ROOT}/x86_64-w64-mingw32/include"
        "${MINGW_ROOT}/include"
        "${MINGW_ROOT}/opt/include"
        "C:/msys64/mingw64/include"
        "C:/msys2/mingw64/include"
    )
    foreach(_c ${_candidates})
        if(EXISTS "${_c}/openssl/ssl.h")
            set(OPENSSL_INCLUDE_DIR "${_c}")
            message(STATUS "OpenSSL headers: MinGW system (${_c})")
            break()
        endif()
    endforeach()
endif()

if(NOT OPENSSL_INCLUDE_DIR AND NOT WIN32)
    foreach(_c
        "/usr/include"
        "/usr/local/include"
        "/opt/homebrew/include"
        "/usr/local/opt/openssl/include"
        "/opt/homebrew/opt/openssl/include"
    )
        if(EXISTS "${_c}/openssl/ssl.h")
            set(OPENSSL_INCLUDE_DIR "${_c}")
            message(STATUS "OpenSSL headers: system (${_c})")
            break()
        endif()
    endforeach()
endif()

if(NOT OPENSSL_INCLUDE_DIR)
    message(FATAL_ERROR
        "OpenSSL headers not found.\n"
        "On Windows with CLion/MinGW: copy OpenSSL headers to openssl-prebuilt/windows/include/\n"
        "  or install: pacman -S mingw-w64-x86_64-openssl  (in MSYS2)\n"
        "On Linux: sudo apt install libssl-dev\n"
        "On macOS: brew install openssl")
endif()

# If bundled .a libs were not found, fall back to system link libs
if(NOT SSL_LIB OR NOT CRYPTO_LIB)
    if(WIN32)
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(MINGW_ROOT    "${MINGW_BIN_DIR}" DIRECTORY)
        find_library(SSL_LIB    NAMES ssl    libssl
            PATHS "${MINGW_ROOT}/x86_64-w64-mingw32/lib" "${MINGW_ROOT}/lib"
                  "C:/msys64/mingw64/lib" "C:/msys2/mingw64/lib"
            NO_DEFAULT_PATH)
        find_library(CRYPTO_LIB NAMES crypto libcrypto
            PATHS "${MINGW_ROOT}/x86_64-w64-mingw32/lib" "${MINGW_ROOT}/lib"
                  "C:/msys64/mingw64/lib" "C:/msys2/mingw64/lib"
            NO_DEFAULT_PATH)
    endif()
    if(NOT SSL_LIB OR NOT CRYPTO_LIB)
        find_package(OpenSSL QUIET)
        if(OpenSSL_FOUND)
            set(SSL_LIB    OpenSSL::SSL)
            set(CRYPTO_LIB OpenSSL::Crypto)
            message(STATUS "OpenSSL libs: CMake find_package")
        else()
            message(STATUS "OpenSSL libs: not found - TLS will be disabled at runtime")
        endif()
    else()
        message(STATUS "OpenSSL libs: MinGW system")
    endif()
else()
    message(STATUS "OpenSSL libs: bundled prebuilt")
endif()

add_executable(Metis_Exterminator_Plus
    src/main.cpp
    src/server.cpp
    src/router.cpp
    src/pson.cpp
    src/database.cpp
    src/auth.cpp
    src/customer_store.cpp
    src/job_store.cpp
    src/invoice_store.cpp
    third_party/sqlite/sqlite3.c
    third_party/bcrypt/bcrypt.c
    third_party/bcrypt/crypt_blowfish/crypt_blowfish.c
    third_party/bcrypt/crypt_blowfish/crypt_gensalt.c
    third_party/bcrypt/crypt_blowfish/crypt.c
)

target_include_directories(Metis_Exterminator_Plus PRIVATE
    src
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}/third_party/sqlite"
    "${CMAKE_SOURCE_DIR}/third_party/bcrypt"
    "${CMAKE_SOURCE_DIR}/third_party/nlohmann"
    "${CMAKE_SOURCE_DIR}/infra"
    "${OPENSSL_INCLUDE_DIR}"
)

if(WIN32)
    set_source_files_properties(
        third_party/sqlite/sqlite3.c
        PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3_win32_fix.h"
    )
endif()

target_compile_definitions(Metis_Exterminator_Plus PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_WIN32_NO_ANSI
    $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN>
    $<$<BOOL:${WIN32}>:NOMINMAX>
    $<$<BOOL:${WIN32}>:NOGDI>
)

if(WIN32)
    # Link order matters for MinGW: OpenSSL .a files FIRST, then the Windows
    # system libs they depend on (ws2_32, crypt32, mswsock, iphlpapi).
    # Reversing this order causes hundreds of undefined __imp_WSAGetLastError etc.
    if(SSL_LIB AND CRYPTO_LIB)
        target_link_libraries(Metis_Exterminator_Plus
            "${SSL_LIB}"
            "${CRYPTO_LIB}"
            ws2_32
            crypt32
            mswsock
            iphlpapi
        )
    else()
        target_link_libraries(Metis_Exterminator_Plus ws2_32 crypt32 mswsock iphlpapi)
    endif()
elseif(APPLE)
    if(SSL_LIB AND CRYPTO_LIB)
        target_link_libraries(Metis_Exterminator_Plus "${SSL_LIB}" "${CRYPTO_LIB}")
    endif()
else()
    target_link_libraries(Metis_Exterminator_Plus pthread dl)
    if(SSL_LIB AND CRYPTO_LIB)
        target_link_libraries(Metis_Exterminator_Plus "${SSL_LIB}" "${CRYPTO_LIB}")
    endif()
endif()

add_custom_command(TARGET Metis_Exterminator_Plus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:Metis_Exterminator_Plus>/web"
    COMMENT "Copying web assets"
)
add_custom_command(TARGET Metis_Exterminator_Plus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:Metis_Exterminator_Plus>/config"
    COMMENT "Copying config"
)
add_custom_command(TARGET Metis_Exterminator_Plus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/tls"
        "$<TARGET_FILE_DIR:Metis_Exterminator_Plus>/tls"
    COMMENT "Copying TLS certs"
)
