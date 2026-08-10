#pragma once

#include "FFmpegSupport.hpp"

#if LUDORK_HAS_FFMPEG
#include <cstdint>
#include <string>
#include <vector>

namespace ludork::video {

class VideoDecoder {
public:
    explicit VideoDecoder(const std::string& path);

    bool readFrame();
    const std::vector<std::uint8_t>& rgbaFrame();
    double fps() const noexcept;
    int frameIndex() const noexcept;
    int width() const noexcept;
    int height() const noexcept;

private:
    FormatContextPtr format_;
    CodecContextPtr decoder_;
    PacketPtr packet_;
    FramePtr frame_;
    SwsContextPtr scaler_;
    AVStream* stream_ = nullptr;
    int streamIndex_ = -1;
    int frameIndex_ = 0;
    double fps_ = 0.0;
    bool flushing_ = false;
    std::vector<std::uint8_t> rgba_;
};

}  // namespace ludork::video
#endif
