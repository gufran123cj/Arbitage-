cmake_minimum_required(VERSION 3.12)
project(arb_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB CORE_SOURCES "src/core/*.cpp")
file(GLOB NET_SOURCES "src/net/*.cpp")
file(GLOB UTIL_SOURCES "src/util/*.cpp")
file(GLOB MAIN_SOURCES "main.cpp")
set(SOURCES ${CORE_SOURCES} ${NET_SOURCES} ${UTIL_SOURCES} ${MAIN_SOURCES})

add_executable(arb_engine ${SOURCES})

target_include_directories(arb_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(arb_engine PRIVATE /W4)
    target_compile_definitions(arb_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
    
    # Release modunda optimizasyon, Debug modunda runtime checks
    target_compile_options(arb_engine PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/RTC1>
    )
else()
    target_compile_options(arb_engine PRIVATE -O3 -march=native -Wall -Wextra -Wpedantic)
endif()

