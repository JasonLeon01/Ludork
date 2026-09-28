#include "VideoDecoder.hpp"

#if LUDORK_HAS_FFMPEG
#include <algorithm>
#include <cmath>
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
            const std::int64_t start =
                stream_->start_time == AV_NOPTS_VALUE ? 0 : stream_->start_time;
            time_ = frame_->best_effort_timestamp == AV_NOPTS_VALUE
                        ? static_cast<double>(frameIndex_ - 1) / fps_
                        : static_cast<double>(frame_->best_effort_timestamp -
                                              start) *
                              av_q2d(stream_->time_base);
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

double VideoDecoder::duration() const noexcept {
    if (stream_->duration != AV_NOPTS_VALUE && stream_->duration > 0) {
        return static_cast<double>(stream_->duration) *
               av_q2d(stream_->time_base);
    }
    return format_->duration == AV_NOPTS_VALUE
               ? 0.0
               : std::max(0.0, static_cast<double>(format_->duration) /
                                   AV_TIME_BASE);
}

double VideoDecoder::time() const noexcept {
    return time_;
}

bool VideoDecoder::seek(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        throw std::invalid_argument(
            "Video seek time must be finite and nonnegative");
    }
    const double boundedTime =
        duration() > 0 ? std::min(seconds, duration()) : seconds;
    const std::int64_t start =
        stream_->start_time == AV_NOPTS_VALUE ? 0 : stream_->start_time;
    const std::int64_t timestamp =
        start +
        static_cast<std::int64_t>(boundedTime / av_q2d(stream_->time_base));
    requireFfmpeg(av_seek_frame(format_.get(), streamIndex_, timestamp,
                                AVSEEK_FLAG_BACKWARD),
                  "Failed to seek video");
    avcodec_flush_buffers(decoder_.get());
    av_packet_unref(packet_.get());
    av_frame_unref(frame_.get());
    flushing_ = false;
    frameIndex_ = 0;
    time_ = -1;
    while (readFrame()) {
        if (time_ + 0.5 / fps_ >= boundedTime) {
            return true;
        }
    }
    return false;
}

}  // namespace ludork::video
#endif
