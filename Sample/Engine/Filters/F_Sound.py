# -*- encoding: utf-8 -*-

from __future__ import annotations
import math
from dataclasses import dataclass
from typing import List, Union, TypeAlias, Callable, Optional, Tuple, TYPE_CHECKING

if TYPE_CHECKING:
    from Engine import Time, Vector3f, Angle, Sound, Music

EffectProcessor: TypeAlias = Callable[[List[float], int, List[float], int, int], None]


@dataclass
class SoundFilter:
    loop: Optional[bool] = None
    offset: Optional[Union[float, Time]] = None
    needEffect: bool = False
    soundEffect: Optional[EffectProcessor] = None
    pitch: Optional[float] = None
    pan: Optional[float] = None
    volume: Optional[float] = None
    spatial: Optional[bool] = None
    position: Optional[Union[Tuple[float, float, float], Vector3f]] = None
    direction: Optional[Union[Tuple[float, float, float], Vector3f]] = None
    cone: Optional[Union[Tuple[Angle, Angle, float], Tuple[float, float, float], Sound.Cone]] = None
    velocity: Optional[Union[Tuple[float, float, float], Vector3f]] = None
    dopplerFactor: Optional[float] = None
    directionalAttenuationFactor: Optional[float] = None
    relativeToListener: Optional[bool] = None
    minDistance: Optional[float] = None
    maxDistance: Optional[float] = None
    minGain: Optional[float] = None
    maxGain: Optional[float] = None
    attenuation: Optional[float] = None


@dataclass
class MusicFilter(SoundFilter):
    loopPoint: Optional[Union[Tuple[float, float], Tuple[Time, Time], Music.TimeSpan]] = None


def echoEffect(delay: float, decay: float, sampleRate: float):
    delay_samples = int(delay * sampleRate / 1000)
    delay_buffer = [0.0] * (delay_samples * 2)
    buffer_index = 0

    def processor(
        inputFrames: List[float],
        inputCount: int,
        outputFrames: List[float],
        outputCount: int,
        channelCount: int,
    ) -> None:
        nonlocal buffer_index

        required_size = delay_samples * channelCount
        while len(delay_buffer) < required_size:
            delay_buffer.extend([0.0] * channelCount)

        for i in range(inputCount):
            for ch in range(channelCount):
                idx = i * channelCount + ch
                buf_idx = (buffer_index * channelCount + ch) % len(delay_buffer)

                if idx < len(inputFrames):
                    delayed_signal = delay_buffer[buf_idx]
                    output_signal = inputFrames[idx] + delayed_signal * decay

                    delay_buffer[buf_idx] = inputFrames[idx]
                    outputFrames[idx] = output_signal
                else:
                    outputFrames[idx] = 0.0

            buffer_index = (buffer_index + 1) % delay_samples

    return processor


def distortionEffect(drive: float, threshold: float = 0.7) -> EffectProcessor:
    def processor(
        inputFrames: List[float],
        inputCount: int,
        outputFrames: List[float],
        outputCount: int,
        channelCount: int,
    ) -> None:
        total_samples = inputCount * channelCount

        for i in range(total_samples):
            if i < len(inputFrames):
                amplified = inputFrames[i] * drive

                if abs(amplified) > threshold:
                    outputFrames[i] = threshold if amplified > 0 else -threshold
                else:
                    outputFrames[i] = amplified
            else:
                outputFrames[i] = 0.0

    return processor


def underwaterEffect(depth: float = 0.7, bubble_intensity: float = 0.3, sample_rate: float = 44100) -> EffectProcessor:
    cutoff_freq = 800 - depth * 600
    rc = 1.0 / (cutoff_freq * 2 * math.pi)
    dt = 1.0 / sample_rate
    alpha = dt / (rc + dt)

    reverb_delays = [int(sample_rate * ms / 1000) for ms in [43, 67, 89, 127, 173]]
    reverb_buffers = []
    reverb_indices = []
    for delay in reverb_delays:
        reverb_buffers.append([0.0] * (delay * 2))
        reverb_indices.append(0)

    import random

    bubble_phase = 0.0
    bubble_rate = 0.001 + bubble_intensity * 0.005

    water_phase = 0.0
    water_freq = 0.5

    prev_outputs = [0.0, 0.0]

    envelope = 0.0
    compress_ratio = 1.0 + depth * 2.0

    def processor(
        inputFrames: List[float],
        inputCount: int,
        outputFrames: List[float],
        outputCount: int,
        channelCount: int,
    ) -> None:
        nonlocal bubble_phase, water_phase, prev_outputs, envelope, reverb_indices

        while len(prev_outputs) < channelCount:
            prev_outputs.append(0.0)

        for i, delay in enumerate(reverb_delays):
            required_size = delay * channelCount
            while len(reverb_buffers[i]) < required_size:
                reverb_buffers[i].extend([0.0] * channelCount)

        for i in range(inputCount):
            water_modulation = 1.0 + 0.05 * depth * math.sin(water_phase)
            water_phase += 2 * math.pi * water_freq / sample_rate
            if water_phase >= 2 * math.pi:
                water_phase -= 2 * math.pi

            bubble_noise = 0.0
            if random.random() < bubble_rate:
                bubble_freq = 200 + random.random() * 800
                bubble_amp = (random.random() * 0.5 + 0.5) * bubble_intensity * 0.1
                bubble_noise = bubble_amp * math.sin(bubble_phase * bubble_freq) * math.exp(-bubble_phase * 10)

            bubble_phase += dt
            if bubble_phase > 1.0:
                bubble_phase = 0.0

            for ch in range(channelCount):
                idx = i * channelCount + ch

                if idx < len(inputFrames):
                    signal = inputFrames[idx]

                    signal *= water_modulation

                    signal_abs = abs(signal)
                    if signal_abs > envelope:
                        envelope = signal_abs + (envelope - signal_abs) * 0.95
                    else:
                        envelope = signal_abs + (envelope - signal_abs) * 0.999

                    if envelope > 0.3:
                        gain = 0.3 + (envelope - 0.3) / compress_ratio
                        signal *= gain / envelope if envelope > 0 else 1.0

                    filtered = alpha * signal + (1 - alpha) * prev_outputs[ch]
                    prev_outputs[ch] = filtered

                    reverb_sum = 0.0
                    for j, (buffer, delay) in enumerate(zip(reverb_buffers, reverb_delays)):
                        buf_idx = (reverb_indices[j] * channelCount + ch) % len(buffer)
                        reverb_sum += buffer[buf_idx] * (0.2 + depth * 0.3)

                        decay_factor = 0.7 - depth * 0.2
                        buffer[buf_idx] = filtered + buffer[buf_idx] * decay_factor

                    if ch == 0:
                        current_bubble = bubble_noise
                    else:
                        current_bubble = bubble_noise * (0.7 + random.random() * 0.6)

                    final_signal = filtered + reverb_sum * 0.4 + current_bubble

                    volume_reduction = 1.0 - depth * 0.4
                    final_signal *= volume_reduction

                    if abs(final_signal) > 0.8:
                        final_signal *= 0.8 / abs(final_signal)

                    outputFrames[idx] = final_signal
                else:
                    outputFrames[idx] = 0.0

            for j in range(len(reverb_indices)):
                reverb_indices[j] = (reverb_indices[j] + 1) % reverb_delays[j]

    return processor
