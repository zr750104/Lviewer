#include "Codec.h"
#include "I18n.h"
#include "core/Filters.h"
#include <cstring>

namespace lv {

IWICImagingFactory* Wic() {
    static Com<IWICImagingFactory> f;
    if (!f) {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(f.put()))))
            CoCreateInstance(CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(f.put()));
    }
    return f.get();
}

// ------------------------------------------------------------------- codecs
std::vector<CodecDesc> ListCodecs() {
    std::vector<CodecDesc> out;
    if (!Wic()) return out;
    Com<IEnumUnknown> en;
    if (FAILED(Wic()->CreateComponentEnumerator(WICDecoder, WICComponentEnumerateDefault, en.put()))) return out;
    for (;;) {
        Com<IUnknown> u; ULONG got = 0;
        if (en->Next(1, u.put(), &got) != S_OK || !got) break;
        Com<IWICBitmapCodecInfo> ci;
        if (FAILED(u->QueryInterface(IID_PPV_ARGS(ci.put())))) continue;
        UINT n = 0;
        CodecDesc d;
        ci->GetFriendlyName(0, nullptr, &n);
        d.name.resize(n); if (n) { ci->GetFriendlyName(n, &d.name[0], &n); d.name.resize(n ? n - 1 : 0); }
        ci->GetFileExtensions(0, nullptr, &n);
        std::wstring ex(n, 0); if (n) { ci->GetFileExtensions(n, &ex[0], &n); ex.resize(n ? n - 1 : 0); }
        for (auto& e : Split(ex, L',')) {
            std::wstring t = ToLower(Trim(e));
            if (!t.empty() && t[0] == L'.') t = t.substr(1);
            if (!t.empty()) d.exts.push_back(t);
        }
        GUID cf = {};
        ci->GetContainerFormat(&cf);
        Com<IWICBitmapEncoder> enc;
        d.canWrite = SUCCEEDED(Wic()->CreateEncoder(cf, nullptr, enc.put()));
        out.push_back(d);
    }
    return out;
}

const std::vector<std::wstring>& ImageExtensions() {
    static std::vector<std::wstring> v;
    if (v.empty()) {
        for (auto& c : ListCodecs()) for (auto& e : c.exts)
            if (std::find(v.begin(), v.end(), e) == v.end()) v.push_back(e);
        // always-known baseline (Windows ships these WIC codecs)
        for (const wchar_t* e : {L"bmp", L"dib", L"gif", L"jpg", L"jpeg", L"jpe", L"jfif", L"png", L"tif", L"tiff", L"ico", L"wdp", L"jxr"})
            if (std::find(v.begin(), v.end(), e) == v.end()) v.push_back(e);
    }
    return v;
}

bool IsImageExt(const std::wstring& ext) {
    auto& v = ImageExtensions();
    return std::find(v.begin(), v.end(), ToLower(ext)) != v.end();
}

// -------------------------------------------------------------- metadata util
static bool PropToInt(const PROPVARIANT& v, long long& out) {
    switch (v.vt) {
        case VT_UI1: out = v.bVal; return true;
        case VT_UI2: out = v.uiVal; return true;
        case VT_UI4: out = v.ulVal; return true;
        case VT_UI8: out = (long long)v.uhVal.QuadPart; return true;
        case VT_I1: out = v.cVal; return true;
        case VT_I2: out = v.iVal; return true;
        case VT_I4: out = v.lVal; return true;
        case VT_I8: out = v.hVal.QuadPart; return true;
        case VT_BOOL: out = v.boolVal ? 1 : 0; return true;
        default: return false;
    }
}
static std::wstring PropToString(const PROPVARIANT& v) {
    if (v.vt == VT_LPWSTR && v.pwszVal) return v.pwszVal;
    if (v.vt == VT_LPSTR && v.pszVal) {
        std::string s = v.pszVal;
        int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
        std::wstring w(n, 0); MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], n);
        if (!w.empty() && w.back() == 0) w.pop_back();
        return w;
    }
    if (v.vt == VT_BSTR && v.bstrVal) return v.bstrVal;
    if (v.vt == VT_R8) return Fmt(L"%g", v.dblVal);
    if (v.vt == VT_R4) return Fmt(L"%g", double(v.fltVal));
    if (v.vt == VT_UI8) {  // WIC rational: low = numerator, high = denominator
        unsigned lo = v.uhVal.LowPart, hi = v.uhVal.HighPart;
        if (hi) return Fmt(L"%g", double(lo) / hi);
    }
    long long n;
    if (PropToInt(v, n)) return Fmt(L"%lld", n);
    return L"";
}
static bool QueryInt(IWICMetadataQueryReader* q, const wchar_t* name, long long& out) {
    if (!q) return false;
    PROPVARIANT v; PropVariantInit(&v);
    bool ok = SUCCEEDED(q->GetMetadataByName(name, &v)) && PropToInt(v, out);
    PropVariantClear(&v);
    return ok;
}
static std::wstring QueryStr(IWICMetadataQueryReader* q, const wchar_t* name) {
    if (!q) return L"";
    PROPVARIANT v; PropVariantInit(&v);
    std::wstring s;
    if (SUCCEEDED(q->GetMetadataByName(name, &v))) s = PropToString(v);
    PropVariantClear(&v);
    return Trim(s);
}

