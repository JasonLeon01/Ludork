#include <MapImageBuffer.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

void sampleBilinearRGBA(const std::uint8_t *src, int srcW, int srcH, int srcStride, float fx,
                        float fy, std::uint8_t out[4]) {
    fx = std::max(0.0f, std::min(fx, static_cast<float>(srcW - 1)));
    fy = std::max(0.0f, std::min(fy, static_cast<float>(srcH - 1)));
    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, srcW - 1);
    int y1 = std::min(y0 + 1, srcH - 1);
    float tx = fx - static_cast<float>(x0);
    float ty = fy - static_cast<float>(y0);

    for (int c = 0; c < 4; ++c) {
        float v00 = static_cast<float>(src[y0 * srcStride + x0 * 4 + c]);
        float v10 = static_cast<float>(src[y0 * srcStride + x1 * 4 + c]);
        float v01 = static_cast<float>(src[y1 * srcStride + x0 * 4 + c]);
        float v11 = static_cast<float>(src[y1 * srcStride + x1 * 4 + c]);
        float top = v00 + (v10 - v00) * tx;
        float bottom = v01 + (v11 - v01) * tx;
        float value = top + (bottom - top) * ty;
        out[c] = static_cast<std::uint8_t>(std::max(0.0f, std::min(255.0f, value)));
    }
}

}  // namespace

void blitRectCopy(const RgbaImageView &src, int srcX, int srcY, int copyW, int copyH,
                  std::uint8_t *dst, int dstW, int dstH, int dstStride, int dstX, int dstY) {
    if (copyW <= 0 || copyH <= 0) {
        return;
    }
    for (int py = 0; py < copyH; ++py) {
        int sy = srcY + py;
        int dy = dstY + py;
        if (sy < 0 || sy >= src.h || dy < 0 || dy >= dstH) {
            continue;
        }
        const std::uint8_t *srcRow = src.data + sy * src.stride + srcX * 4;
        std::uint8_t *dstRow = dst + dy * dstStride + dstX * 4;
        for (int px = 0; px < copyW; ++px) {
            int sx = srcX + px;
            int dx = dstX + px;
            if (sx < 0 || sx >= src.w || dx < 0 || dx >= dstW) {
                continue;
            }
            const std::uint8_t *sp = srcRow + px * 4;
            if (sp[3] == 0) {
                continue;
            }
            std::uint8_t *dp = dstRow + px * 4;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }
}

void blitRectScaled(const RgbaImageView &src, int srcX, int srcY, int srcW, int srcH,
                    std::uint8_t *dst, int dstW, int dstH, int dstStride, int dstX, int dstY,
                    int outW, int outH) {
    if (srcW <= 0 || srcH <= 0 || outW <= 0 || outH <= 0) {
        return;
    }
    if (srcW == outW && srcH == outH) {
        blitRectCopy(src, srcX, srcY, srcW, srcH, dst, dstW, dstH, dstStride, dstX, dstY);
        return;
    }
    for (int py = 0; py < outH; ++py) {
        int dy = dstY + py;
        if (dy < 0 || dy >= dstH) {
            continue;
        }
        float fy = (static_cast<float>(py) + 0.5f) * static_cast<float>(srcH) /
                       static_cast<float>(outH) -
                   0.5f;
        for (int px = 0; px < outW; ++px) {
            int dx = dstX + px;
            if (dx < 0 || dx >= dstW) {
                continue;
            }
            float fx = (static_cast<float>(px) + 0.5f) * static_cast<float>(srcW) /
                           static_cast<float>(outW) -
                       0.5f;
            float sampleX = static_cast<float>(srcX) + fx;
            float sampleY = static_cast<float>(srcY) + fy;
            std::uint8_t pixel[4];
            sampleBilinearRGBA(src.data, src.w, src.h, src.stride, sampleX, sampleY, pixel);
            if (pixel[3] == 0) {
                continue;
            }
            std::uint8_t *dp = dst + dy * dstStride + dx * 4;
            dp[0] = pixel[0];
            dp[1] = pixel[1];
            dp[2] = pixel[2];
            dp[3] = pixel[3];
        }
    }
}

void scaleRgbaImage(const std::uint8_t *src, int srcW, int srcH, int srcStride, std::uint8_t *dst,
                    int dstW, int dstH, int dstStride) {
    RgbaImageView view{src, srcW, srcH, srcStride};
    blitRectScaled(view, 0, 0, srcW, srcH, dst, dstW, dstH, dstStride, 0, 0, dstW, dstH);
}

void compositeRgbaLayer(std::uint8_t *base, int baseW, int baseH, int baseStride,
                        const std::uint8_t *overlay, int overlayW, int overlayH, int overlayStride) {
    if (baseW != overlayW || baseH != overlayH) {
        return;
    }
    for (int y = 0; y < baseH; ++y) {
        const std::uint8_t *srcRow = overlay + y * overlayStride;
        std::uint8_t *dstRow = base + y * baseStride;
        for (int x = 0; x < baseW; ++x) {
            const std::uint8_t *sp = srcRow + x * 4;
            if (sp[3] == 0) {
                continue;
            }
            std::uint8_t *dp = dstRow + x * 4;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }
}
