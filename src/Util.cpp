#include "Win.h"
#include <cstdarg>
#include <cwctype>

namespace lv {

std::wstring Utf8ToW(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), &w[0], n);
    return w;
}
std::string WToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring Fmt(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf(buf, 2047, fmt, ap);
    va_end(ap);
    buf[2047] = 0;
    return buf;
}
std::wstring ExePath() {
    std::wstring s(32768, 0);
    DWORD n = GetModuleFileNameW(nullptr, &s[0], DWORD(s.size()));
    s.resize(n);
    return s;
}
std::wstring ExeDir() { return PathDir(ExePath()); }
std::wstring PathFileName(const std::wstring& p) {
    size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? p : p.substr(k + 1);
}
std::wstring PathDir(const std::wstring& p) {
    size_t k = p.find_last_of(L"\\/");
    if (k == std::wstring::npos) return L"";
    if (k == 2 && p.size() > 1 && p[1] == L':') return p.substr(0, 3);
    return p.substr(0, k);
}
std::wstring PathExt(const std::wstring& p) {
    std::wstring f = PathFileName(p);
    size_t k = f.find_last_of(L'.');
    return k == std::wstring::npos ? L"" : ToLower(f.substr(k + 1));
}
std::wstring PathJoin(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}
std::wstring LongPath(const std::wstring& p) {
    if (p.size() < MAX_PATH - 12 || p.compare(0, 4, L"\\\\?\\") == 0) return p;
    if (p.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    return L"\\\\?\\" + p;
}
bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(LongPath(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
bool DirExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(LongPath(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
std::wstring ToLower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], DWORD(s.size()));
    return s;
}
std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> out;
    size_t a = 0;
    for (;;) {
        size_t b = s.find(sep, a);
        out.push_back(s.substr(a, b == std::wstring::npos ? std::wstring::npos : b - a));
        if (b == std::wstring::npos) break;
        a = b + 1;
    }
    return out;
}
std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n"), b = s.find_last_not_of(L" \t\r\n");
    return a == std::wstring::npos ? L"" : s.substr(a, b - a + 1);
}
std::wstring FormatBytes(unsigned long long n) {
    if (n < 1024) return Fmt(L"%llu B", n);
    if (n < 1024ull * 1024) return Fmt(L"%.1f KB", n / 1024.0);
    if (n < 1024ull * 1024 * 1024) return Fmt(L"%.2f MB", n / 1048576.0);
    return Fmt(L"%.2f GB", n / 1073741824.0);
}
std::wstring FormatFileTime(const FILETIME& ft) {
    FILETIME lt; SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &lt);
    FileTimeToSystemTime(&lt, &st);
    wchar_t d[64] = {}, t[64] = {};
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, d, 64, nullptr);
    GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, nullptr, t, 64);
    return std::wstring(d) + L" " + t;
}
int DpiOf(HWND h) {
    typedef UINT(WINAPI * Fn)(HWND);
    static Fn fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn && h) return int(fn(h));
    HDC dc = GetDC(nullptr);
    int d = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return d;
}

} // namespace lv
