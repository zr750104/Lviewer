// Lviewer - self update from GitHub Releases
#include "Updater.h"
#include "miniz.h"
#include <winhttp.h>
#include <cstdio>

namespace lv {

const wchar_t* kUpdateRepo = L"zr750104/Lviewer";

namespace {

struct Handle {
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET x = nullptr) : h(x) {}
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    operator HINTERNET() const { return h; }
};

HINTERNET OpenSession() {
    const wchar_t* ua = L"Lviewer-updater/" LV_VERSION_STR;
    // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY (Windows 8.1+) follows the system / PAC proxy settings
    HINTERNET s = WinHttpOpen(ua, 4 /*WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY*/, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(ua, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (s) WinHttpSetTimeouts(s, 10000, 10000, 15000, 60000);
    return s;
}

// GET url; calls sink for each chunk. Follows redirects (WinHTTP default policy).
bool HttpGet(const std::wstring& url, const std::function<bool(const char*, DWORD)>& sink, std::wstring* err, const wchar_t* accept) {
    URL_COMPONENTS uc = {sizeof(uc)};
    wchar_t host[256] = {}, path[4096] = {};
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 4096;
    wchar_t extra[2048] = {};
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { if (err) *err = L"Bad URL"; return false; }
    Handle ses(OpenSession());
    if (!ses) { if (err) *err = Fmt(L"WinHttpOpen (%lu)", GetLastError()); return false; }
    Handle con(WinHttpConnect(ses, host, uc.nPort, 0));
    if (!con) { if (err) *err = Fmt(L"Connect (%lu)", GetLastError()); return false; }
    std::wstring obj = std::wstring(path) + extra;
    Handle req(WinHttpOpenRequest(con, L"GET", obj.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
    if (!req) { if (err) *err = Fmt(L"OpenRequest (%lu)", GetLastError()); return false; }
    std::wstring hdr = std::wstring(L"Accept: ") + (accept ? accept : L"*/*") + L"\r\n";
    if (!WinHttpSendRequest(req, hdr.c_str(), DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(req, nullptr)) {
        if (err) *err = Fmt(L"Network error (%lu)", GetLastError());
        return false;
    }
    DWORD status = 0, sz = sizeof(status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) { if (err) *err = Fmt(L"HTTP %lu", status); return false; }
    std::vector<char> buf(64 * 1024);
    for (;;) {
        DWORD rd = 0;
        if (!WinHttpReadData(req, buf.data(), DWORD(buf.size()), &rd)) { if (err) *err = Fmt(L"Read (%lu)", GetLastError()); return false; }
        if (!rd) break;
        if (!sink(buf.data(), rd)) { if (err) *err = L"Write failed"; return false; }
    }
    return true;
}

// Returns the JSON string value of the first "key" at or after pos (handles common escapes).
std::string JsonString(const std::string& js, const std::string& key, size_t from = 0, size_t* endPos = nullptr) {
    std::string pat = "\"" + key + "\"";
    size_t k = js.find(pat, from);
    if (k == std::string::npos) return {};
    size_t c = js.find(':', k + pat.size());
    if (c == std::string::npos) return {};
    size_t q = js.find_first_not_of(" \t\r\n", c + 1);
    if (q == std::string::npos || js[q] != '"') return {};
    std::string out;
    size_t i = q + 1;
    for (; i < js.size() && js[i] != '"'; ++i) {
        if (js[i] != '\\' || i + 1 >= js.size()) { out += js[i]; continue; }
        char e = js[++i];
        switch (e) {
            case 'n': out += '\n'; break;
            case 'r': break;
            case 't': out += '\t'; break;
            case 'u': {
                if (i + 4 >= js.size()) break;
                unsigned cp = std::stoul(js.substr(i + 1, 4), nullptr, 16);
                i += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < js.size() && js[i + 1] == '\\' && js[i + 2] == 'u') {
                    unsigned lo = std::stoul(js.substr(i + 3, 4), nullptr, 16);
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                }
                if (cp < 0x80) out += char(cp);
                else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 63)); }
                else if (cp < 0x10000) { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
                else { out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 63)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
            } break;
            default: out += e; break;
        }
    }
    if (endPos) *endPos = i;
    return out;
}

bool WriteFileAll(const std::wstring& path, const void* data, size_t n) {
    HANDLE h = CreateFileW(LongPath(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    bool ok = WriteFile(h, data, DWORD(n), &wr, nullptr) && wr == n;
    CloseHandle(h);
    return ok;
}

} // namespace

int TodayNumber() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    return int(u.QuadPart / 864000000000ULL);
}

int CompareVersions(const std::wstring& a0, const std::wstring& b0) {
    auto parts = [](std::wstring s) {
        if (!s.empty() && (s[0] == L'v' || s[0] == L'V')) s = s.substr(1);
        std::vector<int> v;
        for (auto& p : Split(s, L'.')) v.push_back(_wtoi(p.c_str()));
        while (v.size() < 3) v.push_back(0);
        return v;
    };
    auto a = parts(a0), b = parts(b0);
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

bool FetchLatestRelease(ReleaseInfo& out, std::wstring* err) {
    std::string js;
    std::wstring url = std::wstring(L"https://api.github.com/repos/") + kUpdateRepo + L"/releases/latest";
    if (!HttpGet(url, [&](const char* p, DWORD n) { js.append(p, n); return js.size() < 4 * 1024 * 1024; }, err, L"application/vnd.github+json"))
        return false;
    out.tag = Utf8ToW(JsonString(js, "tag_name"));
    if (out.tag.empty()) { if (err) *err = L"No release information"; return false; }
    out.pageUrl = std::wstring(L"https://github.com/") + kUpdateRepo + L"/releases/tag/" + out.tag;
    out.notes = Utf8ToW(JsonString(js, "body"));
    size_t pos = 0;
    for (;;) {
        size_t end = 0;
        std::string u = JsonString(js, "browser_download_url", pos, &end);
        if (u.empty()) break;
        if (u.size() > 10 && u.compare(u.size() - 10, 10, "-win64.zip") == 0) { out.zipUrl = Utf8ToW(u); break; }
        pos = end;
    }
    if (out.zipUrl.empty()) { if (err) *err = L"The release has no -win64.zip file"; return false; }
    return true;
}

bool DownloadToFile(const std::wstring& url, const std::wstring& path, std::wstring* err) {
    FILE* f = _wfopen(LongPath(path).c_str(), L"wb");
    if (!f) { if (err) *err = L"Cannot create " + path; return false; }
    bool ok = HttpGet(url, [&](const char* p, DWORD n) { return fwrite(p, 1, n, f) == n; }, err, L"application/octet-stream");
    fclose(f);
    if (!ok) DeleteFileW(LongPath(path).c_str());
    return ok;
}

bool InstallUpdate(const std::wstring& zipPath, std::wstring* err) {
    auto fail = [&](const std::wstring& m) { if (err) *err = m; return false; };
    FILE* f = _wfopen(LongPath(zipPath).c_str(), L"rb");
    if (!f) return fail(L"Cannot open the downloaded file");
    _fseeki64(f, 0, SEEK_END);
    long long size = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    mz_zip_archive z = {};
    if (!mz_zip_reader_init_cfile(&z, f, mz_uint64(size), 0)) { fclose(f); return fail(L"The downloaded file is not a valid ZIP"); }

    std::wstring exe = ExePath(), dir = ExeDir();
    std::wstring newExe = exe + L".new", oldExe = exe + L".old";
    bool gotExe = false;
    std::vector<std::pair<std::wstring, std::vector<char>>> others;
    mz_uint n = mz_zip_reader_get_num_files(&z);
    for (mz_uint i = 0; i < n; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st) || st.m_is_directory) continue;
        std::wstring name = Utf8ToW(st.m_filename);
        for (auto& c : name) if (c == L'/') c = L'\\';
        // strip the top "Lviewer\" folder
        size_t k = name.find(L'\\');
        std::wstring rel = k == std::wstring::npos ? name : name.substr(k + 1);
        if (rel.empty() || rel.find(L"..") != std::wstring::npos) continue;
        size_t sz = 0;
        void* p = mz_zip_reader_extract_to_heap(&z, i, &sz, 0);
        if (!p) continue;
        if (lstrcmpiW(PathExt(rel).c_str(), L"exe") == 0 && rel.find(L'\\') == std::wstring::npos) {
            gotExe = WriteFileAll(newExe, p, sz);
        } else {
            others.push_back({rel, std::vector<char>(static_cast<char*>(p), static_cast<char*>(p) + sz)});
        }
        mz_free(p);
    }
    mz_zip_reader_end(&z);
    fclose(f);
    if (!gotExe) { DeleteFileW(LongPath(newExe).c_str()); return fail(L"Lviewer.exe was not found in the update, or the folder is not writable"); }

    // swap the running exe: Windows allows renaming a running executable
    DeleteFileW(LongPath(oldExe).c_str());
    if (!MoveFileExW(LongPath(exe).c_str(), LongPath(oldExe).c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(LongPath(newExe).c_str());
        return fail(Fmt(L"Cannot replace Lviewer.exe (%lu)", GetLastError()));
    }
    if (!MoveFileExW(LongPath(newExe).c_str(), LongPath(exe).c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DWORD e = GetLastError();
        MoveFileExW(LongPath(oldExe).c_str(), LongPath(exe).c_str(), MOVEFILE_REPLACE_EXISTING);   // roll back
        return fail(Fmt(L"Cannot install the new Lviewer.exe (%lu)", e));
    }
    for (auto& o : others) {
        std::wstring target = PathJoin(dir, o.first);
        SHCreateDirectoryExW(nullptr, PathDir(target).c_str(), nullptr);
        WriteFileAll(target, o.second.data(), o.second.size());
    }
    return true;
}

void CleanupAfterUpdate() {
    std::wstring old = ExePath() + L".old";
    if (FileExists(old)) DeleteFileW(LongPath(old).c_str());
}

} // namespace lv
