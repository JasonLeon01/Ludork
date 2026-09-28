#include "VideoAudio.hpp"

#if LUDORK_HAS_FFMPEG
#include "FFmpegSupport.hpp"

#include <stdexcept>

namespace ludork::video {
namespace {

void appendAudioFrame(SwrContext* resampler, AVCodecContext* decoder,
                      AVFrame* frame, AudioData& audio) {
    const int outputCapacity = static_cast<int>(av_rescale_rnd(
        swr_get_delay(resampler, decoder->sample_rate) + frame->nb_samples,
        decoder->sample_rate, decoder->sample_rate, AV_ROUND_UP));
    const std::size_t previousSize = audio.samples.size();
    audio.samples.resize(previousSize +
                         static_cast<std::size_t>(outputCapacity) *
                             audio.channelCount);
    uint8_t* output[] = {
        reinterpret_cast<uint8_t*>(audio.samples.data() + previousSize)};
    const uint8_t* const* input = frame->extended_data;
    const int converted = swr_convert(resampler, output, outputCapacity, input,
                                      frame->nb_samples);
    requireFfmpeg(converted, "Failed to convert video audio");
    audio.samples.resize(previousSize + static_cast<std::size_t>(converted) *
                                            audio.channelCount);
}

void drainAudioDecoder(AVCodecContext* decoder, SwrContext* resampler,
                       AVFrame* frame, AudioData& audio, bool flushing) {
    while (true) {
        const int result = avcodec_receive_frame(decoder, frame);
        if (result == AVERROR(EAGAIN)) {
            if (flushing) {
                throw std::runtime_error(
                    "Audio decoder requested input while flushing");
            }
            return;
        }
        if (result == AVERROR_EOF) {
            return;
        }
        requireFfmpeg(result, "Failed to decode video audio");
        appendAudioFrame(resampler, decoder, frame, audio);
        av_frame_unref(frame);
    }
}

}  // namespace

AudioData extractAudio(const std::string& path) {
    FormatInput format = openFormat(path);
    const int streamIndex = av_find_best_stream(
        format.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex == AVERROR_STREAM_NOT_FOUND) {
        return {};
    }
    requireFfmpeg(streamIndex, "Failed to find video audio stream");
    AVStream* stream = format->streams[streamIndex];
    CodecContextPtr decoder = openCodec(stream, AV_CODEC_ID_AAC, "audio");

    AudioData audio;
    audio.channelCount = decoder->ch_layout.nb_channels == 1 ? 1U : 2U;
    audio.sampleRate = static_cast<unsigned int>(decoder->sample_rate);
    audio.channelMap = audio.channelCount == 1
                           ? std::vector{sf::SoundChannel::Mono}
                           : std::vector{sf::SoundChannel::FrontLeft,
                                         sf::SoundChannel::FrontRight};

    AVChannelLayout outputLayout;
    av_channel_layout_default(&outputLayout,
                              static_cast<int>(audio.channelCount));
    SwrContext* rawResampler = nullptr;
    const int allocationResult = swr_alloc_set_opts2(
        &rawResampler, &outputLayout, AV_SAMPLE_FMT_S16, decoder->sample_rate,
        &decoder->ch_layout, decoder->sample_fmt, decoder->sample_rate, 0,
        nullptr);
    av_channel_layout_uninit(&outputLayout);
    SwrContextPtr resampler(rawResampler);
    requireFfmpeg(allocationResult, "Failed to allocate video audio converter");
    requireFfmpeg(swr_init(resampler.get()),
                  "Failed to initialize video audio converter");

    PacketPtr packet(av_packet_alloc());
    FramePtr frame(av_frame_alloc());
    if (packet == nullptr || frame == nullptr) {
        throw std::runtime_error(
            "Failed to allocate video audio decode buffers");
    }
    int readResult = 0;
    while ((readResult = av_read_frame(format.get(), packet.get())) >= 0) {
        if (packet->stream_index == streamIndex) {
            requireFfmpeg(avcodec_send_packet(decoder.get(), packet.get()),
                          "Failed to submit video audio packet");
            drainAudioDecoder(decoder.get(), resampler.get(), frame.get(),
                              audio, false);
        }
        av_packet_unref(packet.get());
    }
    if (readResult != AVERROR_EOF) {
        requireFfmpeg(readResult, "Failed to read video audio packet");
    }
    requireFfmpeg(avcodec_send_packet(decoder.get(), nullptr),
                  "Failed to flush video audio decoder");
    drainAudioDecoder(decoder.get(), resampler.get(), frame.get(), audio, true);

    while (true) {
        const int outputCapacity = swr_get_out_samples(resampler.get(), 0);
        if (outputCapacity <= 0) {
            break;
        }
        const std::size_t previousSize = audio.samples.size();
        audio.samples.resize(previousSize +
                             static_cast<std::size_t>(outputCapacity) *
                                 audio.channelCount);
        uint8_t* output[] = {
            reinterpret_cast<uint8_t*>(audio.samples.data() + previousSize)};
        const int converted =
            swr_convert(resampler.get(), output, outputCapacity, nullptr, 0);
        requireFfmpeg(converted, "Failed to flush video audio converter");
        audio.samples.resize(previousSize +
                             static_cast<std::size_t>(converted) *
                                 audio.channelCount);
        if (converted == 0) {
            break;
        }
    }
    return audio;
}

}  // namespace ludork::video
#endif
