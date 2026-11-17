// Host-side unit tests for the portable core (build with -DLVIEWER_TESTS=ON on Linux/macOS/Windows)
#include "core/Filters.h"
#include "core/NaturalSort.h"
#include "core/LangFile.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <set>
#include <string>

using namespace lv;
static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static Image Pattern(int w, int h) {
    Image im(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) im.at(x, y) = ARGB(255, (x * 37 + y) & 255, (y * 53 + x * 3) & 255, (x * y) & 255);
    return im;
}
static bool Same(const Image& a, const Image& b) { return a.w == b.w && a.h == b.h && a.px == b.px; }

static void TestGeometry() {
    Image a = Pattern(37, 21);
    Image r = Rotate90(Rotate90(Rotate90(Rotate90(a, true), true), true), true);
    CHECK(Same(a, r));
    Image l = Rotate90(Rotate90(a, true), false);
    CHECK(Same(a, l));
    Image c = Rotate90(a, true);
    CHECK(c.w == 21 && c.h == 37 && c.at(20, 0) == a.at(0, 0));
    Image f = a; FlipH(f); FlipH(f); CHECK(Same(a, f));
    f = a; FlipV(f); CHECK(f.at(0, 0) == a.at(0, 20)); FlipV(f); CHECK(Same(a, f));
    f = a; Rotate180(f); CHECK(f.at(0, 0) == a.at(36, 20));
    Image cr = Crop(a, Rect{5, 6, 10, 4});
    CHECK(cr.w == 10 && cr.h == 4 && cr.at(0, 0) == a.at(5, 6));
    Image cv = Canvas(a, 50, 30, 4, 0xFF000000);
    CHECK(cv.w == 50 && cv.h == 30 && cv.at((50 - 37) / 2, (30 - 21) / 2) == a.at(0, 0));
    Image ra = RotateAngle(a, 90, 0xFF000000);
    CHECK(ra.w == 21 && ra.h == 37);
}

static void TestResize() {
    Image a(64, 32, ARGB(255, 10, 200, 30));
    for (auto q : {ResizeQuality::Nearest, ResizeQuality::Bilinear, ResizeQuality::Bicubic, ResizeQuality::Area}) {
        Image d = Resize(a, 17, 9, q);
        CHECK(d.w == 17 && d.h == 9);
        bool flat = true;
        for (Pixel p : d.px) if (std::abs(R(p) - 10) > 1 || std::abs(G(p) - 200) > 1 || std::abs(B(p) - 30) > 1 || A(p) != 255) flat = false;
        CHECK(flat);
        Image u = Resize(a, 130, 70, q);
        CHECK(u.w == 130 && u.h == 70);
    }
    // area average of a 2x1 black/white pair -> mid gray
    Image bw(2, 1); bw.at(0, 0) = 0xFF000000; bw.at(1, 0) = 0xFFFFFFFF;
    Image g = Resize(bw, 1, 1, ResizeQuality::Area);
    CHECK(std::abs(R(g.at(0, 0)) - 128) <= 1);
}

static void TestTone() {
    Image a = Pattern(16, 16), b = a;
    Invert(b); Invert(b); CHECK(Same(a, b));
    Xor80(b); Xor80(b); CHECK(Same(a, b));
    for (int m = 0; m < 3; ++m) { b = a; SwapChannels(b, m); SwapChannels(b, m); CHECK(Same(a, b)); }
    b = a; SwapChannels(b, 3); SwapChannels(b, 4); CHECK(Same(a, b));
    b = a; Brightness(b, 0, 0, 1.0); CHECK(Same(a, b));
    b = a; Levels(b, 0, 255, 1.0); CHECK(Same(a, b));
    b = a; HueSatLight(b, 0, 0, 0);
    int maxd = 0; for (size_t i = 0; i < a.px.size(); ++i) maxd = std::max({maxd, std::abs(R(a.px[i]) - R(b.px[i])), std::abs(G(a.px[i]) - G(b.px[i])), std::abs(B(a.px[i]) - B(b.px[i]))});
    CHECK(maxd <= 1);
    b = a; Grayscale(b); CHECK(IsGrayscale(b)); CHECK(b.mode == ColorMode::Gray);
    b = a; ReduceColors(b, Depth::BW, true); CHECK(IsBlackWhite(b)); CHECK(b.mode == ColorMode::BW && b.srcBpp == 1);
    b = a; AutoLevels(b); AutoContrast(b); CHECK(b.valid());
}

