cmake_minimum_required(VERSION 3.14)
project(arb_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Try to find Boost first
# Note: beast is header-only, so we only need system component
find_package(Boost QUIET COMPONENTS system)

# If Boost not found, provide helpful error message
if(NOT Boost_FOUND)
    message(STATUS "")
    message(STATUS "==========================================")
    message(STATUS "Boost kütüphanesi bulunamadı!")
    message(STATUS "==========================================")
    message(STATUS "")
    message(STATUS "OTOMATIK YUKLEME ICIN:")
    message(STATUS "  1. vcpkg yukleyin: https://github.com/microsoft/vcpkg")
    message(STATUS "  2. Calistirin: vcpkg install boost-beast boost-system openssl")
    message(STATUS "  3. CMake'i tekrar calistirin:")
    message(STATUS "     cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake")
    message(STATUS "")
    message(STATUS "VEYA MANUEL YUKLEME:")
    message(STATUS "  https://www.boost.org/users/download/ adresinden indirin")
    message(STATUS "")
    message(FATAL_ERROR "Boost gerekli! Lutfen yukleyin.")
endif()

# Find OpenSSL
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
    message(STATUS "")
    message(STATUS "==========================================")
    message(STATUS "OpenSSL bulunamadı!")
    message(STATUS "==========================================")
    message(STATUS "")
    message(STATUS "OTOMATIK YUKLEME ICIN:")
    message(STATUS "  vcpkg install openssl")
    message(STATUS "")
    message(STATUS "VEYA MANUEL YUKLEME:")
    message(STATUS "  https://slproweb.com/products/Win32OpenSSL.html")
    message(STATUS "")
    message(FATAL_ERROR "OpenSSL gerekli! Lutfen yukleyin.")
endif()

message(STATUS "Boost bulundu: ${Boost_VERSION}")
message(STATUS "OpenSSL bulundu: ${OPENSSL_VERSION}")

file(GLOB NET_SOURCES "src/net/*.cpp")
file(GLOB MAIN_SOURCES "main.cpp")
set(SOURCES ${NET_SOURCES} ${MAIN_SOURCES})

add_executable(arb_engine ${SOURCES})

target_include_directories(arb_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(arb_engine PRIVATE
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(MSVC)
    target_compile_options(arb_engine PRIVATE /W4)
    target_compile_definitions(arb_engine PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0601
    )
    
    # Release modunda optimizasyon, Debug modunda runtime checks
    target_compile_options(arb_engine PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/RTC1>
    )
else()
    target_compile_options(arb_engine PRIVATE -O3 -march=native -Wall -Wextra -Wpedantic)
endif()
