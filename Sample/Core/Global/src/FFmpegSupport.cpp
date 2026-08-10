#include "FFmpegSupport.hpp"

#if LUDORK_HAS_FFMPEG
#include <stdexcept>

namespace ludork::video {

std::string ffmpegError(int code) {
    char message[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, message, sizeof(message));
    return message;
}

void requireFfmpeg(int result, const std::string& operation) {
    if (result < 0) {
        throw std::runtime_error(operation + ": " + ffmpegError(result));
    }
}

void FormatContextDeleter::operator()(AVFormatContext* context) const noexcept {
    if (context != nullptr) {
        avformat_close_input(&context);
    }
}

void CodecContextDeleter::operator()(AVCodecContext* context) const noexcept {
    if (context != nullptr) {
        avcodec_free_context(&context);
    }
}

void FrameDeleter::operator()(AVFrame* frame) const noexcept {
    if (frame != nullptr) {
        av_frame_free(&frame);
    }
}

void PacketDeleter::operator()(AVPacket* packet) const noexcept {
    if (packet != nullptr) {
        av_packet_free(&packet);
    }
}

void SwrContextDeleter::operator()(SwrContext* context) const noexcept {
    if (context != nullptr) {
        swr_free(&context);
    }
}

void SwsContextDeleter::operator()(SwsContext* context) const noexcept {
    if (context != nullptr) {
        sws_freeContext(context);
    }
}

FormatContextPtr openFormat(const std::string& path) {
    AVFormatContext* rawContext = nullptr;
    const int openResult =
        avformat_open_input(&rawContext, path.c_str(), nullptr, nullptr);
    FormatContextPtr context(rawContext);
    requireFfmpeg(openResult, "Failed to open video file");
    requireFfmpeg(avformat_find_stream_info(context.get(), nullptr),
                  "Failed to read video stream information");
    return context;
}

CodecContextPtr openCodec(AVStream* stream, AVCodecID requiredCodec,
                          const std::string& streamName) {
    if (stream->codecpar->codec_id != requiredCodec) {
        throw std::runtime_error(
            "Unsupported " + streamName + " codec: " +
            std::string(avcodec_get_name(stream->codecpar->codec_id)));
    }
    const AVCodec* codec = avcodec_find_decoder(requiredCodec);
    if (codec == nullptr) {
        throw std::runtime_error(streamName + " decoder is unavailable");
    }
    CodecContextPtr context(avcodec_alloc_context3(codec));
    if (context == nullptr) {
        throw std::runtime_error("Failed to allocate " + streamName +
                                 " decoder");
    }
    requireFfmpeg(
        avcodec_parameters_to_context(context.get(), stream->codecpar),
        "Failed to prepare " + streamName + " decoder");
    requireFfmpeg(avcodec_open2(context.get(), codec, nullptr),
                  "Failed to open " + streamName + " decoder");
    return context;
}

}  // namespace ludork::video
#endif
