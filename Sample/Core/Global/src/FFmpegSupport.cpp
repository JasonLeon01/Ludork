#include "FFmpegSupport.hpp"

#if LUDORK_HAS_FFMPEG
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <limits>
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

void AvioContextDeleter::operator()(AVIOContext* context) const noexcept {
    if (context != nullptr) {
        av_freep(&context->buffer);
        avio_context_free(&context);
    }
}

namespace {

int readAssetPacket(void* opaque, std::uint8_t* buffer, int size) {
    if (opaque == nullptr || buffer == nullptr || size < 0) {
        return AVERROR(EINVAL);
    }
    auto& stream = *static_cast<ludork::runtime::AssetInputStream*>(opaque);
    const std::optional<std::size_t> count =
        stream.read(buffer, static_cast<std::size_t>(size));
    if (!count.has_value()) {
        return AVERROR(EIO);
    }
    if (*count == 0) {
        return AVERROR_EOF;
    }
    return static_cast<int>(*count);
}

std::int64_t seekAsset(void* opaque, std::int64_t offset, int whence) {
    if (opaque == nullptr) {
        return AVERROR(EINVAL);
    }
    auto& stream = *static_cast<ludork::runtime::AssetInputStream*>(opaque);
    const std::optional<std::size_t> size = stream.getSize();
    if (!size.has_value()) {
        return AVERROR(EIO);
    }
    if (whence & AVSEEK_SIZE) {
        if (*size > static_cast<std::size_t>(
                        std::numeric_limits<std::int64_t>::max())) {
            return AVERROR(EOVERFLOW);
        }
        return static_cast<std::int64_t>(*size);
    }
    whence &= ~AVSEEK_FORCE;
    std::int64_t base = 0;
    if (whence == SEEK_CUR) {
        const std::optional<std::size_t> position = stream.tell();
        if (!position.has_value() ||
            *position > static_cast<std::size_t>(
                            std::numeric_limits<std::int64_t>::max())) {
            return AVERROR(EIO);
        }
        base = static_cast<std::int64_t>(*position);
    } else if (whence == SEEK_END) {
        if (*size > static_cast<std::size_t>(
                        std::numeric_limits<std::int64_t>::max())) {
            return AVERROR(EOVERFLOW);
        }
        base = static_cast<std::int64_t>(*size);
    } else if (whence != SEEK_SET) {
        return AVERROR(EINVAL);
    }
    if ((offset > 0 &&
         base > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 &&
         base < std::numeric_limits<std::int64_t>::min() - offset)) {
        return AVERROR(EOVERFLOW);
    }
    const std::int64_t target = base + offset;
    if (target < 0 || static_cast<std::uint64_t>(target) >
                          static_cast<std::uint64_t>(*size)) {
        return AVERROR(EINVAL);
    }
    const std::optional<std::size_t> position =
        stream.seek(static_cast<std::size_t>(target));
    return position.has_value() ? target : AVERROR(EIO);
}

}  // namespace

FormatInput openFormat(const std::string& path) {
    FormatInput input;
    input.stream = ludork::runtime::assetStore().open(path);
    constexpr int BufferSize = 32 * 1024;
    unsigned char* buffer = static_cast<unsigned char*>(av_malloc(BufferSize));
    if (buffer == nullptr) {
        throw std::runtime_error("Failed to allocate video input buffer");
    }
    input.io.reset(avio_alloc_context(buffer, BufferSize, 0, input.stream.get(),
                                      readAssetPacket, nullptr, seekAsset));
    if (input.io == nullptr) {
        av_free(buffer);
        throw std::runtime_error("Failed to create video input stream");
    }
    AVFormatContext* rawContext = avformat_alloc_context();
    if (rawContext == nullptr) {
        throw std::runtime_error("Failed to allocate video format context");
    }
    rawContext->pb = input.io.get();
    rawContext->flags |= AVFMT_FLAG_CUSTOM_IO;
    const int openResult =
        avformat_open_input(&rawContext, nullptr, nullptr, nullptr);
    input.context.reset(rawContext);
    requireFfmpeg(openResult, "Failed to open video asset " + path);
    requireFfmpeg(avformat_find_stream_info(input.context.get(), nullptr),
                  "Failed to read video stream information");
    return input;
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
