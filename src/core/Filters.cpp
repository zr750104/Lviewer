// Lviewer - image processing (portable implementation)
#include "Filters.h"
#include <cmath>
#include <cstring>
#include <array>

namespace lv {

static ParallelFor g_pf;
void SetParallelFor(ParallelFor pf) { g_pf = std::move(pf); }
void RunParallel(int n, const std::function<void(int, int)>& body) {
    if (n <= 0) return;
    if (g_pf && n >= 64) g_pf(n, body);
    else body(0, n);
}

static const double kPi = 3.14159265358979323846;

// ===================================================================== geometry
Image Rotate90(const Image& s, bool cw) {
    Image d; d.copyMeta(s); d.create(s.h, s.w);
    std::swap(d.dpiX, d.dpiY);
    RunParallel(s.h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const Pixel* sr = s.row(y);
            for (int x = 0; x < s.w; ++x) {
                if (cw) d.at(s.h - 1 - y, x) = sr[x];
                else    d.at(y, s.w - 1 - x) = sr[x];
            }
        }
    });
    return d;
}

void Rotate180(Image& im) { std::reverse(im.px.begin(), im.px.end()); }

void FlipH(Image& im, Rect r) {
    r = ClipRect(im, r);
    for (int y = r.y; y < r.bottom(); ++y) std::reverse(im.row(y) + r.x, im.row(y) + r.right());
}

void FlipV(Image& im, Rect r) {
    r = ClipRect(im, r);
    for (int i = 0; i < r.h / 2; ++i)
        std::swap_ranges(im.row(r.y + i) + r.x, im.row(r.y + i) + r.right(), im.row(r.bottom() - 1 - i) + r.x);
}

static inline Pixel SampleBilinear(const Image& s, double fx, double fy, Pixel bg) {
    int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
    double ax = fx - x0, ay = fy - y0;
    double acc[4] = {0, 0, 0, 0};
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            int x = x0 + i, y = y0 + j;
            Pixel p = (x >= 0 && y >= 0 && x < s.w && y < s.h) ? s.at(x, y) : bg;
            double wgt = (i ? ax : 1 - ax) * (j ? ay : 1 - ay);
            acc[0] += B(p) * wgt; acc[1] += G(p) * wgt; acc[2] += R(p) * wgt; acc[3] += A(p) * wgt;
        }
    return ARGB(Clamp255(acc[3]), Clamp255(acc[2]), Clamp255(acc[1]), Clamp255(acc[0]));
}

Image RotateAngle(const Image& s, double deg, Pixel bg) {
    double a = deg * kPi / 180.0, c = std::cos(a), sn = std::sin(a);
    int w = int(std::ceil(std::fabs(s.w * c) + std::fabs(s.h * sn) - 1e-6));
    int h = int(std::ceil(std::fabs(s.w * sn) + std::fabs(s.h * c) - 1e-6));
    Image d; d.copyMeta(s); d.create(std::max(1, w), std::max(1, h), bg);
    if (A(bg) != 255) d.hasAlpha = true;
    double cx = s.w / 2.0, cy = s.h / 2.0, dcx = d.w / 2.0, dcy = d.h / 2.0;
    RunParallel(d.h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < d.w; ++x) {
                double dx = x + 0.5 - dcx, dy = y + 0.5 - dcy;
                // inverse rotation (clockwise positive on screen)
                double sx = dx * c + dy * sn + cx - 0.5;
                double sy = -dx * sn + dy * c + cy - 0.5;
                if (sx < -1 || sy < -1 || sx > s.w || sy > s.h) continue;
                d.at(x, y) = SampleBilinear(s, sx, sy, bg);
            }
    });
    if (d.mode == ColorMode::BW) d.mode = ColorMode::Gray;
    return d;
}

