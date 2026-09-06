#pragma once
#include <Runtime/AssetInputStream.hpp>

#ifndef LUDORK_HAS_FFMPEG
#define LUDORK_HAS_FFMPEG 0
#endif

#if LUDORK_HAS_FFMPEG
#include <Runtime/AssetStore.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <memory>
#include <string>

namespace ludork::video {

struct CodecContextDeleter {
    void operator()(AVCodecContext* context) const noexcept;
};

struct FrameDeleter {
    void operator()(AVFrame* frame) const noexcept;
};

struct PacketDeleter {
    void operator()(AVPacket* packet) const noexcept;
};

struct SwrContextDeleter {
    void operator()(SwrContext* context) const noexcept;
};

struct SwsContextDeleter {
    void operator()(SwsContext* context) const noexcept;
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

struct FormatInput {
    struct AvioContextDeleter {
        void operator()(AVIOContext* context) const noexcept;
    };

    struct FormatContextDeleter {
        void operator()(AVFormatContext* context) const noexcept;
    };

    std::unique_ptr<ludork::runtime::AssetInputStream> stream;
    std::unique_ptr<AVIOContext, FormatInput::AvioContextDeleter> io;
    std::unique_ptr<AVFormatContext, FormatInput::FormatContextDeleter> context;

    [[nodiscard]] AVFormatContext* get() const noexcept {
        return context.get();
    }

    [[nodiscard]] AVFormatContext* operator->() const noexcept {
        return context.get();
    }
};

using FormatContextPtr =
    std::unique_ptr<AVFormatContext, FormatInput::FormatContextDeleter>;
using AvioContextPtr =
    std::unique_ptr<AVIOContext, FormatInput::AvioContextDeleter>;

std::string ffmpegError(int code);
void requireFfmpeg(int result, const std::string& operation);
FormatInput openFormat(const std::string& path);
CodecContextPtr openCodec(AVStream* stream, AVCodecID requiredCodec,
                          const std::string& streamName);

}  // namespace ludork::video
#endif
