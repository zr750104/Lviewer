#pragma once
#include "Win.h"

namespace lv {

IWICImagingFactory* Wic();

struct CodecDesc { std::wstring name; std::vector<std::wstring> exts; bool canWrite = false; };
std::vector<CodecDesc> ListCodecs();                 // installed WIC decoders/encoders
const std::vector<std::wstring>& ImageExtensions(); // lower-case, without dot
bool IsImageExt(const std::wstring& ext);

using InfoList = std::vector<std::pair<std::wstring, std::wstring>>;

// An opened image file (keeps the WIC decoder so pages can be fetched lazily).
class ImageSource {
public:
    bool OpenFile(const std::wstring& path, std::wstring* err = nullptr);
    bool OpenMemory(std::vector<uint8_t> data, std::wstring* err = nullptr);
    int PageCount() const { return pages_; }
    bool IsAnimated() const { return animated_; }
    std::wstring FormatName() const { return format_; }
    GUID Container() const { return container_; }
    // Decode one page (TIFF page / ICO entry / first GIF frame).
    bool LoadPage(int index, Image& out, int* exifOrientation = nullptr);
    // Composite all GIF (or other animated) frames; delays in ms.
    bool LoadAnimation(std::vector<Image>& frames, std::vector<int>& delays);
    InfoList Properties(int page);
private:
    bool Init(std::wstring* err);
    Com<IWICBitmapDecoder> dec_;
    Com<IWICStream> stream_;
    std::vector<uint8_t> mem_;
    int pages_ = 0;
    bool animated_ = false;
    std::wstring format_;
    GUID container_ = {};
};

struct SaveOptions {
    int jpegQuality = 90;
    int tiffCompression = 0;   // 0 auto, 1 none, 2 LZW, 3 ZIP, 4 CCITT G4, 5 PackBits
    bool interlace = false;
};
bool SaveImage(const Image& im, const std::wstring& path, const SaveOptions& opt, std::wstring* err);
GUID ContainerForExt(const std::wstring& ext);

// Clipboard
bool CopyImageToClipboard(HWND owner, const Image& im);
bool PasteImageFromClipboard(HWND owner, Image& out);
bool ClipboardHasImage();
bool CopyTextToClipboard(HWND owner, const std::wstring& s);

// DIB helper for printing / wallpaper
HBITMAP CreateDib32(int w, int h, void** bits);

} // namespace lv