Image Crop(const Image& s, Rect r) {
    r = ClipRect(s, r);
    Image d; d.copyMeta(s);
    if (r.empty()) return d;
    d.create(r.w, r.h);
    for (int y = 0; y < r.h; ++y) std::memcpy(d.row(y), s.row(r.y + y) + r.x, sizeof(Pixel) * r.w);
    return d;
}

Image Canvas(const Image& s, int w, int h, int anchor, Pixel bg) {
    Image d; d.copyMeta(s); d.create(std::max(1, w), std::max(1, h), bg);
    if (A(bg) != 255) d.hasAlpha = true;
    int ax = anchor % 3, ay = anchor / 3;
    int ox = ax == 0 ? 0 : (ax == 1 ? (w - s.w) / 2 : w - s.w);
    int oy = ay == 0 ? 0 : (ay == 1 ? (h - s.h) / 2 : h - s.h);
    for (int y = 0; y < s.h; ++y) {
        int dy = y + oy; if (dy < 0 || dy >= d.h) continue;
        for (int x = 0; x < s.w; ++x) {
            int dx = x + ox; if (dx < 0 || dx >= d.w) continue;
            d.at(dx, dy) = s.at(x, y);
        }
    }
    return d;
}

// ---- separable resampling ----------------------------------------------------
namespace {
struct Contrib { int start = 0; std::vector<float> w; };

double KernelBilinear(double x) { x = std::fabs(x); return x < 1 ? 1 - x : 0; }
double KernelCubic(double x) {   // Catmull-Rom
    const double a = -0.5; x = std::fabs(x);
    if (x < 1) return (a + 2) * x * x * x - (a + 3) * x * x + 1;
    if (x < 2) return a * x * x * x - 5 * a * x * x + 8 * a * x - 4 * a;
    return 0;
}

std::vector<Contrib> MakeContribs(int srcN, int dstN, ResizeQuality q) {
    std::vector<Contrib> out(dstN);
    double scale = double(dstN) / srcN;
    for (int i = 0; i < dstN; ++i) {
        Contrib& c = out[i];
        double center = (i + 0.5) / scale;   // in source pixel space
        if (q == ResizeQuality::Area) {
            double l = i / scale, r = (i + 1) / scale;
            int s0 = int(std::floor(l)), s1 = int(std::ceil(r));
            s0 = std::max(0, s0); s1 = std::min(srcN, std::max(s1, s0 + 1));
            c.start = s0;
            double sum = 0;
            for (int s = s0; s < s1; ++s) {
                double ov = std::min<double>(s + 1, r) - std::max<double>(s, l);
                if (ov < 0) ov = 0;
                if (scale > 1) ov = 1;     // upscale with "area" == nearest-ish box
                c.w.push_back(float(ov)); sum += ov;
            }
            if (scale > 1) { c.start = std::min(srcN - 1, int(center)); c.w.assign(1, 1.f); sum = 1; }
            for (auto& v : c.w) v = float(v / (sum > 0 ? sum : 1));
            continue;
        }
        double support = (q == ResizeQuality::Bicubic) ? 2.0 : 1.0;
        double fscale = scale < 1 ? 1.0 / scale : 1.0;
        double sup = support * fscale;
        int s0 = int(std::floor(center - sup)), s1 = int(std::ceil(center + sup));
        s0 = std::max(0, s0); s1 = std::min(srcN, s1);
        if (s1 <= s0) { s0 = std::min(srcN - 1, std::max(0, int(center))); s1 = s0 + 1; }
        c.start = s0;
        double sum = 0;
        for (int s = s0; s < s1; ++s) {
            double x = (s + 0.5 - center) / fscale;
            double v = (q == ResizeQuality::Bicubic) ? KernelCubic(x) : KernelBilinear(x);
            c.w.push_back(float(v)); sum += v;
        }
        if (sum == 0) { c.w.assign(1, 1.f); c.start = std::min(srcN - 1, std::max(0, int(center))); sum = 1; }
        for (auto& v : c.w) v = float(v / sum);
    }
    return out;
}

inline void Premul(Pixel p, float* o) {
    float a = A(p) / 255.f;
    o[0] = B(p) * a; o[1] = G(p) * a; o[2] = R(p) * a; o[3] = float(A(p));
}
inline Pixel Unpremul(const float* v) {
    float a = v[3];
    if (a <= 0.5f) return 0;
    float k = 255.f / a;
    return ARGB(Clamp255(double(a)), Clamp255(double(v[2] * k)), Clamp255(double(v[1] * k)), Clamp255(double(v[0] * k)));
}
} // namespace

