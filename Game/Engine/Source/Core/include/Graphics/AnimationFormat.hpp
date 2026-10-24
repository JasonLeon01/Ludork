#pragma once

namespace ludork::engine::animation_format {
inline constexpr int DefaultFrameRate = 30;
inline constexpr char FrameSegment[] = "frame";
inline constexpr char Base64ZlibEncoding[] = "base64+zlib";
inline constexpr float SoundDurationTolerance = 1e-5f;
}  // namespace ludork::engine::animation_format