static int BppOf(const WICPixelFormatGUID& pf) {
    Com<IWICComponentInfo> ci;
    if (FAILED(Wic()->CreateComponentInfo(pf, ci.put()))) return 0;
    Com<IWICPixelFormatInfo> pi;
    if (FAILED(ci->QueryInterface(IID_PPV_ARGS(pi.put())))) return 0;
    UINT b = 0; pi->GetBitsPerPixel(&b);
    return int(b);
}

static ColorMode ModeOf(const WICPixelFormatGUID& pf) {
    if (IsEqualGUID(pf, GUID_WICPixelFormatBlackWhite)) return ColorMode::BW;
    if (IsEqualGUID(pf, GUID_WICPixelFormat2bppGray) || IsEqualGUID(pf, GUID_WICPixelFormat4bppGray) ||
        IsEqualGUID(pf, GUID_WICPixelFormat8bppGray) || IsEqualGUID(pf, GUID_WICPixelFormat16bppGray) ||
        IsEqualGUID(pf, GUID_WICPixelFormat16bppGrayFixedPoint) || IsEqualGUID(pf, GUID_WICPixelFormat32bppGrayFloat))
        return ColorMode::Gray;
    return ColorMode::Color;
}

static bool HasAlphaFormat(const WICPixelFormatGUID& pf) {
    static const GUID* fmts[] = {&GUID_WICPixelFormat32bppBGRA, &GUID_WICPixelFormat32bppPBGRA, &GUID_WICPixelFormat32bppRGBA,
                                 &GUID_WICPixelFormat32bppPRGBA, &GUID_WICPixelFormat64bppRGBA, &GUID_WICPixelFormat64bppPRGBA,
                                 &GUID_WICPixelFormat64bppBGRA, &GUID_WICPixelFormat128bppRGBAFloat, &GUID_WICPixelFormat64bppRGBAHalf,
                                 &GUID_WICPixelFormat40bppCMYKAlpha, &GUID_WICPixelFormat80bppCMYKAlpha};
    for (auto* g : fmts) if (IsEqualGUID(pf, *g)) return true;
    return false;
}

