// Lviewer - common Windows includes and helpers
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "core/Image.h"

#ifndef LV_VERSION_STR
#define LV_VERSION_STR L"1.0.0"
#endif

namespace lv {

// Minimal COM smart pointer (avoids WRL dependency for MinGW/MSVC portability)
template <class T> class Com {
    T* p_ = nullptr;
public:
    Com() = default;
    Com(T* p) : p_(p) {}
    Com(const Com& o) : p_(o.p_) { if (p_) p_->AddRef(); }
    Com(Com&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    ~Com() { reset(); }
    Com& operator=(Com o) { std::swap(p_, o.p_); return *this; }
    void reset() { if (p_) { p_->Release(); p_ = nullptr; } }
    T* get() const { return p_; }
    T* operator->() const { return p_; }
    T** put() { reset(); return &p_; }
    explicit operator bool() const { return p_ != nullptr; }
};

std::wstring Utf8ToW(const std::string& s);
std::string WToUtf8(const std::wstring& s);
std::wstring Fmt(const wchar_t* fmt, ...);
std::wstring ExeDir();
std::wstring ExePath();
std::wstring PathFileName(const std::wstring& p);
std::wstring PathDir(const std::wstring& p);
std::wstring PathExt(const std::wstring& p);          // lower-case, without dot
std::wstring PathJoin(const std::wstring& a, const std::wstring& b);
bool FileExists(const std::wstring& p);
bool DirExists(const std::wstring& p);
std::wstring ToLower(std::wstring s);
std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep);
std::wstring Trim(const std::wstring& s);
std::wstring FormatBytes(unsigned long long n);
std::wstring FormatFileTime(const FILETIME& ft);
// Extend to \\?\ form for long paths when needed
std::wstring LongPath(const std::wstring& p);

// Window/DPI helpers
int DpiOf(HWND h);
inline int Scale(int v, int dpi) { return MulDiv(v, dpi, 96); }

} // namespace lv
