cmake_minimum_required(VERSION 3.21)

include(ExternalProject)

get_filename_component(
    LUDORK_FFMPEG_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../ffmpeg"
    ABSOLUTE)
if(NOT EXISTS "${LUDORK_FFMPEG_SOURCE_DIR}/configure")
    message(FATAL_ERROR "FFmpeg source was not found. Run the platform init tool first.")
endif()

function(ludork_ffmpeg_major library macro output)
    file(STRINGS
        "${LUDORK_FFMPEG_SOURCE_DIR}/${library}/version_major.h"
        version_line
        REGEX "^#define[ \t]+${macro}[ \t]+[0-9]+")
    if(NOT version_line)
        file(STRINGS
            "${LUDORK_FFMPEG_SOURCE_DIR}/${library}/version.h"
            version_line
            REGEX "^#define[ \t]+${macro}[ \t]+[0-9]+")
    endif()
    if(NOT version_line)
        message(FATAL_ERROR "Unable to read ${macro} from FFmpeg source.")
    endif()
    string(REGEX REPLACE ".*[ \t]+([0-9]+).*" "\\1" version "${version_line}")
    set(${output} "${version}" PARENT_SCOPE)
endfunction()

ludork_ffmpeg_major(libavutil LIBAVUTIL_VERSION_MAJOR FFMPEG_AVUTIL_MAJOR)
ludork_ffmpeg_major(libavcodec LIBAVCODEC_VERSION_MAJOR FFMPEG_AVCODEC_MAJOR)
ludork_ffmpeg_major(libavformat LIBAVFORMAT_VERSION_MAJOR FFMPEG_AVFORMAT_MAJOR)
ludork_ffmpeg_major(libswscale LIBSWSCALE_VERSION_MAJOR FFMPEG_SWSCALE_MAJOR)
ludork_ffmpeg_major(libswresample LIBSWRESAMPLE_VERSION_MAJOR FFMPEG_SWRESAMPLE_MAJOR)

set(LUDORK_FFMPEG_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/external")
set(LUDORK_FFMPEG_BUILD_DIR "${LUDORK_FFMPEG_PREFIX}/build")
set(LUDORK_FFMPEG_INSTALL_DIR "${LUDORK_FFMPEG_PREFIX}/install")
file(MAKE_DIRECTORY "${LUDORK_FFMPEG_INSTALL_DIR}/include")

function(ludork_use_static_ffmpeg_runtime)
    set(FFMPEG_LIBRARY_TYPE STATIC PARENT_SCOPE)
    foreach(ffmpeg_library IN ITEMS
        AVUTIL AVCODEC AVFORMAT SWSCALE SWRESAMPLE)
        string(TOLOWER "${ffmpeg_library}" ffmpeg_library_name)
        set(FFMPEG_${ffmpeg_library}_RUNTIME
            "${LUDORK_FFMPEG_INSTALL_DIR}/lib/lib${ffmpeg_library_name}.a"
            PARENT_SCOPE)
    endforeach()
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    if(NOT DEFINED OHOS_SDK_NATIVE OR OHOS_SDK_NATIVE STREQUAL "")
        message(FATAL_ERROR "OHOS_SDK_NATIVE is required for the FFmpeg OHOS build.")
    endif()
    if(NOT EXISTS "${OHOS_SDK_NATIVE}/llvm/bin/aarch64-unknown-linux-ohos-clang")
        message(FATAL_ERROR "The OHOS native LLVM toolchain was not found: ${OHOS_SDK_NATIVE}")
    endif()
    find_program(LUDORK_GNU_MAKE_EXECUTABLE NAMES gmake make REQUIRED)
    ludork_use_static_ffmpeg_runtime()
    set(FFMPEG_CONFIGURE_COMMAND
        sh
        "${CMAKE_CURRENT_SOURCE_DIR}/build_ohos.sh"
        "${LUDORK_FFMPEG_SOURCE_DIR}"
        "${LUDORK_FFMPEG_BUILD_DIR}"
        "${LUDORK_FFMPEG_INSTALL_DIR}"
        "${LUDORK_GNU_MAKE_EXECUTABLE}"
        "${OHOS_SDK_NATIVE}")
elseif(ANDROID)
    if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR
            "The FFmpeg Android build supports only arm64-v8a.")
    endif()
    if(NOT ANDROID_PLATFORM_LEVEL STREQUAL "24")
        message(FATAL_ERROR
            "The FFmpeg Android build requires API level 24.")
    endif()
    get_filename_component(
        LUDORK_ANDROID_TOOLCHAIN_BIN
        "${CMAKE_C_COMPILER}"
        DIRECTORY)
    get_filename_component(
        LUDORK_ANDROID_TOOLCHAIN_ROOT
        "${LUDORK_ANDROID_TOOLCHAIN_BIN}"
        DIRECTORY)
    if(NOT EXISTS "${LUDORK_ANDROID_TOOLCHAIN_ROOT}/bin/clang"
       OR NOT EXISTS "${LUDORK_ANDROID_TOOLCHAIN_ROOT}/sysroot")
        message(FATAL_ERROR
            "The Android NDK LLVM toolchain was not found: "
            "${LUDORK_ANDROID_TOOLCHAIN_ROOT}")
    endif()
    if(NOT DEFINED CMAKE_MAKE_PROGRAM
       OR CMAKE_MAKE_PROGRAM STREQUAL ""
       OR NOT EXISTS "${CMAKE_MAKE_PROGRAM}")
        message(FATAL_ERROR
            "CMAKE_MAKE_PROGRAM must name the validated GNU Make executable "
            "for the Android FFmpeg build.")
    endif()
    set(LUDORK_GNU_MAKE_EXECUTABLE "${CMAKE_MAKE_PROGRAM}")
    ludork_use_static_ffmpeg_runtime()
    set(FFMPEG_CONFIGURE_COMMAND
        sh
        "${CMAKE_CURRENT_SOURCE_DIR}/build_android.sh"
        "${LUDORK_FFMPEG_SOURCE_DIR}"
        "${LUDORK_FFMPEG_BUILD_DIR}"
        "${LUDORK_FFMPEG_INSTALL_DIR}"
        "${LUDORK_GNU_MAKE_EXECUTABLE}"
        "${LUDORK_ANDROID_TOOLCHAIN_ROOT}"
        "${ANDROID_PLATFORM_LEVEL}")
