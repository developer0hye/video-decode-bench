# FindFFmpeg.cmake
# Finds FFmpeg libraries (libavcodec, libavformat, libavutil)
#
# This module defines:
#   FFMPEG_FOUND        - True if FFmpeg is found
#   FFMPEG_INCLUDE_DIRS - FFmpeg include directories
#   FFMPEG_LIBRARIES    - FFmpeg libraries to link against

include(FindPackageHandleStandardArgs)

# Use pkg-config to find FFmpeg components
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(AVCODEC QUIET libavcodec)
    pkg_check_modules(AVFORMAT QUIET libavformat)
    pkg_check_modules(AVUTIL QUIET libavutil)
endif()

# Find include directories
find_path(AVCODEC_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    HINTS ${AVCODEC_INCLUDE_DIRS}
    PATH_SUFFIXES ffmpeg
)

find_path(AVFORMAT_INCLUDE_DIR
    NAMES libavformat/avformat.h
    HINTS ${AVFORMAT_INCLUDE_DIRS}
    PATH_SUFFIXES ffmpeg
)

find_path(AVUTIL_INCLUDE_DIR
    NAMES libavutil/avutil.h
    HINTS ${AVUTIL_INCLUDE_DIRS}
    PATH_SUFFIXES ffmpeg
)

# Find libraries
find_library(AVCODEC_LIBRARY
    NAMES avcodec
    HINTS ${AVCODEC_LIBRARY_DIRS}
)

find_library(AVFORMAT_LIBRARY
    NAMES avformat
    HINTS ${AVFORMAT_LIBRARY_DIRS}
)

find_library(AVUTIL_LIBRARY
    NAMES avutil
    HINTS ${AVUTIL_LIBRARY_DIRS}
)

# Set output variables
set(FFMPEG_INCLUDE_DIRS
    ${AVCODEC_INCLUDE_DIR}
    ${AVFORMAT_INCLUDE_DIR}
    ${AVUTIL_INCLUDE_DIR}
)
list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)

set(FFMPEG_LIBRARIES
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
)

# Handle standard arguments
find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS
        AVCODEC_LIBRARY
        AVFORMAT_LIBRARY
        AVUTIL_LIBRARY
        AVCODEC_INCLUDE_DIR
        AVFORMAT_INCLUDE_DIR
        AVUTIL_INCLUDE_DIR
)

# Create imported targets
if(FFMPEG_FOUND AND NOT TARGET FFmpeg::avcodec)
    add_library(FFmpeg::avcodec UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::avcodec PROPERTIES
        IMPORTED_LOCATION "${AVCODEC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AVCODEC_INCLUDE_DIR}"
    )

    add_library(FFmpeg::avformat UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::avformat PROPERTIES
        IMPORTED_LOCATION "${AVFORMAT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AVFORMAT_INCLUDE_DIR}"
    )

    add_library(FFmpeg::avutil UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::avutil PROPERTIES
        IMPORTED_LOCATION "${AVUTIL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AVUTIL_INCLUDE_DIR}"
    )

    # Combined target for convenience
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    set_target_properties(FFmpeg::FFmpeg PROPERTIES
        INTERFACE_LINK_LIBRARIES "FFmpeg::avcodec;FFmpeg::avformat;FFmpeg::avutil"
    )
endif()

mark_as_advanced(
    AVCODEC_INCLUDE_DIR
    AVFORMAT_INCLUDE_DIR
    AVUTIL_INCLUDE_DIR
    AVCODEC_LIBRARY
    AVFORMAT_LIBRARY
    AVUTIL_LIBRARY
)