static void TestMasks() {
    Image a = Pattern(70, 45), b = a;
    GlassV(b, 8, false); CHECK(!Same(a, b)); GlassV(b, 8, false); CHECK(Same(a, b));
    GlassH(b, 8, true); GlassH(b, 8, true); CHECK(Same(a, b));
    WinMask(b); CHECK(!Same(a, b)); WinMask(b); CHECK(Same(a, b));
    Q0Options o; o.flipH = true; o.flipV = true; o.xor80 = true;
    Q0Mask(b, 8, o); Q0Mask(b, 8, o); CHECK(Same(a, b));
    Rect r{3, 4, 33, 20};
    b = a; GlassV(b, 4, true, r); GlassV(b, 4, true, r); CHECK(Same(a, b));
}

static void TestFilters() {
    Image a(20, 20, 0xFF808080), b = a;
    Sharpen(b, 1); CHECK(Same(a, b));
    Blur(b, 2); CHECK(Same(a, b));
    GaussianBlur(b, 2.0); CHECK(Same(a, b));
    UnsharpMask(b, 2.0, 150, 0); CHECK(Same(a, b));
    Image p = Pattern(30, 30);
    Image q = p; SharpenEdges(q); BlurSoft(q); CHECK(q.valid());
}

static void TestSort() {
    CHECK(CompareNames(L"img2.png", L"img10.png", true) < 0);
    CHECK(CompareNames(L"img2.png", L"img10.png", false) > 0);
    CHECK(CompareNames(L"IMG_001", L"img_1", true) < 0);
    CHECK(CompareNames(L"a", L"A", true) == 0);
    CHECK(CompareNames(L"page９", L"page１０", true) < 0);   // full-width digits
}

static std::string ReadFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

static void TestLang(const std::string& dir) {
    auto en = ParseLangFile(ReadFile(dir + "/en.ini"));
    auto ja = ParseLangFile(ReadFile(dir + "/ja.ini"));
    auto th = ParseLangFile(ReadFile(dir + "/th.ini"));
    CHECK(en.size() > 300);
    CHECK(en.size() == ja.size());
    CHECK(en.size() == th.size());
    for (auto& kv : en) {
        if (!ja.count(kv.first)) { std::printf("  ja missing %s\n", kv.first.c_str()); ++g_fail; }
        if (!th.count(kv.first)) { std::printf("  th missing %s\n", kv.first.c_str()); ++g_fail; }
        auto fmtCount = [](const std::string& s) { size_t n = 0, k = 0; while ((k = s.find("%ls", k)) != std::string::npos) { ++n; k += 3; } return n; };
        if (ja.count(kv.first) && fmtCount(ja[kv.first]) != fmtCount(kv.second)) { std::printf("  ja format mismatch %s\n", kv.first.c_str()); ++g_fail; }
        if (th.count(kv.first) && fmtCount(th[kv.first]) != fmtCount(kv.second)) { std::printf("  th format mismatch %s\n", kv.first.c_str()); ++g_fail; }
    }
    auto p = ParseLangFile("\xEF\xBB\xBF# c\na = b\\nc\n[x]\nk=v=w\n");
    CHECK(p["a"] == "b\nc");
    CHECK(p["k"] == "v=w");
}

int main(int argc, char** argv) {
    TestGeometry();
    TestResize();
    TestTone();
    TestMasks();
    TestFilters();
    TestSort();
    TestLang(argc > 1 ? argv[1] : "res/lang");
    std::printf(g_fail ? "%d FAILURE(S)\n" : "all tests passed\n", g_fail);
    return g_fail ? 1 : 0;
}