static bool FrameToImage(IWICBitmapSource* src, Image& out) {
    UINT w = 0, h = 0;
    if (FAILED(src->GetSize(&w, &h)) || !w || !h) return false;
    if (size_t(w) * h > size_t(1) << 31) return false;   // > 2 Gpx: refuse
    Com<IWICFormatConverter> conv;
    if (FAILED(Wic()->CreateFormatConverter(conv.put()))) return false;
    if (FAILED(conv->Initialize(src, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
        return false;
    try { out.create(int(w), int(h)); } catch (...) { return false; }
    // copy in strips to keep peak memory low for huge TIFFs
    const UINT strip = 512;
    for (UINT y = 0; y < h; y += strip) {
        WICRect rc = {0, INT(y), INT(w), INT(std::min(strip, h - y))};
        if (FAILED(conv->CopyPixels(&rc, w * 4, UINT(rc.Height) * w * 4, reinterpret_cast<BYTE*>(out.row(int(y)))))) return false;
    }
    return true;
}

// -------------------------------------------------------------- ImageSource
bool ImageSource::Init(std::wstring* err) {
    UINT n = 0;
    dec_->GetFrameCount(&n);
    pages_ = int(n);
    dec_->GetContainerFormat(&container_);
    Com<IWICBitmapDecoderInfo> di;
    if (SUCCEEDED(dec_->GetDecoderInfo(di.put()))) {
        UINT len = 0; di->GetFriendlyName(0, nullptr, &len);
        std::wstring s(len, 0); if (len) { di->GetFriendlyName(len, &s[0], &len); s.resize(len ? len - 1 : 0); }
        format_ = s;
    }
    animated_ = IsEqualGUID(container_, GUID_ContainerFormatGif) && pages_ > 1;
    if (pages_ <= 0 && err) *err = T("err.noframes");
    return pages_ > 0;
}

bool ImageSource::OpenFile(const std::wstring& path, std::wstring* err) {
    if (!Wic()) { if (err) *err = L"WIC unavailable"; return false; }
    HRESULT hr = Wic()->CreateDecoderFromFilename(LongPath(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, dec_.put());
    if (FAILED(hr)) {
        if (err) *err = hr == WINCODEC_ERR_COMPONENTNOTFOUND ? T("err.unsupported") : Fmt(L"%ls (0x%08X)", T("err.decode").c_str(), unsigned(hr));
        return false;
    }
    return Init(err);
}

bool ImageSource::OpenMemory(std::vector<uint8_t> data, std::wstring* err) {
    if (!Wic()) return false;
    mem_ = std::move(data);
    if (FAILED(Wic()->CreateStream(stream_.put())) || FAILED(stream_->InitializeFromMemory(mem_.data(), DWORD(mem_.size())))) return false;
    HRESULT hr = Wic()->CreateDecoderFromStream(stream_.get(), nullptr, WICDecodeMetadataCacheOnDemand, dec_.put());
    if (FAILED(hr)) {
        if (err) *err = hr == WINCODEC_ERR_COMPONENTNOTFOUND ? T("err.unsupported") : Fmt(L"%ls (0x%08X)", T("err.decode").c_str(), unsigned(hr));
        return false;
    }
    return Init(err);
}

bool ImageSource::LoadPage(int index, Image& out, int* orient) {
    if (!dec_ || index < 0 || index >= pages_) return false;
    Com<IWICBitmapFrameDecode> fr;
    if (FAILED(dec_->GetFrame(UINT(index), fr.put()))) return false;
    WICPixelFormatGUID pf = {};
    fr->GetPixelFormat(&pf);
    if (!FrameToImage(fr.get(), out)) return false;
    out.srcBpp = BppOf(pf);
    out.mode = ModeOf(pf);
    out.hasAlpha = HasAlphaFormat(pf) || (IsEqualGUID(container_, GUID_ContainerFormatGif) || IsEqualGUID(container_, GUID_ContainerFormatPng) ||
                                          IsEqualGUID(container_, GUID_ContainerFormatIco)) ;
    if (out.hasAlpha) out.hasAlpha = HasTransparency(out);
    double dx = 96, dy = 96;
    if (SUCCEEDED(fr->GetResolution(&dx, &dy)) && dx > 1 && dy > 1) { out.dpiX = dx; out.dpiY = dy; }
    if (orient) {
        *orient = 1;
        Com<IWICMetadataQueryReader> q;
        if (SUCCEEDED(fr->GetMetadataQueryReader(q.put()))) {
            long long o = 1;
            if (QueryInt(q.get(), L"/app1/ifd/{ushort=274}", o) || QueryInt(q.get(), L"/ifd/{ushort=274}", o) ||
                QueryInt(q.get(), L"System.Photo.Orientation", o))
                *orient = int(o);
        }
    }
    return true;
}

bool ImageSource::LoadAnimation(std::vector<Image>& frames, std::vector<int>& delays) {
    frames.clear(); delays.clear();
    if (!dec_) return false;
    UINT sw = 0, sh = 0;
    Com<IWICMetadataQueryReader> gq;
    if (SUCCEEDED(dec_->GetMetadataQueryReader(gq.put()))) {
        long long v;
        if (QueryInt(gq.get(), L"/logscrdesc/Width", v)) sw = UINT(v);
        if (QueryInt(gq.get(), L"/logscrdesc/Height", v)) sh = UINT(v);
    }
    Image canvas, prev;
    for (int i = 0; i < pages_; ++i) {
        Com<IWICBitmapFrameDecode> fr;
        if (FAILED(dec_->GetFrame(UINT(i), fr.put()))) break;
        Image f;
        if (!FrameToImage(fr.get(), f)) break;
        long long left = 0, top = 0, delay = 10, disp = 0;
        Com<IWICMetadataQueryReader> q;
        if (SUCCEEDED(fr->GetMetadataQueryReader(q.put()))) {
            QueryInt(q.get(), L"/imgdesc/Left", left);
            QueryInt(q.get(), L"/imgdesc/Top", top);
            QueryInt(q.get(), L"/grctlext/Delay", delay);
            QueryInt(q.get(), L"/grctlext/Disposal", disp);
        }
        if (i == 0) {
            if (!sw || !sh) { sw = UINT(f.w); sh = UINT(f.h); }
            canvas.create(int(sw), int(sh), 0);
        }
        if (disp == 3) prev = canvas;
        for (int y = 0; y < f.h; ++y) {
            int cy = int(top) + y; if (cy < 0 || cy >= canvas.h) continue;
            for (int x = 0; x < f.w; ++x) {
                int cx = int(left) + x; if (cx < 0 || cx >= canvas.w) continue;
                Pixel p = f.at(x, y);
                if (A(p)) canvas.at(cx, cy) = p;
            }
        }
        Image shot = canvas;
        shot.hasAlpha = HasTransparency(shot);
        shot.srcBpp = 8;
        frames.push_back(std::move(shot));
        delays.push_back(delay <= 1 ? 100 : int(delay) * 10);
        if (disp == 2) {
            for (int y = 0; y < f.h; ++y) {
                int cy = int(top) + y; if (cy < 0 || cy >= canvas.h) continue;
                for (int x = 0; x < f.w; ++x) {
                    int cx = int(left) + x; if (cx >= 0 && cx < canvas.w) canvas.at(cx, cy) = 0;
                }
            }
        } else if (disp == 3 && prev.valid()) {
            canvas = prev;
        }
    }
    return !frames.empty();
}

static std::wstring TiffCompressionName(long long c) {
    switch (c) {
        case 1: return L"None";
        case 2: return L"CCITT RLE";
        case 3: return L"CCITT Group 3";
        case 4: return L"CCITT Group 4";
        case 5: return L"LZW";
        case 6: return L"JPEG (old)";
        case 7: return L"JPEG";
        case 8: case 32946: return L"Deflate (ZIP)";
        case 32773: return L"PackBits";
        case 34712: return L"JPEG 2000";
        default: return Fmt(L"%lld", c);
    }
}

InfoList ImageSource::Properties(int page) {
    InfoList v;
    if (!dec_) return v;
    v.push_back({T("info.format"), format_});
    if (pages_ > 1) v.push_back({T("info.pages"), Fmt(L"%d", pages_)});
    Com<IWICBitmapFrameDecode> fr;
    if (FAILED(dec_->GetFrame(UINT(std::max(0, std::min(page, pages_ - 1))), fr.put()))) return v;
    UINT w = 0, h = 0; fr->GetSize(&w, &h);
    WICPixelFormatGUID pf = {}; fr->GetPixelFormat(&pf);
    v.push_back({T("info.size"), Fmt(L"%u x %u", w, h)});
    v.push_back({T("info.bpp"), Fmt(L"%d", BppOf(pf))});
    double dx = 0, dy = 0; fr->GetResolution(&dx, &dy);
    if (dx > 0) v.push_back({T("info.dpi"), Fmt(L"%.0f x %.0f", dx, dy)});
    Com<IWICMetadataQueryReader> q;
    if (SUCCEEDED(fr->GetMetadataQueryReader(q.put()))) {
        long long n;
        if (IsEqualGUID(container_, GUID_ContainerFormatTiff)) {
            if (QueryInt(q.get(), L"/ifd/{ushort=259}", n)) v.push_back({T("info.compression"), TiffCompressionName(n)});
            if (QueryInt(q.get(), L"/ifd/{ushort=262}", n)) {
                static const wchar_t* ph[] = {L"WhiteIsZero", L"BlackIsZero", L"RGB", L"Palette", L"Mask", L"CMYK", L"YCbCr"};
                v.push_back({T("info.photometric"), n >= 0 && n < 7 ? ph[n] : Fmt(L"%lld", n)});
            }
            std::wstring s = QueryStr(q.get(), L"/ifd/{ushort=305}");
            if (!s.empty()) v.push_back({T("info.software"), s});
            s = QueryStr(q.get(), L"/ifd/{ushort=306}");
            if (!s.empty()) v.push_back({T("info.datetime"), s});
            s = QueryStr(q.get(), L"/ifd/{ushort=270}");
            if (!s.empty()) v.push_back({T("info.description"), s});
        }
        if (IsEqualGUID(container_, GUID_ContainerFormatJpeg)) {
            std::wstring mk = QueryStr(q.get(), L"/app1/ifd/{ushort=271}"), md = QueryStr(q.get(), L"/app1/ifd/{ushort=272}");
            if (!mk.empty() || !md.empty()) v.push_back({T("info.camera"), Trim(mk + L" " + md)});
            std::wstring dt = QueryStr(q.get(), L"/app1/ifd/exif/{ushort=36867}");
            if (!dt.empty()) v.push_back({T("info.datetime"), dt});
            std::wstring sw = QueryStr(q.get(), L"/app1/ifd/{ushort=305}");
            if (!sw.empty()) v.push_back({T("info.software"), sw});
        }
        if (QueryInt(q.get(), L"/app1/ifd/{ushort=274}", n) || QueryInt(q.get(), L"/ifd/{ushort=274}", n))
            v.push_back({T("info.orientation"), Fmt(L"%lld", n)});
    }
    return v;
}

// --------------------------------------------------------------------- save
GUID ContainerForExt(const std::wstring& e0) {
    std::wstring e = ToLower(e0);
    if (e == L"jpg" || e == L"jpeg" || e == L"jpe" || e == L"jfif") return GUID_ContainerFormatJpeg;
    if (e == L"png") return GUID_ContainerFormatPng;
    if (e == L"bmp" || e == L"dib") return GUID_ContainerFormatBmp;
    if (e == L"gif") return GUID_ContainerFormatGif;
    if (e == L"tif" || e == L"tiff") return GUID_ContainerFormatTiff;
    if (e == L"wdp" || e == L"jxr" || e == L"hdp") return GUID_ContainerFormatWmp;
    return GUID_NULL;
}

static void SetProp(IPropertyBag2* bag, const wchar_t* name, VARIANT& v) {
    PROPBAG2 opt = {}; opt.pstrName = const_cast<LPOLESTR>(name);
    bag->Write(1, &opt, &v);
}

// Pack our 32-bit image into the exact pixel layout the encoder will receive.
static std::vector<BYTE> PackPixels(const Image& im, const WICPixelFormatGUID& pf, UINT& stride, std::vector<WICColor>* palette) {
    std::vector<BYTE> buf;
    int w = im.w, h = im.h;
    auto luma = [](Pixel p) { return BYTE((R(p) * 299 + G(p) * 587 + B(p) * 114 + 500) / 1000); };
    if (IsEqualGUID(pf, GUID_WICPixelFormatBlackWhite)) {          // 1 = white, MSB first
        stride = UINT((w + 31) / 32 * 4);
        buf.assign(size_t(stride) * h, 0);
        for (int y = 0; y < h; ++y) {
            const Pixel* s = im.row(y); BYTE* d = &buf[size_t(y) * stride];
            for (int x = 0; x < w; ++x) if (luma(s[x]) >= 128) d[x >> 3] |= BYTE(0x80 >> (x & 7));
        }
    } else if (IsEqualGUID(pf, GUID_WICPixelFormat8bppGray)) {
        stride = UINT((w + 3) / 4 * 4);
        buf.assign(size_t(stride) * h, 0);
        for (int y = 0; y < h; ++y) { const Pixel* s = im.row(y); BYTE* d = &buf[size_t(y) * stride]; for (int x = 0; x < w; ++x) d[x] = luma(s[x]); }
    } else if (IsEqualGUID(pf, GUID_WICPixelFormat24bppBGR)) {
        stride = UINT((w * 3 + 3) / 4 * 4);
        buf.assign(size_t(stride) * h, 0);
        for (int y = 0; y < h; ++y) {
            const Pixel* s = im.row(y); BYTE* d = &buf[size_t(y) * stride];
            for (int x = 0; x < w; ++x) { d[x * 3] = B(s[x]); d[x * 3 + 1] = G(s[x]); d[x * 3 + 2] = R(s[x]); }
        }
    } else if (IsEqualGUID(pf, GUID_WICPixelFormat8bppIndexed)) {
        // fixed 6x7x6 colour cube (252 entries) + one transparent entry, with error diffusion
        stride = UINT((w + 3) / 4 * 4);
        buf.assign(size_t(stride) * h, 0);
        palette->clear();
        for (int r = 0; r < 6; ++r) for (int g = 0; g < 7; ++g) for (int b = 0; b < 6; ++b)
            palette->push_back(0xFF000000u | (UINT(r * 51) << 16) | (UINT(g * 255 / 6) << 8) | UINT(b * 51));
        palette->push_back(0x00000000u);   // 252 = transparent
        std::vector<float> err(size_t(w + 2) * 3 * 2, 0.f);
        for (int y = 0; y < h; ++y) {
            float* cur = &err[size_t(y & 1) * (w + 2) * 3];
            float* nxt = &err[size_t((y + 1) & 1) * (w + 2) * 3];
            std::fill(nxt, nxt + (w + 2) * 3, 0.f);
            const Pixel* s = im.row(y); BYTE* d = &buf[size_t(y) * stride];
            for (int x = 0; x < w; ++x) {
                if (A(s[x]) < 128) { d[x] = 252; continue; }
                float v[3] = {R(s[x]) + cur[(x + 1) * 3], G(s[x]) + cur[(x + 1) * 3 + 1], B(s[x]) + cur[(x + 1) * 3 + 2]};
                int q[3]; int lv[3] = {5, 6, 5};
                for (int c = 0; c < 3; ++c) {
                    float vc = std::max(0.f, std::min(255.f, v[c]));
                    q[c] = int(vc * lv[c] / 255.f + 0.5f);
                    float e = vc - q[c] * 255.f / lv[c];
                    cur[(x + 2) * 3 + c] += e * 7 / 16; nxt[x * 3 + c] += e * 3 / 16;
                    nxt[(x + 1) * 3 + c] += e * 5 / 16; nxt[(x + 2) * 3 + c] += e / 16;
                }
                d[x] = BYTE(q[0] * 42 + q[1] * 6 + q[2]);
            }
        }
    } else {   // 32bppBGRA
        stride = UINT(w * 4);
        buf.resize(size_t(stride) * h);
        std::memcpy(buf.data(), im.px.data(), buf.size());
    }
    return buf;
}

bool SaveImage(const Image& im, const std::wstring& path, const SaveOptions& o, std::wstring* err) {
    std::wstring tmp = path + L".lvtmp";
    Com<IWICStream> st;
    Com<IWICBitmapEncoder> enc;
    Com<IWICBitmapFrameEncode> fe;
    auto fail = [&](const wchar_t* what, HRESULT hr) {
        if (err) *err = Fmt(L"%ls (0x%08X)", what, unsigned(hr));
        fe.reset(); enc.reset(); st.reset();
        DeleteFileW(LongPath(tmp).c_str());
        return false;
    };
    GUID cf = ContainerForExt(PathExt(path));
    if (IsEqualGUID(cf, GUID_NULL)) { if (err) *err = T("err.saveformat"); return false; }
    if (!im.valid()) return false;
    HRESULT hr;
    bool alpha = im.hasAlpha && HasTransparency(im);
    bool isTiff = IsEqualGUID(cf, GUID_ContainerFormatTiff), isJpeg = IsEqualGUID(cf, GUID_ContainerFormatJpeg);
    bool isPng = IsEqualGUID(cf, GUID_ContainerFormatPng), isGif = IsEqualGUID(cf, GUID_ContainerFormatGif);
    bool isBmp = IsEqualGUID(cf, GUID_ContainerFormatBmp);
    bool bw = im.mode == ColorMode::BW && !alpha && IsBlackWhite(im);
    bool gray = !alpha && (bw || im.mode == ColorMode::Gray) && IsGrayscale(im);

    WICPixelFormatGUID target = GUID_WICPixelFormat24bppBGR;
    if (alpha && (isPng || isTiff || isBmp || IsEqualGUID(cf, GUID_ContainerFormatWmp))) target = GUID_WICPixelFormat32bppBGRA;
    else if (bw && (isTiff || isPng || isBmp)) target = GUID_WICPixelFormatBlackWhite;
    else if (gray && (isTiff || isPng || isJpeg)) target = GUID_WICPixelFormat8bppGray;
    if (isGif) target = GUID_WICPixelFormat8bppIndexed;
    int tc = o.tiffCompression;
    if (isTiff && tc == 4 && !IsEqualGUID(target, GUID_WICPixelFormatBlackWhite)) tc = 2;  // CCITT needs 1bpp

    // write to a temp file first, then replace (safe overwrite)
    if (FAILED(hr = Wic()->CreateStream(st.put()))) return fail(L"CreateStream", hr);
    if (FAILED(hr = st->InitializeFromFilename(LongPath(tmp).c_str(), GENERIC_WRITE))) return fail(T("err.write").c_str(), hr);
    if (FAILED(hr = Wic()->CreateEncoder(cf, nullptr, enc.put()))) return fail(L"CreateEncoder", hr);
    if (FAILED(hr = enc->Initialize(st.get(), WICBitmapEncoderNoCache))) return fail(L"Encoder", hr);
    Com<IPropertyBag2> bag;
    if (FAILED(hr = enc->CreateNewFrame(fe.put(), bag.put()))) return fail(L"Frame", hr);
    if (bag) {
        VARIANT v; VariantInit(&v);
        if (isJpeg) { v.vt = VT_R4; v.fltVal = float(std::max(1, std::min(100, o.jpegQuality))) / 100.f; SetProp(bag.get(), L"ImageQuality", v); }
        if (isTiff) {
            static const BYTE map[] = {WICTiffCompressionDontCare, WICTiffCompressionNone, WICTiffCompressionLZW,
                                       WICTiffCompressionZIP, WICTiffCompressionCCITT4, WICTiffCompressionRLE};
            BYTE m = map[std::max(0, std::min(5, tc))];
            if (tc == 0) m = IsEqualGUID(target, GUID_WICPixelFormatBlackWhite) ? BYTE(WICTiffCompressionCCITT4) : BYTE(WICTiffCompressionLZW);
            v.vt = VT_UI1; v.bVal = m; SetProp(bag.get(), L"TiffCompressionMethod", v);
        }
        if (isPng && o.interlace) { v.vt = VT_BOOL; v.boolVal = VARIANT_TRUE; SetProp(bag.get(), L"InterlaceOption", v); }
    }
    if (FAILED(hr = fe->Initialize(bag.get()))) return fail(L"FrameInit", hr);
    if (FAILED(hr = fe->SetSize(UINT(im.w), UINT(im.h)))) return fail(L"SetSize", hr);
    fe->SetResolution(im.dpiX, im.dpiY);
    WICPixelFormatGUID pf = target;
    if (FAILED(hr = fe->SetPixelFormat(&pf))) return fail(L"SetPixelFormat", hr);
    if (!IsEqualGUID(pf, target)) {
        // encoder picked another layout: fall back to the most common one
        target = alpha ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;
        pf = target;
        fe->SetPixelFormat(&pf);
        if (!IsEqualGUID(pf, target)) { target = pf; }
    }
    UINT stride = 0;
    std::vector<WICColor> palColors;
    std::vector<BYTE> buf = PackPixels(im, target, stride, &palColors);
    if (IsEqualGUID(target, GUID_WICPixelFormat8bppIndexed)) {
        Com<IWICPalette> pal;
        Wic()->CreatePalette(pal.put());
        pal->InitializeCustom(palColors.data(), UINT(palColors.size()));
        fe->SetPalette(pal.get());
    }
    if (buf.empty()) return fail(L"Pack", E_FAIL);
    if (FAILED(hr = fe->WritePixels(UINT(im.h), stride, UINT(buf.size()), buf.data()))) return fail(L"WritePixels", hr);
    if (FAILED(hr = fe->Commit())) return fail(L"Commit", hr);
    if (FAILED(hr = enc->Commit())) return fail(L"Commit", hr);
    fe.reset(); enc.reset(); st.reset();
    if (!MoveFileExW(LongPath(tmp).c_str(), LongPath(path).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        DeleteFileW(LongPath(tmp).c_str());
        if (err) *err = Fmt(L"%ls (%lu)", T("err.write").c_str(), GetLastError());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- clipboard
HBITMAP CreateDib32(int w, int h, void** bits) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, bits, nullptr, 0);
}

static HGLOBAL MakeDib(const Image& im, bool v5) {
    size_t hdr = v5 ? sizeof(BITMAPV5HEADER) : sizeof(BITMAPINFOHEADER);
    int bpp = v5 ? 32 : 24;
    size_t stride = ((size_t(im.w) * bpp + 31) / 32) * 4;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, hdr + stride * im.h);
    if (!g) return nullptr;
    BYTE* p = static_cast<BYTE*>(GlobalLock(g));
    std::memset(p, 0, hdr);
    if (v5) {
        auto* h = reinterpret_cast<BITMAPV5HEADER*>(p);
        h->bV5Size = sizeof(*h); h->bV5Width = im.w; h->bV5Height = im.h; h->bV5Planes = 1; h->bV5BitCount = 32;
        h->bV5Compression = BI_BITFIELDS; h->bV5RedMask = 0x00FF0000; h->bV5GreenMask = 0x0000FF00; h->bV5BlueMask = 0x000000FF;
        h->bV5AlphaMask = 0xFF000000; h->bV5CSType = LCS_sRGB; h->bV5Intent = LCS_GM_IMAGES;
        h->bV5SizeImage = DWORD(stride * im.h);
    } else {
        auto* h = reinterpret_cast<BITMAPINFOHEADER*>(p);
        h->biSize = sizeof(*h); h->biWidth = im.w; h->biHeight = im.h; h->biPlanes = 1; h->biBitCount = 24; h->biCompression = BI_RGB;
        h->biSizeImage = DWORD(stride * im.h);
    }
    BYTE* d = p + hdr;
    for (int y = 0; y < im.h; ++y) {
        const Pixel* s = im.row(im.h - 1 - y);
        BYTE* r = d + stride * y;
        if (v5) std::memcpy(r, s, size_t(im.w) * 4);
        else for (int x = 0; x < im.w; ++x) { r[x * 3] = B(s[x]); r[x * 3 + 1] = G(s[x]); r[x * 3 + 2] = R(s[x]); }
    }
    GlobalUnlock(g);
    return g;
}

bool CopyImageToClipboard(HWND owner, const Image& im) {
    if (!im.valid() || !OpenClipboard(owner)) return false;
    EmptyClipboard();
    if (HGLOBAL g = MakeDib(im, false)) SetClipboardData(CF_DIB, g);
    if (im.hasAlpha) {
        if (HGLOBAL g = MakeDib(im, true)) SetClipboardData(CF_DIBV5, g);
    }
    CloseClipboard();
    return true;
}

static bool DibToImage(const BYTE* p, size_t size, Image& out) {
    if (size < sizeof(BITMAPINFOHEADER)) return false;
    const auto* h = reinterpret_cast<const BITMAPINFOHEADER*>(p);
    int w = h->biWidth, hh = std::abs(h->biHeight), bpp = h->biBitCount;
    bool topDown = h->biHeight < 0;
    if (w <= 0 || hh <= 0) return false;
    if (bpp != 24 && bpp != 32 && bpp != 8 && bpp != 1 && bpp != 4 && bpp != 16) return false;
    size_t off = h->biSize;
    DWORD masks[3] = {0x00FF0000, 0x0000FF00, 0x000000FF};
    if (h->biCompression == BI_BITFIELDS && h->biSize == sizeof(BITMAPINFOHEADER)) {
        std::memcpy(masks, p + off, 12); off += 12;
    } else if (h->biCompression == BI_BITFIELDS && h->biSize >= sizeof(BITMAPV4HEADER)) {
        const auto* v4 = reinterpret_cast<const BITMAPV4HEADER*>(p);
        masks[0] = v4->bV4RedMask; masks[1] = v4->bV4GreenMask; masks[2] = v4->bV4BlueMask;
    }
    int ncol = 0;
    if (bpp <= 8) ncol = h->biClrUsed ? int(h->biClrUsed) : (1 << bpp);
    const RGBQUAD* pal = reinterpret_cast<const RGBQUAD*>(p + off);
    off += size_t(ncol) * 4;
    size_t stride = ((size_t(w) * bpp + 31) / 32) * 4;
    if (off + stride * hh > size) return false;
    const BYTE* bits = p + off;
    out.create(w, hh);
    bool anyAlpha = false;
    auto shiftOf = [](DWORD m) { int s = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; ++s; } return s; };
    auto widthOf = [](DWORD m) { int n = 0; while (m) { n += m & 1; m >>= 1; } return n; };
    for (int y = 0; y < hh; ++y) {
        const BYTE* r = bits + stride * (topDown ? y : hh - 1 - y);
        Pixel* d = out.row(y);
        for (int x = 0; x < w; ++x) {
            switch (bpp) {
                case 32: {
                    DWORD v = reinterpret_cast<const DWORD*>(r)[x];
                    int a = int(v >> 24);
                    if (a) anyAlpha = true;
                    auto ch = [&](int i) { int s = shiftOf(masks[i]), n = widthOf(masks[i]); int c = int((v & masks[i]) >> s); return n >= 8 ? c >> (n - 8) : (n ? c * 255 / ((1 << n) - 1) : 0); };
                    d[x] = ARGB(a, ch(0), ch(1), ch(2));
                } break;
                case 24: d[x] = ARGB(255, r[x * 3 + 2], r[x * 3 + 1], r[x * 3]); break;
                case 16: {
                    WORD v = reinterpret_cast<const WORD*>(r)[x];
                    DWORD m[3] = {masks[0], masks[1], masks[2]};
                    if (h->biCompression != BI_BITFIELDS) { m[0] = 0x7C00; m[1] = 0x03E0; m[2] = 0x001F; }
                    auto ch = [&](int i) { int s = shiftOf(m[i]), n = widthOf(m[i]); int c = int((v & m[i]) >> s); return n ? c * 255 / ((1 << n) - 1) : 0; };
                    d[x] = ARGB(255, ch(0), ch(1), ch(2));
                } break;
                default: {
                    int idx;
                    if (bpp == 8) idx = r[x];
                    else if (bpp == 4) idx = (r[x / 2] >> ((1 - x % 2) * 4)) & 15;
                    else idx = (r[x / 8] >> (7 - x % 8)) & 1;
                    RGBQUAD q = idx < ncol ? pal[idx] : RGBQUAD{0, 0, 0, 0};
                    d[x] = ARGB(255, q.rgbRed, q.rgbGreen, q.rgbBlue);
                }
            }
        }
    }
    if (bpp == 32) {
        if (!anyAlpha) for (auto& px : out.px) px |= 0xFF000000;
        out.hasAlpha = anyAlpha && HasTransparency(out);
    }
    out.srcBpp = bpp;
    return true;
}

bool ClipboardHasImage() {
    return IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5) || IsClipboardFormatAvailable(CF_BITMAP);
}

bool PasteImageFromClipboard(HWND owner, Image& out) {
    if (!OpenClipboard(owner)) return false;
    bool ok = false;
    UINT fmts[] = {CF_DIBV5, CF_DIB};
    for (UINT f : fmts) {
        HANDLE hd = GetClipboardData(f);
        if (!hd) continue;
        const BYTE* p = static_cast<const BYTE*>(GlobalLock(hd));
        if (p) { ok = DibToImage(p, GlobalSize(hd), out); GlobalUnlock(hd); }
        if (ok) break;
    }
    CloseClipboard();
    return ok;
}

bool CopyTextToClipboard(HWND owner, const std::wstring& s) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t));
    if (g) {
        std::memcpy(GlobalLock(g), s.c_str(), (s.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(g);
        SetClipboardData(CF_UNICODETEXT, g);
    }
    CloseClipboard();
    return g != nullptr;
}

} // namespace lv