Image Resize(const Image& s, int w, int h, ResizeQuality q) {
    Image d; d.copyMeta(s);
    w = std::max(1, w); h = std::max(1, h);
    d.create(w, h);
    if (!s.valid()) return d;
    if (q == ResizeQuality::Nearest) {
        RunParallel(h, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                int sy = std::min(s.h - 1, int((y + 0.5) * s.h / h));
                const Pixel* sr = s.row(sy); Pixel* dr = d.row(y);
                for (int x = 0; x < w; ++x) dr[x] = sr[std::min(s.w - 1, int((x + 0.5) * s.w / w))];
            }
        });
        return d;
    }
    auto cx = MakeContribs(s.w, w, q);
    auto cy = MakeContribs(s.h, h, q);
    // horizontal pass into float premultiplied buffer (s.h rows x w)
    std::vector<float> tmp(size_t(w) * s.h * 4);
    RunParallel(s.h, [&](int y0, int y1) {
        std::vector<float> rowp(size_t(s.w) * 4);
        for (int y = y0; y < y1; ++y) {
            const Pixel* sr = s.row(y);
            for (int x = 0; x < s.w; ++x) Premul(sr[x], &rowp[size_t(x) * 4]);
            float* tr = &tmp[size_t(y) * w * 4];
            for (int x = 0; x < w; ++x) {
                const Contrib& c = cx[x];
                float a0 = 0, a1 = 0, a2 = 0, a3 = 0;
                for (size_t k = 0; k < c.w.size(); ++k) {
                    const float* p = &rowp[size_t(c.start + k) * 4]; float wt = c.w[k];
                    a0 += p[0] * wt; a1 += p[1] * wt; a2 += p[2] * wt; a3 += p[3] * wt;
                }
                tr[x * 4] = a0; tr[x * 4 + 1] = a1; tr[x * 4 + 2] = a2; tr[x * 4 + 3] = a3;
            }
        }
    });
    RunParallel(h, [&](int y0, int y1) {
        std::vector<float> acc(size_t(w) * 4);
        for (int y = y0; y < y1; ++y) {
            std::fill(acc.begin(), acc.end(), 0.f);
            const Contrib& c = cy[y];
            for (size_t k = 0; k < c.w.size(); ++k) {
                const float* tr = &tmp[size_t(c.start + k) * w * 4]; float wt = c.w[k];
                for (int i = 0; i < w * 4; ++i) acc[i] += tr[i] * wt;
            }
            Pixel* dr = d.row(y);
            for (int x = 0; x < w; ++x) dr[x] = Unpremul(&acc[size_t(x) * 4]);
        }
    });
    if (d.mode == ColorMode::BW) d.mode = ColorMode::Gray;
    return d;
}

void ScaleBox(const Image& s, Image& d, int dw, int dh) {
    d = Resize(s, dw, dh, ResizeQuality::Area);
}

// ================================================================== tone/colour
template <class F> static void ForEachPixel(Image& im, Rect r, F f) {
    r = ClipRect(im, r);
    if (r.empty()) return;
    RunParallel(r.h, [&](int y0, int y1) {
        for (int y = r.y + y0; y < r.y + y1; ++y) {
            Pixel* p = im.row(y) + r.x;
            for (int x = 0; x < r.w; ++x) p[x] = f(p[x]);
        }
    });
}