elseif(WIN32)
    set(FFMPEG_LIBRARY_TYPE SHARED)
    if(NOT LUDORK_GNU_MAKE_EXECUTABLE)
        find_program(LUDORK_GNU_MAKE_EXECUTABLE NAMES gnumake make)
    endif()
    if(NOT LUDORK_GNU_MAKE_EXECUTABLE)
        message(FATAL_ERROR "GNU Make was not found. Run tools/init.bat first.")
    endif()
    set(FFMPEG_AVUTIL_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/bin/avutil-${FFMPEG_AVUTIL_MAJOR}.dll")
    set(FFMPEG_AVCODEC_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/bin/avcodec-${FFMPEG_AVCODEC_MAJOR}.dll")
    set(FFMPEG_AVFORMAT_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/bin/avformat-${FFMPEG_AVFORMAT_MAJOR}.dll")
    set(FFMPEG_SWSCALE_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/bin/swscale-${FFMPEG_SWSCALE_MAJOR}.dll")
    set(FFMPEG_SWRESAMPLE_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/bin/swresample-${FFMPEG_SWRESAMPLE_MAJOR}.dll")
    set(FFMPEG_CONFIGURE_COMMAND
        cmd.exe /d /c "${CMAKE_CURRENT_SOURCE_DIR}/build_msvc.bat"
        "${LUDORK_FFMPEG_SOURCE_DIR}"
        "${LUDORK_FFMPEG_BUILD_DIR}"
        "${LUDORK_FFMPEG_INSTALL_DIR}"
        "${LUDORK_GNU_MAKE_EXECUTABLE}")
elseif(APPLE)
    find_program(LUDORK_GNU_MAKE_EXECUTABLE NAMES gmake make REQUIRED)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(FFMPEG_APPLE_PLATFORM ios)
        set(FFMPEG_APPLE_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        ludork_use_static_ffmpeg_runtime()
    else()
        set(FFMPEG_APPLE_PLATFORM macos)
        set(FFMPEG_APPLE_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        set(FFMPEG_LIBRARY_TYPE SHARED)
        set(FFMPEG_AVUTIL_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/lib/libavutil.${FFMPEG_AVUTIL_MAJOR}.dylib")
        set(FFMPEG_AVCODEC_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/lib/libavcodec.${FFMPEG_AVCODEC_MAJOR}.dylib")
        set(FFMPEG_AVFORMAT_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/lib/libavformat.${FFMPEG_AVFORMAT_MAJOR}.dylib")
        set(FFMPEG_SWSCALE_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/lib/libswscale.${FFMPEG_SWSCALE_MAJOR}.dylib")
        set(FFMPEG_SWRESAMPLE_RUNTIME "${LUDORK_FFMPEG_INSTALL_DIR}/lib/libswresample.${FFMPEG_SWRESAMPLE_MAJOR}.dylib")
    endif()
    set(FFMPEG_CONFIGURE_COMMAND
        sh
        "${CMAKE_CURRENT_SOURCE_DIR}/build_apple.sh"
        "${LUDORK_FFMPEG_SOURCE_DIR}"
        "${LUDORK_FFMPEG_BUILD_DIR}"
        "${LUDORK_FFMPEG_INSTALL_DIR}"
        "${LUDORK_GNU_MAKE_EXECUTABLE}"
        "${FFMPEG_APPLE_PLATFORM}"
        "${FFMPEG_APPLE_DEPLOYMENT_TARGET}")
else()
    message(FATAL_ERROR
        "The trimmed FFmpeg subproject supports Windows, macOS, iOS, Android, and OHOS.")
endif()

set(LUDORK_FFMPEG_RUNTIME_FILES
    "${FFMPEG_AVUTIL_RUNTIME}"
    "${FFMPEG_AVCODEC_RUNTIME}"
    "${FFMPEG_AVFORMAT_RUNTIME}"
    "${FFMPEG_SWSCALE_RUNTIME}"
    "${FFMPEG_SWRESAMPLE_RUNTIME}")

ExternalProject_Add(LudorkFFmpegExternal
    SOURCE_DIR "${LUDORK_FFMPEG_SOURCE_DIR}"
    BINARY_DIR "${LUDORK_FFMPEG_PREFIX}"
    CONFIGURE_COMMAND ${FFMPEG_CONFIGURE_COMMAND}
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_BYPRODUCTS ${LUDORK_FFMPEG_RUNTIME_FILES})

function(ludork_ffmpeg_import target library runtime)
    add_library(${target} ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
    set_target_properties(${target} PROPERTIES
        IMPORTED_LOCATION "${runtime}"
        INTERFACE_INCLUDE_DIRECTORIES "${LUDORK_FFMPEG_INSTALL_DIR}/include")
    if(WIN32)
        set_target_properties(${target} PROPERTIES
            IMPORTED_IMPLIB "${LUDORK_FFMPEG_INSTALL_DIR}/bin/${library}.lib")
    endif()
    add_dependencies(${target} LudorkFFmpegExternal)
endfunction()

ludork_ffmpeg_import(FFmpeg::avutil avutil "${FFMPEG_AVUTIL_RUNTIME}")
ludork_ffmpeg_import(FFmpeg::avcodec avcodec "${FFMPEG_AVCODEC_RUNTIME}")
ludork_ffmpeg_import(FFmpeg::avformat avformat "${FFMPEG_AVFORMAT_RUNTIME}")
ludork_ffmpeg_import(FFmpeg::swscale swscale "${FFMPEG_SWSCALE_RUNTIME}")
ludork_ffmpeg_import(FFmpeg::swresample swresample "${FFMPEG_SWRESAMPLE_RUNTIME}")

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS" OR ANDROID)
    target_link_libraries(FFmpeg::avutil INTERFACE Threads::Threads m)
    if(ANDROID)
        target_link_libraries(FFmpeg::avutil INTERFACE atomic)
    endif()
    target_link_libraries(FFmpeg::avcodec INTERFACE FFmpeg::avutil)
    target_link_libraries(FFmpeg::avformat INTERFACE FFmpeg::avcodec FFmpeg::avutil)
    target_link_libraries(FFmpeg::swscale INTERFACE FFmpeg::avutil)
    target_link_libraries(FFmpeg::swresample INTERFACE FFmpeg::avutil)
endif()

set(LUDORK_FFMPEG_RUNTIME_FILES
    "${LUDORK_FFMPEG_RUNTIME_FILES}"
    PARENT_SCOPE)

function(ludork_copy_ffmpeg_runtime target)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS"
       OR CMAKE_SYSTEM_NAME STREQUAL "OHOS"
       OR ANDROID)
        return()
    endif()
    foreach(runtime_file IN LISTS LUDORK_FFMPEG_RUNTIME_FILES)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_file}"
                "$<TARGET_FILE_DIR:${target}>")
    endforeach()
endfunction()
