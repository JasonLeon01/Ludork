#pragma once

#include <cstdint>
#include <vector>

struct RgbaImageView {
    const std::uint8_t *data = nullptr;
    int w = 0;
    int h = 0;
    int stride = 0;
};

void blitRectCopy(const RgbaImageView &src, int srcX, int srcY, int copyW, int copyH,
                  std::uint8_t *dst, int dstW, int dstH, int dstStride, int dstX, int dstY);

void blitRectScaled(const RgbaImageView &src, int srcX, int srcY, int srcW, int srcH,
                    std::uint8_t *dst, int dstW, int dstH, int dstStride, int dstX, int dstY,
                    int outW, int outH);

void scaleRgbaImage(const std::uint8_t *src, int srcW, int srcH, int srcStride,
                    std::uint8_t *dst, int dstW, int dstH, int dstStride);

void compositeRgbaLayer(std::uint8_t *base, int baseW, int baseH, int baseStride,
                        const std::uint8_t *overlay, int overlayW, int overlayH, int overlayStride);
