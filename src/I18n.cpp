#include "I18n.h"
#include "core/LangFile.h"
#include <map>

namespace lv {

const LangInfo kLanguages[3] = {{L"ja", L"日本語"}, {L"en", L"English"}, {L"th", L"ภาษาไทย"}};

static std::map<std::string, std::wstring> g_tab, g_fallback;
static std::wstring g_lang = L"en";

static std::string LoadResourceText(const wchar_t* name) {
    HRSRC r = FindResourceW(nullptr, name, MAKEINTRESOURCEW(10) /*RT_RCDATA*/);
    if (!r) return {};
    HGLOBAL g = LoadResource(nullptr, r);
    DWORD n = SizeofResource(nullptr, r);
    const char* p = static_cast<const char*>(LockResource(g));
    return p ? std::string(p, n) : std::string();
}

static std::string LoadFileText(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    DWORD sz = GetFileSize(h, nullptr), rd = 0;
    std::string s(sz, 0);
    if (sz) ReadFile(h, &s[0], sz, &rd, nullptr);
    CloseHandle(h);
    s.resize(rd);
    return s;
}

static void Fill(std::map<std::string, std::wstring>& tab, const std::wstring& code) {
    tab.clear();
    std::wstring res = L"LANG_" + code;
    CharUpperBuffW(&res[0], DWORD(res.size()));
    for (auto& kv : ParseLangFile(LoadResourceText(res.c_str()))) tab[kv.first] = Utf8ToW(kv.second);
    std::string ov = LoadFileText(PathJoin(PathJoin(ExeDir(), L"lang"), code + L".ini"));
    for (auto& kv : ParseLangFile(ov)) tab[kv.first] = Utf8ToW(kv.second);
}

std::wstring DetectOsLanguage() {
    LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)) {
        case LANG_JAPANESE: return L"ja";
        case LANG_THAI: return L"th";
        default: return L"en";
    }
}

void I18nLoad(const std::wstring& code) {
    std::wstring c = (code == L"ja" || code == L"th" || code == L"en") ? code : DetectOsLanguage();
    g_lang = c;
    if (g_fallback.empty()) Fill(g_fallback, L"en");
    Fill(g_tab, c);
}

const std::wstring& CurrentLanguage() { return g_lang; }

const std::wstring& T(const char* key) {
    auto it = g_tab.find(key);
    if (it != g_tab.end()) return it->second;
    it = g_fallback.find(key);
    if (it != g_fallback.end()) return it->second;
    static std::map<std::string, std::wstring> miss;
    auto& s = miss[key];
    if (s.empty()) s = Utf8ToW(key);
    return s;
}

static bool FontExists(const wchar_t* face) {
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    bool found = false;
    HDC dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int {
        *reinterpret_cast<bool*>(p) = true; return 0; }, LPARAM(&found), 0);
    ReleaseDC(nullptr, dc);
    return found;
}

std::wstring UiFontFace() {
    // MS UI Gothic has no Thai glyphs; choose a face per language with fallbacks.
    static const wchar_t* ja[] = {L"Yu Gothic UI", L"Meiryo UI", L"MS UI Gothic", nullptr};
    static const wchar_t* th[] = {L"Leelawadee UI", L"Leelawadee", L"Tahoma", nullptr};
    static const wchar_t* en[] = {L"Segoe UI", L"Tahoma", nullptr};
    const wchar_t** list = g_lang == L"ja" ? ja : (g_lang == L"th" ? th : en);
    for (int i = 0; list[i]; ++i) if (FontExists(list[i])) return list[i];
    return L"Tahoma";
}

HFONT CreateUiFont(int dpi, int pt, bool bold) {
    LOGFONTW lf = {};
    lf.lfHeight = -MulDiv(pt, dpi, 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    std::wstring face = UiFontFace();
    wcsncpy(lf.lfFaceName, face.c_str(), LF_FACESIZE - 1);
    return CreateFontIndirectW(&lf);
}

} // namespace lv