static void ApplyLUT(Image& im, Rect r, const uint8_t lr[256], const uint8_t lg[256], const uint8_t lb[256]) {
    ForEachPixel(im, r, [&](Pixel p) { return ARGB(A(p), lr[R(p)], lg[G(p)], lb[B(p)]); });
}

static inline int Luma(Pixel p) { return (R(p) * 299 + G(p) * 587 + B(p) * 114 + 500) / 1000; }

void Invert(Image& im, Rect r, int m) {
    Pixel x = ((m & 4) ? 0xFF0000u : 0) | ((m & 2) ? 0xFF00u : 0) | ((m & 1) ? 0xFFu : 0);
    ForEachPixel(im, r, [&](Pixel p) { return p ^ x; });
}

void Grayscale(Image& im, Rect r) {
    ForEachPixel(im, r, [](Pixel p) { int l = Luma(p); return ARGB(A(p), l, l, l); });
    if (ClipRect(im, r).w == im.w && ClipRect(im, r).h == im.h && im.mode == ColorMode::Color) im.mode = ColorMode::Gray;
}

static void Histogram(const Image& im, Rect r, int hr[256], int hg[256], int hb[256], int hl[256]) {
    std::fill(hr, hr + 256, 0); std::fill(hg, hg + 256, 0); std::fill(hb, hb + 256, 0); std::fill(hl, hl + 256, 0);
    r = ClipRect(im, r);
    for (int y = r.y; y < r.bottom(); ++y) {
        const Pixel* p = im.row(y) + r.x;
        for (int x = 0; x < r.w; ++x) { hr[R(p[x])]++; hg[G(p[x])]++; hb[B(p[x])]++; hl[Luma(p[x])]++; }
    }
}

static void StretchRange(const int h[256], long total, int& lo, int& hi) {
    long clip = total / 200;  // 0.5%
    long acc = 0; lo = 0;
    for (int i = 0; i < 256; ++i) { acc += h[i]; if (acc > clip) { lo = i; break; } }
    acc = 0; hi = 255;
    for (int i = 255; i >= 0; --i) { acc += h[i]; if (acc > clip) { hi = i; break; } }
    if (hi <= lo) { lo = 0; hi = 255; }
}

static void MakeStretchLUT(uint8_t* lut, int lo, int hi) {
    for (int i = 0; i < 256; ++i) lut[i] = uint8_t(Clamp255(double(i - lo) * 255.0 / (hi - lo)));
}

void AutoContrast(Image& im, Rect r) {
    int hr[256], hg[256], hb[256], hl[256];
    Histogram(im, r, hr, hg, hb, hl);
    Rect c = ClipRect(im, r);
    long total = long(c.w) * c.h;
    int lo, hi; StretchRange(hl, total, lo, hi);
    uint8_t lut[256]; MakeStretchLUT(lut, lo, hi);
    ApplyLUT(im, r, lut, lut, lut);
}

void AutoLevels(Image& im, Rect r) {
    int hr[256], hg[256], hb[256], hl[256];
    Histogram(im, r, hr, hg, hb, hl);
    Rect c = ClipRect(im, r);
    long total = long(c.w) * c.h;
    uint8_t lr[256], lg[256], lb[256];
    int lo, hi;
    StretchRange(hr, total, lo, hi); MakeStretchLUT(lr, lo, hi);
    StretchRange(hg, total, lo, hi); MakeStretchLUT(lg, lo, hi);
    StretchRange(hb, total, lo, hi); MakeStretchLUT(lb, lo, hi);
    ApplyLUT(im, r, lr, lg, lb);
}

void Levels(Image& im, int black, int white, double gamma, Rect r) {
    black = std::max(0, std::min(254, black)); white = std::max(black + 1, std::min(255, white));
    gamma = std::max(0.1, std::min(10.0, gamma));
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        double v = double(i - black) / (white - black);
        v = v < 0 ? 0 : (v > 1 ? 1 : v);
        lut[i] = uint8_t(Clamp255(std::pow(v, 1.0 / gamma) * 255.0));
    }
    ApplyLUT(im, r, lut, lut, lut);
}

