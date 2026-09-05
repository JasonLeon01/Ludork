#include "VideoDecoder.hpp"

#if LUDORK_HAS_FFMPEG
#include <stdexcept>

namespace ludork::video {

VideoDecoder::VideoDecoder(const std::string& path)
    : format_(openFormat(path)),
      packet_(av_packet_alloc()),
      frame_(av_frame_alloc()) {
    streamIndex_ = av_find_best_stream(format_.get(), AVMEDIA_TYPE_VIDEO, -1,
                                       -1, nullptr, 0);
    requireFfmpeg(streamIndex_, "Failed to find video stream");
    stream_ = format_->streams[streamIndex_];
    decoder_ = openCodec(stream_, AV_CODEC_ID_H264, "video");
    if (packet_ == nullptr || frame_ == nullptr) {
        throw std::runtime_error("Failed to allocate video decode buffers");
    }
    fps_ = av_q2d(stream_->avg_frame_rate);
    if (fps_ <= 0.0) {
        fps_ = av_q2d(av_guess_frame_rate(format_.get(), stream_, nullptr));
    }
    if (fps_ <= 0.0) {
        throw std::runtime_error("Video frame rate is unavailable");
    }
}

bool VideoDecoder::readFrame() {
    while (true) {
        const int receiveResult =
            avcodec_receive_frame(decoder_.get(), frame_.get());
        if (receiveResult == 0) {
            ++frameIndex_;
            return true;
        }
        if (receiveResult == AVERROR_EOF) {
            return false;
        }
        if (receiveResult != AVERROR(EAGAIN)) {
            requireFfmpeg(receiveResult, "Failed to decode video frame");
        }
        if (flushing_) {
            return false;
        }

        bool packetSubmitted = false;
        int readResult = 0;
        while ((readResult = av_read_frame(format_.get(), packet_.get())) >=
               0) {
            if (packet_->stream_index == streamIndex_) {
                requireFfmpeg(
                    avcodec_send_packet(decoder_.get(), packet_.get()),
                    "Failed to submit video packet");
                packetSubmitted = true;
            }
            av_packet_unref(packet_.get());
            if (packetSubmitted) {
                break;
            }
        }
        if (!packetSubmitted) {
            if (readResult != AVERROR_EOF) {
                requireFfmpeg(readResult, "Failed to read video packet");
            }
            requireFfmpeg(avcodec_send_packet(decoder_.get(), nullptr),
                          "Failed to flush video decoder");
            flushing_ = true;
        }
    }
}

const std::vector<std::uint8_t>& VideoDecoder::rgbaFrame() {
    SwsContext* context =
        sws_getCachedContext(scaler_.release(), frame_->width, frame_->height,
                             static_cast<AVPixelFormat>(frame_->format),
                             frame_->width, frame_->height, AV_PIX_FMT_RGBA,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (context == nullptr) {
        throw std::runtime_error("Failed to initialize video frame converter");
    }
    scaler_.reset(context);
    rgba_.resize(static_cast<std::size_t>(frame_->width) * frame_->height * 4U);
    uint8_t* output[] = {rgba_.data()};
    const int outputStrides[] = {frame_->width * 4};
    const int rows = sws_scale(scaler_.get(), frame_->data, frame_->linesize, 0,
                               frame_->height, output, outputStrides);
    if (rows != frame_->height) {
        throw std::runtime_error("Failed to convert complete video frame");
    }
    return rgba_;
}

double VideoDecoder::fps() const noexcept {
    return fps_;
}

int VideoDecoder::frameIndex() const noexcept {
    return frameIndex_;
}

int VideoDecoder::width() const noexcept {
    return frame_->width;
}

int VideoDecoder::height() const noexcept {
    return frame_->height;
}

}  // namespace ludork::video
#endif
