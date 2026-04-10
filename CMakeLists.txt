cmake_minimum_required(VERSION 3.22)
project(jsmx C)

set(CMAKE_C_STANDARD 11)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

option(JSMX_ENABLE_REGEX "Enable optional regex backend support" OFF)
option(JSMX_ENABLE_CRYPTO "Enable optional crypto backend support" OFF)

add_library(jsmx STATIC
    jsmn.c
    jsnum.c
    jscrypto.c
    jsval.c
    jsmethod.c
    jsregex.c
)

target_include_directories(jsmx PUBLIC .)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(jsmx PRIVATE -Os -flto)
    target_link_options(jsmx PRIVATE -flto)
endif()

if(JSMX_ENABLE_REGEX)
    find_package(pcre2 CONFIG REQUIRED)
    target_compile_definitions(jsmx PUBLIC
        JSMX_WITH_REGEX=1
        JSMX_REGEX_BACKEND_PCRE2=1
    )
    target_link_libraries(jsmx PUBLIC PCRE2::16BIT)
endif()

if(JSMX_ENABLE_CRYPTO)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(jsmx PUBLIC
        JSMX_WITH_CRYPTO=1
        JSMX_CRYPTO_BACKEND_OPENSSL=1
    )
    target_link_libraries(jsmx PUBLIC OpenSSL::Crypto)
endif()
