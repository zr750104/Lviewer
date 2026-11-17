// Lviewer - portable image core (no Windows dependency)
#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>

namespace lv {

// Pixel = 0xAARRGGBB (little-endian memory order B,G,R,A) — straight (non-premultiplied) alpha.
using Pixel = uint32_t;

inline uint8_t A(Pixel p) { return uint8_t(p >> 24); }
inline uint8_t R(Pixel p) { return uint8_t(p >> 16); }
inline uint8_t G(Pixel p) { return uint8_t(p >> 8); }
inline uint8_t B(Pixel p) { return uint8_t(p); }
inline Pixel ARGB(int a, int r, int g, int b) {
    return (Pixel(uint8_t(a)) << 24) | (Pixel(uint8_t(r)) << 16) | (Pixel(uint8_t(g)) << 8) | Pixel(uint8_t(b));
}
inline int Clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
inline int Clamp255(double v) { return v <= 0 ? 0 : (v >= 255 ? 255 : int(v + 0.5)); }

// Semantic colour depth of the image; used when saving (e.g. BW -> 1bpp / CCITT G4 TIFF).
enum class ColorMode { Color, Gray, BW };

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
};

struct Image {
    int w = 0, h = 0;
    std::vector<Pixel> px;
    bool hasAlpha = false;
    int srcBpp = 24;           // bits per pixel of the source file (informational)
    ColorMode mode = ColorMode::Color;
    double dpiX = 96, dpiY = 96;

    Image() = default;
    Image(int W, int H, Pixel fill = 0xFFFFFFFF) { create(W, H, fill); }
    void create(int W, int H, Pixel fill = 0xFFFFFFFF) {
        w = W; h = H; px.assign(size_t(W) * size_t(H), fill);
    }
    bool valid() const { return w > 0 && h > 0 && px.size() == size_t(w) * size_t(h); }
    Pixel* row(int y) { return px.data() + size_t(y) * w; }
    const Pixel* row(int y) const { return px.data() + size_t(y) * w; }
    Pixel& at(int x, int y) { return px[size_t(y) * w + x]; }
    Pixel at(int x, int y) const { return px[size_t(y) * w + x]; }
    Rect full() const { return Rect{0, 0, w, h}; }
    void copyMeta(const Image& o) { hasAlpha = o.hasAlpha; srcBpp = o.srcBpp; mode = o.mode; dpiX = o.dpiX; dpiY = o.dpiY; }
};

// Clip r to image bounds; empty rect means "whole image".
inline Rect ClipRect(const Image& im, Rect r) {
    if (r.empty()) return im.full();
    int x0 = std::max(0, r.x), y0 = std::max(0, r.y);
    int x1 = std::min(im.w, r.right()), y1 = std::min(im.h, r.bottom());
    if (x1 <= x0 || y1 <= y0) return Rect{};
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

} // namespace lv