void Brightness(Image& im, int b, int c, double gamma, Rect r) {
    gamma = std::max(0.1, std::min(10.0, gamma));
    double f = c >= 0 ? 1.0 + c / 100.0 * 3.0 : 1.0 + c / 100.0;   // contrast factor 0..4
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        double v = i + b;
        v = (v - 127.5) * f + 127.5;
        v = v < 0 ? 0 : (v > 255 ? 255 : v);
        v = std::pow(v / 255.0, 1.0 / gamma) * 255.0;
        lut[i] = uint8_t(Clamp255(v));
    }
    ApplyLUT(im, r, lut, lut, lut);
}

static void RgbToHsl(int r, int g, int b, double& h, double& s, double& l) {
    double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
    double mx = std::max({rf, gf, bf}), mn = std::min({rf, gf, bf});
    l = (mx + mn) / 2;
    if (mx == mn) { h = s = 0; return; }
    double d = mx - mn;
    s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    if (mx == rf) h = (gf - bf) / d + (gf < bf ? 6 : 0);
    else if (mx == gf) h = (bf - rf) / d + 2;
    else h = (rf - gf) / d + 4;
    h *= 60;
}
static double Hue2Rgb(double p, double q, double t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}
static void HslToRgb(double h, double s, double l, int& r, int& g, int& b) {
    if (s <= 0) { r = g = b = Clamp255(l * 255); return; }
    double q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q, hh = h / 360.0;
    r = Clamp255(Hue2Rgb(p, q, hh + 1.0 / 3) * 255);
    g = Clamp255(Hue2Rgb(p, q, hh) * 255);
    b = Clamp255(Hue2Rgb(p, q, hh - 1.0 / 3) * 255);
}

void HueSatLight(Image& im, int hue, int sat, int light, Rect r) {
    double sf = 1.0 + sat / 100.0, lf = light / 100.0;
    ForEachPixel(im, r, [&](Pixel p) {
        double h, s, l; RgbToHsl(R(p), G(p), B(p), h, s, l);
        h += hue; while (h < 0) h += 360; while (h >= 360) h -= 360;
        s = std::max(0.0, std::min(1.0, s * sf));
        l = lf >= 0 ? l + (1 - l) * lf : l + l * lf;
        int rr, gg, bb; HslToRgb(h, s, l, rr, gg, bb);
        return ARGB(A(p), rr, gg, bb);
    });
    if (sat > 0 && im.mode != ColorMode::Color) im.mode = ColorMode::Color;
}

void SwapChannels(Image& im, int mode, Rect r) {
    ForEachPixel(im, r, [&](Pixel p) {
        int rr = R(p), gg = G(p), bb = B(p);
        switch (mode) {
            case 0: std::swap(rr, gg); break;
            case 1: std::swap(rr, bb); break;
            case 2: std::swap(gg, bb); break;
            case 3: { int t = bb; bb = gg; gg = rr; rr = t; } break;  // R->G->B->R
            case 4: { int t = rr; rr = gg; gg = bb; bb = t; } break;  // reverse
        }
        return ARGB(A(p), rr, gg, bb);
    });
}

