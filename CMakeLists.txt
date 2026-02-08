cmake_minimum_required(VERSION 3.20)

project(video-benchmark
    VERSION 1.0.0
    DESCRIPTION "Video decoding benchmark tool"
    LANGUAGES CXX
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add cmake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find required packages
find_package(FFmpeg REQUIRED)
find_package(Threads REQUIRED)
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/video/video_info.cpp
    src/decoder/video_decoder.cpp
    src/decoder/decoder_thread.cpp
    src/benchmark/benchmark_runner.cpp
    src/monitor/system_info.cpp
    src/utils/cli_parser.cpp
    src/utils/output_formatter.cpp
    src/utils/logger.cpp
)

# Platform-specific CPU monitor implementation
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SOURCES src/monitor/cpu_monitor_linux.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND SOURCES src/monitor/cpu_monitor_macos.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND SOURCES src/monitor/cpu_monitor_windows.cpp)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Create executable
add_executable(video-benchmark ${SOURCES})

# Include directories
target_include_directories(video-benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(video-benchmark PRIVATE
    ${FFMPEG_LIBRARIES}
    Threads::Threads
)

if(TARGET spdlog::spdlog_header_only)
    target_link_libraries(video-benchmark PRIVATE spdlog::spdlog_header_only)
else()
    target_link_libraries(video-benchmark PRIVATE spdlog::spdlog)
endif()

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS: Link with Mach API for CPU monitoring
    target_link_libraries(video-benchmark PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(video-benchmark PRIVATE /W4)
else()
    target_compile_options(video-benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS video-benchmark
    RUNTIME DESTINATION bin
)
