// Lviewer - image processing (portable)
#pragma once
#include "Image.h"
#include <functional>

namespace lv {

enum class ResizeQuality { Nearest = 0, Bilinear = 1, Bicubic = 2, Area = 3 };

// Optional parallel-for hook: runs body(begin,end) over [0,n). Default is serial.
using ParallelFor = std::function<void(int n, const std::function<void(int, int)>& body)>;
void SetParallelFor(ParallelFor pf);
void RunParallel(int n, const std::function<void(int, int)>& body);

// ---- geometry -------------------------------------------------------------
Image Rotate90(const Image& src, bool clockwise);
void Rotate180(Image& im);
void FlipH(Image& im, Rect r = {});
void FlipV(Image& im, Rect r = {});
Image RotateAngle(const Image& src, double degrees, Pixel bg);
Image Resize(const Image& src, int w, int h, ResizeQuality q);
Image Crop(const Image& src, Rect r);
// anchor: 0..8 (row-major 3x3: 0=top-left, 4=center, 8=bottom-right)
Image Canvas(const Image& src, int w, int h, int anchor, Pixel bg);

// ---- tone / colour --------------------------------------------------------
void Invert(Image& im, Rect r = {}, int channelMask = 7);   // mask: 1=B 2=G 4=R
void Grayscale(Image& im, Rect r = {});
void AutoContrast(Image& im, Rect r = {});
void AutoLevels(Image& im, Rect r = {});
// black/white in 0..255, gamma 0.1..10
void Levels(Image& im, int black, int white, double gamma, Rect r = {});
// brightness -255..255, contrast -100..100, gamma 0.1..10
void Brightness(Image& im, int brightness, int contrast, double gamma, Rect r = {});
// hue -180..180 deg, saturation -100..100, lightness -100..100
void HueSatLight(Image& im, int hue, int sat, int light, Rect r = {});
// 0:R<->G 1:R<->B 2:G<->B 3:R->G->B->R 4:B<-R<-G<-B (reverse rotation)
void SwapChannels(Image& im, int mode, Rect r = {});

// ---- filters --------------------------------------------------------------
void Sharpen(Image& im, int level, Rect r = {});      // level 1 = normal, 2 = strong
void SharpenEdges(Image& im, Rect r = {});
void Blur(Image& im, int level, Rect r = {});         // 1 normal, 2 strong
void BlurSoft(Image& im, Rect r = {});
void GaussianBlur(Image& im, double radius, Rect r = {});
void UnsharpMask(Image& im, double radius, int amountPct, int threshold, Rect r = {});

// ---- masks (reversible scrambles, applying twice restores the image) --------
void GlassV(Image& im, int size, bool remainder, Rect r = {});
void GlassH(Image& im, int size, bool remainder, Rect r = {});
void Xor80(Image& im, int channelMask = 7, Rect r = {});
struct Q0Options { bool glassV = true, glassH = true, nega = true, xor80 = false, flipV = false, flipH = false; };
void Q0Mask(Image& im, int glassSize, const Q0Options& o, Rect r = {});
void WinMask(Image& im, Rect r = {});

// ---- colour depth ---------------------------------------------------------
enum class Depth { BW = 0, Gray16 = 1, Gray256 = 2, Color256 = 3, Color65536 = 4, FullColor = 5 };
void ReduceColors(Image& im, Depth d, bool dither);

// ---- analysis -------------------------------------------------------------
bool IsGrayscale(const Image& im);
bool IsBlackWhite(const Image& im);
bool HasTransparency(const Image& im);

// ---- display scaling (writes into dst buffer of dw*dh) --------------------
// Area-average downscale of the full source to dw x dh.
void ScaleBox(const Image& src, Image& dst, int dw, int dh);

} // namespace lv