// ======================================================================= filters
static void Convolve(Image& im, Rect r, const int* k, int ksize, int div, int bias = 0) {
    r = ClipRect(im, r);
    if (r.empty()) return;
    Image src = im;  // read from untouched copy
    int half = ksize / 2;
    RunParallel(r.h, [&](int y0, int y1) {
        for (int y = r.y + y0; y < r.y + y1; ++y)
            for (int x = r.x; x < r.right(); ++x) {
                int ab = 0, ag = 0, ar = 0;
                for (int j = 0; j < ksize; ++j) {
                    int sy = std::max(0, std::min(src.h - 1, y + j - half));
                    const Pixel* sr = src.row(sy);
                    for (int i = 0; i < ksize; ++i) {
                        int sx = std::max(0, std::min(src.w - 1, x + i - half));
                        int kv = k[j * ksize + i];
                        if (!kv) continue;
                        Pixel p = sr[sx];
                        ab += B(p) * kv; ag += G(p) * kv; ar += R(p) * kv;
                    }
                }
                Pixel o = src.at(x, y);
                im.at(x, y) = ARGB(A(o), Clamp255(ar / div + bias), Clamp255(ag / div + bias), Clamp255(ab / div + bias));
            }
    });
    if (im.mode == ColorMode::BW) im.mode = ColorMode::Gray;
}

void Sharpen(Image& im, int level, Rect r) {
    static const int k1[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
    static const int k2[9] = {-1, -1, -1, -1, 9, -1, -1, -1, -1};
    Convolve(im, r, level >= 2 ? k2 : k1, 3, 1);
}

void SharpenEdges(Image& im, Rect r) {
    r = ClipRect(im, r);
    Image orig = im;
    Sharpen(im, 2, r);
    // keep sharpened pixels only on edges (Sobel magnitude above threshold)
    for (int y = r.y; y < r.bottom(); ++y)
        for (int x = r.x; x < r.right(); ++x) {
            auto L = [&](int xx, int yy) {
                xx = std::max(0, std::min(orig.w - 1, xx)); yy = std::max(0, std::min(orig.h - 1, yy));
                return Luma(orig.at(xx, yy));
            };
            int gx = -L(x - 1, y - 1) - 2 * L(x - 1, y) - L(x - 1, y + 1) + L(x + 1, y - 1) + 2 * L(x + 1, y) + L(x + 1, y + 1);
            int gy = -L(x - 1, y - 1) - 2 * L(x, y - 1) - L(x + 1, y - 1) + L(x - 1, y + 1) + 2 * L(x, y + 1) + L(x + 1, y + 1);
            if (std::abs(gx) + std::abs(gy) < 96) im.at(x, y) = orig.at(x, y);
        }
}

void Blur(Image& im, int level, Rect r) {
    static const int k3[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    static const int k5[25] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    if (level >= 2) Convolve(im, r, k5, 5, 25);
    else Convolve(im, r, k3, 3, 9);
}

void BlurSoft(Image& im, Rect r) {
    static const int k[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
    Convolve(im, r, k, 3, 16);
}

static Image GaussianCopy(const Image& src, double radius) {
    double sigma = std::max(0.3, radius / 2.0);
    int ks = std::max(1, int(std::ceil(sigma * 3)));
    std::vector<float> k(2 * ks + 1);
    double sum = 0;
    for (int i = -ks; i <= ks; ++i) { k[i + ks] = float(std::exp(-(i * i) / (2 * sigma * sigma))); sum += k[i + ks]; }
    for (auto& v : k) v = float(v / sum);
    Image tmp = src, out = src;
    RunParallel(src.h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < src.w; ++x) {
                float a[4] = {0, 0, 0, 0};
                for (int i = -ks; i <= ks; ++i) {
                    Pixel p = src.at(std::max(0, std::min(src.w - 1, x + i)), y);
                    float wt = k[i + ks];
                    a[0] += B(p) * wt; a[1] += G(p) * wt; a[2] += R(p) * wt; a[3] += A(p) * wt;
                }
                tmp.at(x, y) = ARGB(Clamp255(double(a[3])), Clamp255(double(a[2])), Clamp255(double(a[1])), Clamp255(double(a[0])));
            }
    });
    RunParallel(src.h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < src.w; ++x) {
                float a[4] = {0, 0, 0, 0};
                for (int i = -ks; i <= ks; ++i) {
                    Pixel p = tmp.at(x, std::max(0, std::min(src.h - 1, y + i)));
                    float wt = k[i + ks];
                    a[0] += B(p) * wt; a[1] += G(p) * wt; a[2] += R(p) * wt; a[3] += A(p) * wt;
                }
                out.at(x, y) = ARGB(Clamp255(double(a[3])), Clamp255(double(a[2])), Clamp255(double(a[1])), Clamp255(double(a[0])));
            }
    });
    return out;
}

