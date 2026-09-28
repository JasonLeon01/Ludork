add_library(LudorkVideo STATIC
    src/FFmpegSupport.cpp
    src/VideoAudio.cpp
    src/VideoDecoder.cpp)
add_library(Ludork::Video ALIAS LudorkVideo)
set_target_properties(LudorkVideo PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(LudorkVideo PUBLIC cxx_std_20)
target_include_directories(LudorkVideo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(LudorkVideo PUBLIC LUDORK_HAS_FFMPEG=$<BOOL:${LUDORK_ENABLE_FFMPEG}>)
target_link_libraries(LudorkVideo PUBLIC Ludork::Runtime SFML::Graphics SFML::Audio)
if(LUDORK_ENABLE_FFMPEG)
    target_link_libraries(LudorkVideo PUBLIC FFmpeg::avformat FFmpeg::avcodec
        FFmpeg::avutil FFmpeg::swscale FFmpeg::swresample)
endif()