void GaussianBlur(Image& im, double radius, Rect r) {
    r = ClipRect(im, r);
    if (r.empty() || radius <= 0) return;
    Image blurred = GaussianCopy(im, radius);
    for (int y = r.y; y < r.bottom(); ++y)
        std::memcpy(im.row(y) + r.x, blurred.row(y) + r.x, sizeof(Pixel) * r.w);
    if (im.mode == ColorMode::BW) im.mode = ColorMode::Gray;
}

void UnsharpMask(Image& im, double radius, int amount, int threshold, Rect r) {
    r = ClipRect(im, r);
    if (r.empty() || radius <= 0) return;
    Image bl = GaussianCopy(im, radius);
    double a = amount / 100.0;
    for (int y = r.y; y < r.bottom(); ++y)
        for (int x = r.x; x < r.right(); ++x) {
            Pixel o = im.at(x, y), b = bl.at(x, y);
            auto ch = [&](int ov, int bv) {
                int diff = ov - bv;
                if (std::abs(diff) < threshold) return ov;
                return Clamp255(ov + a * diff);
            };
            im.at(x, y) = ARGB(A(o), ch(R(o), R(b)), ch(G(o), G(b)), ch(B(o), B(b)));
        }
    if (im.mode == ColorMode::BW) im.mode = ColorMode::Gray;
}

// ========================================================================= masks
void GlassV(Image& im, int size, bool rem, Rect r) {
    r = ClipRect(im, r);
    if (size < 2) return;
    for (int bx = r.x; bx < r.right(); bx += size) {
        int bw = std::min(size, r.right() - bx);
        if (bw < size && !rem) break;
        FlipH(im, Rect{bx, r.y, bw, r.h});
    }
}

void GlassH(Image& im, int size, bool rem, Rect r) {
    r = ClipRect(im, r);
    if (size < 2) return;
    for (int by = r.y; by < r.bottom(); by += size) {
        int bh = std::min(size, r.bottom() - by);
        if (bh < size && !rem) break;
        FlipV(im, Rect{r.x, by, r.w, bh});
    }
}

void Xor80(Image& im, int m, Rect r) {
    Pixel x = ((m & 4) ? 0x800000u : 0) | ((m & 2) ? 0x8000u : 0) | ((m & 1) ? 0x80u : 0);
    ForEachPixel(im, r, [&](Pixel p) { return p ^ x; });
}

void Q0Mask(Image& im, int size, const Q0Options& o, Rect r) {
    r = ClipRect(im, r);
    if (size < 2) size = 8;
    Rect rv = r;
    if ((o.glassV && o.flipH)) rv.w -= rv.w % size;
    if ((o.glassH && o.flipV)) rv.h -= rv.h % size;
    if (o.glassV) GlassV(im, size, false, rv);
    if (o.glassH) GlassH(im, size, false, rv);
    if (o.nega) Invert(im, r);
    if (o.xor80) Xor80(im, 7, r);
    if (o.flipV) FlipV(im, rv);
    if (o.flipH) FlipH(im, rv);
}

void WinMask(Image& im, Rect r) {
    static const int order[16] = {12, 8, 6, 15, 9, 13, 2, 11, 1, 4, 14, 7, 0, 5, 10, 3};
    r = ClipRect(im, r);
    std::vector<Pixel> tmp(16);
    for (int y = r.y; y < r.bottom(); ++y) {
        Pixel* row = im.row(y);
        for (int bx = r.x; bx + 16 <= r.right(); bx += 16) {
            for (int i = 0; i < 16; ++i) tmp[i] = row[bx + order[i]];
            std::copy(tmp.begin(), tmp.end(), row + bx);
        }
    }
}

// ================================================================= colour depth
void ReduceColors(Image& im, Depth d, bool dither) {
    if (!im.valid()) return;
    int w = im.w, h = im.h;
    if (d == Depth::FullColor) { im.mode = ColorMode::Color; im.srcBpp = im.hasAlpha ? 32 : 24; return; }
    if (d == Depth::Gray256) { Grayscale(im); im.mode = ColorMode::Gray; im.srcBpp = 8; return; }

    // error-diffusion helpers on float planes
    std::vector<float> er(size_t(w) * 2 * 3, 0.f);   // two rows x 3 channels
    auto quant = [&](int c, float v) -> int {
        switch (d) {
            case Depth::BW: return v >= 128 ? 255 : 0;
            case Depth::Gray16: return Clamp255(std::round(v / 17.0) * 17.0);
            case Depth::Color256: {  // 6 (R) x 7 (G) x 6 (B)
                int lv = (c == 1) ? 7 : 6; double step = 255.0 / (lv - 1);
                return Clamp255(std::round(v / step) * step);
            }
            case Depth::Color65536: {
                int bits = (c == 1) ? 6 : 5; int lv = (1 << bits) - 1; double step = 255.0 / lv;
                return Clamp255(std::round(v / step) * step);
            }
            default: return Clamp255(double(v));
        }
    };
    bool gray = (d == Depth::BW || d == Depth::Gray16);
    for (int y = 0; y < h; ++y) {
        float* cur = &er[size_t(y & 1) * w * 3];
        float* nxt = &er[size_t((y + 1) & 1) * w * 3];
        std::fill(nxt, nxt + w * 3, 0.f);
        Pixel* row = im.row(y);
        for (int x = 0; x < w; ++x) {
            Pixel p = row[x];
            float in[3];
            if (gray) { float l = float(Luma(p)); in[0] = in[1] = in[2] = l; }
            else { in[0] = float(R(p)); in[1] = float(G(p)); in[2] = float(B(p)); }
            int out[3];
            for (int c = 0; c < 3; ++c) {
                float v = in[c] + (dither ? cur[x * 3 + c] : 0.f);
                out[c] = quant(c, v);
                if (dither) {
                    float e = v - out[c];
                    if (x + 1 < w) cur[(x + 1) * 3 + c] += e * 7 / 16;
                    if (x > 0) nxt[(x - 1) * 3 + c] += e * 3 / 16;
                    nxt[x * 3 + c] += e * 5 / 16;
                    if (x + 1 < w) nxt[(x + 1) * 3 + c] += e * 1 / 16;
                }
                if (gray) { out[1] = out[2] = out[0]; break; }
            }
            row[x] = ARGB(A(p), out[0], out[1], out[2]);
        }
    }
    switch (d) {
        case Depth::BW: im.mode = ColorMode::BW; im.srcBpp = 1; break;
        case Depth::Gray16: im.mode = ColorMode::Gray; im.srcBpp = 4; break;
        case Depth::Color256: im.mode = ColorMode::Color; im.srcBpp = 8; break;
        case Depth::Color65536: im.mode = ColorMode::Color; im.srcBpp = 16; break;
        default: break;
    }
}

// ====================================================================== analysis
bool IsGrayscale(const Image& im) {
    for (Pixel p : im.px) if (R(p) != G(p) || G(p) != B(p)) return false;
    return true;
}
bool IsBlackWhite(const Image& im) {
    for (Pixel p : im.px) { Pixel c = p & 0xFFFFFF; if (c != 0 && c != 0xFFFFFF) return false; }
    return true;
}
bool HasTransparency(const Image& im) {
    for (Pixel p : im.px) if (A(p) != 255) return true;
    return false;
}

} // namespace lv
