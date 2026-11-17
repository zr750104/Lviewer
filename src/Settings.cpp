#include "Settings.h"

namespace lv {

Settings g_set;

namespace {
enum class Ty { Int, Bool, Str, Color };
struct Field { const wchar_t* sec; const wchar_t* key; Ty ty; size_t off; };
#define F(sec, key, ty, member) {sec, key, Ty::ty, offsetof(Settings, member)}
const Field kFields[] = {
    F(L"General", L"SettingsVersion", Int, settingsVersion), F(L"General", L"Lang", Str, lang), F(L"General", L"MultiInstance", Bool, multiInstance),
    F(L"General", L"RestoreLast", Bool, restoreLast), F(L"General", L"LastFile", Str, lastFile),
    F(L"Window", L"X", Int, wx), F(L"Window", L"Y", Int, wy), F(L"Window", L"W", Int, ww), F(L"Window", L"H", Int, wh),
    F(L"Window", L"Maximized", Bool, maximized), F(L"Window", L"TopMost", Bool, topMost),
    F(L"Window", L"Caption", Bool, caption), F(L"Window", L"OuterFrame", Bool, outerFrame),
    F(L"Window", L"InnerFrame", Bool, innerFrame), F(L"Window", L"MenuBar", Bool, menuBar),
    F(L"Window", L"ToolBar", Bool, toolBar), F(L"Window", L"StatusBar", Bool, statusBar),
    F(L"Window", L"ScrollBar", Bool, scrollBar), F(L"Window", L"CenterWindow", Bool, centerWindow),
    F(L"Window", L"FitToToolbar", Bool, fitWindowToToolbar),
    F(L"Background", L"Color1", Color, bgColor1), F(L"Background", L"Color2", Color, bgColor2),
    F(L"Background", L"Checker", Bool, bgChecker), F(L"Background", L"CheckerSize", Int, checkerSize),
    F(L"Background", L"FullScreenColor", Color, fsBgColor),
    F(L"View", L"DispOpt", Int, dispOpt), F(L"View", L"DispOptFull", Int, dispOptFull),
    F(L"View", L"FitOpt", Int, fitOpt), F(L"View", L"RotOpt", Int, rotOpt),
    F(L"View", L"MaxFitZoom", Int, maxFitZoom), F(L"View", L"ZoomLock", Bool, zoomLock),
    F(L"View", L"Quality", Int, quality), F(L"View", L"SmoothZoomIn", Bool, smoothZoomIn),
    F(L"View", L"HoverScroll", Bool, hoverScroll), F(L"View", L"HoverMargin", Int, hoverMargin),
    F(L"View", L"ExifRotate", Bool, exifRotate), F(L"View", L"ShowTransparency", Bool, showTransparency),
    F(L"View", L"FullScreenHideCursor", Bool, fsHideCursor),
    F(L"List", L"Sort", Int, sortKey), F(L"List", L"Reverse", Bool, sortReverse),
    F(L"List", L"Numeric", Bool, sortNumeric), F(L"List", L"Loop", Bool, loopList),
    F(L"List", L"BeepAtEnd", Bool, beepAtEnd), F(L"List", L"PagesInNav", Bool, pagesInNav),
    F(L"List", L"SkipCount", Int, skipCount), F(L"List", L"IncludeHidden", Bool, includeHidden),
    F(L"Comic", L"Enabled", Bool, comic), F(L"Comic", L"RightToLeft", Bool, comicRtoL),
    F(L"Comic", L"LandscapeSingle", Bool, comicLandscapeSingle),
    F(L"Slide", L"IntervalMs", Int, slideMs), F(L"Slide", L"Loop", Bool, slideLoop),
    F(L"Slide", L"Random", Bool, slideRandom), F(L"Slide", L"FullScreen", Bool, slideFull),
    F(L"Slide", L"NextFolder", Bool, slideNextDir),
    F(L"Mouse", L"WheelScroll", Int, wheelScroll), F(L"Mouse", L"DragMargin", Int, dragMargin),
    F(L"Mouse", L"DragScrollRate", Int, dragScrollRate), F(L"Mouse", L"DragZoomSpeed", Int, dragZoomSpeed),
    F(L"Mouse", L"DragZoomDir", Int, dragZoomDir),
    F(L"Mouse", L"DragL", Int, dragAction[0]), F(L"Mouse", L"DragR", Int, dragAction[1]), F(L"Mouse", L"DragM", Int, dragAction[2]),
    F(L"Mouse", L"DragX1", Int, dragAction[3]), F(L"Mouse", L"DragX2", Int, dragAction[4]), F(L"Mouse", L"HoverMinMove", Int, hoverMinMove),
    F(L"Update", L"CheckAtStart", Bool, updateCheckAtStart), F(L"Update", L"LastCheckDay", Int, lastUpdateCheckDay),
    F(L"Mouse", L"WheelDull", Bool, wheelDull), F(L"Mouse", L"ActivateNoClick", Bool, activateNoClick),
    F(L"File", L"ConfirmDelete", Bool, confirmDelete), F(L"File", L"ConfirmOverwrite", Bool, confirmOverwrite),
    F(L"File", L"MoveNextAfterDelete", Bool, moveNextAfterDelete),
    F(L"File", L"LastSaveDir", Str, lastSaveDir), F(L"File", L"LastCopyDir", Str, lastCopyDir),
    F(L"File", L"LastMoveDir", Str, lastMoveDir),
    F(L"Save", L"JpegQuality", Int, jpegQuality), F(L"Save", L"PngCompression", Int, pngCompression),
    F(L"Save", L"TiffCompression", Int, tiffCompression), F(L"Save", L"KeepTimestamp", Bool, keepTimestamp),
    F(L"Edit", L"UndoMax", Int, undoMax), F(L"Edit", L"SelUnit", Int, selUnit),
    F(L"Mask", L"GlassSize", Int, glassSize), F(L"Mask", L"GlassRemainder", Bool, glassRemainder),
    F(L"Mask", L"Q0GlassV", Bool, q0GlassV), F(L"Mask", L"Q0GlassH", Bool, q0GlassH),
    F(L"Mask", L"Q0Nega", Bool, q0Nega), F(L"Mask", L"Q0Xor", Bool, q0Xor),
    F(L"Mask", L"Q0FlipV", Bool, q0FlipV), F(L"Mask", L"Q0FlipH", Bool, q0FlipH),
    F(L"Other", L"WallpaperFile", Str, wallpaperFile),
};
#undef F

std::wstring ReadStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, bool& found) {
    std::wstring buf(4096, 0);
    DWORD n = GetPrivateProfileStringW(sec, key, L"\x01", &buf[0], DWORD(buf.size()), ini.c_str());
    buf.resize(n);
    found = !(n == 1 && buf[0] == 1);
    return found ? buf : L"";
}
void WriteStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, const std::wstring& v) {
    WritePrivateProfileStringW(sec, key, v.c_str(), ini.c_str());
}
} // namespace

void PushMru(std::vector<std::wstring>& v, const std::wstring& s, size_t max) {
    if (s.empty()) return;
    for (size_t i = 0; i < v.size(); ++i)
        if (lstrcmpiW(v[i].c_str(), s.c_str()) == 0) { v.erase(v.begin() + i); break; }
    v.insert(v.begin(), s);
    if (v.size() > max) v.resize(max);
}

std::wstring ResolveIniPath(const std::wstring& user, bool userGiven) {
    std::wstring dir = ExeDir();
    if (userGiven && !user.empty()) {
        std::wstring d = PathJoin(PathJoin(dir, L"user"), user);
        SHCreateDirectoryExW(nullptr, d.c_str(), nullptr);
        return PathJoin(d, L"Lviewer.ini");
    }
    // Portable: next to the exe when writable, otherwise %APPDATA%\Lviewer
    std::wstring p = PathJoin(dir, L"Lviewer.ini");
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        bool created = GetLastError() != ERROR_ALREADY_EXISTS;
        CloseHandle(h);
        if (created) DeleteFileW(p.c_str());
        return p;
    }
    wchar_t* app = nullptr;
    std::wstring d;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &app))) { d = PathJoin(app, L"Lviewer"); CoTaskMemFree(app); }
    SHCreateDirectoryExW(nullptr, d.c_str(), nullptr);
    return PathJoin(d, L"Lviewer.ini");
}

void Settings::Load(const std::wstring& path) {
    iniPath = path;
    char* base = reinterpret_cast<char*>(this);
    for (const Field& f : kFields) {
        bool found;
        std::wstring v = ReadStr(path, f.sec, f.key, found);
        if (!found) continue;
        void* p = base + f.off;
        switch (f.ty) {
            case Ty::Int: *static_cast<int*>(p) = _wtoi(v.c_str()); break;
            case Ty::Bool: *static_cast<bool*>(p) = _wtoi(v.c_str()) != 0; break;
            case Ty::Str: *static_cast<std::wstring*>(p) = v; break;
            case Ty::Color: {
                unsigned r = 0, g = 0, b = 0;
                if (swscanf(v.c_str(), L"%u,%u,%u", &r, &g, &b) == 3) *static_cast<COLORREF*>(p) = RGB(r, g, b);
            } break;
        }
    }
    bool found;
    mruDirs.clear(); mruFiles.clear();
    for (int i = 0; i < 10; ++i) {
        std::wstring v = ReadStr(path, L"MRU", Fmt(L"Dir%d", i).c_str(), found);
        if (found && !v.empty()) mruDirs.push_back(v);
        v = ReadStr(path, L"MRU", Fmt(L"File%d", i).c_str(), found);
        if (found && !v.empty()) mruFiles.push_back(v);
    }
    for (int i = 0; i < 9; ++i) {
        std::wstring v = ReadStr(path, L"OtherApp", Fmt(L"App%d", i + 1).c_str(), found);
        if (!found) continue;
        auto parts = Split(v, L'|');
        apps[i].name = parts.size() > 0 ? parts[0] : L"";
        apps[i].path = parts.size() > 1 ? parts[1] : L"";
        apps[i].args = parts.size() > 2 ? parts[2] : L"";
    }
    // [Keys] section: name=keys
    std::wstring sec(65536, 0);
    DWORD n = GetPrivateProfileSectionW(L"Keys", &sec[0], DWORD(sec.size()), path.c_str());
    keys.clear();
    for (const wchar_t* s = sec.c_str(); s < sec.c_str() + n && *s; s += wcslen(s) + 1) {
        std::wstring line = s;
        size_t eq = line.find(L'=');
        if (eq != std::wstring::npos) keys[line.substr(0, eq)] = line.substr(eq + 1);
    }
    // one-time migration of settings files written by v1.2.0 or older
    if (FileExists(path)) {
        bool hasVer = false;
        ReadStr(path, L"General", L"SettingsVersion", hasVer);
        if (!hasVer) {
            if (dispOpt == DispBigToWnd) dispOpt = DispBigToDesk;
            fitWindowToToolbar = false;
            settingsVersion = 2;
        }
    }
    checkerSize = std::max(2, std::min(64, checkerSize));
    undoMax = std::max(0, std::min(50, undoMax));
    slideMs = std::max(100, slideMs);
    jpegQuality = std::max(1, std::min(100, jpegQuality));
    dragScrollRate = std::max(1, std::min(10, dragScrollRate));
    dragZoomSpeed = std::max(0, std::min(2, dragZoomSpeed));
    dragZoomDir = std::max(0, std::min(3, dragZoomDir));
    hoverMargin = std::max(0, std::min(50, hoverMargin));
    hoverMinMove = std::max(0, std::min(10, hoverMinMove));
    for (int& a : dragAction) a = std::max(0, std::min(3, a));
}

void Settings::Save() const {
    if (iniPath.empty()) return;
    // ensure a UTF-16LE file so that WritePrivateProfileStringW keeps non-ASCII paths intact
    if (!FileExists(iniPath)) {
        HANDLE h = CreateFileW(iniPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            const unsigned char bom[2] = {0xFF, 0xFE}; DWORD wr;
            WriteFile(h, bom, 2, &wr, nullptr); CloseHandle(h);
        }
    }
    const char* base = reinterpret_cast<const char*>(this);
    for (const Field& f : kFields) {
        const void* p = base + f.off;
        std::wstring v;
        switch (f.ty) {
            case Ty::Int: v = Fmt(L"%d", *static_cast<const int*>(p)); break;
            case Ty::Bool: v = *static_cast<const bool*>(p) ? L"1" : L"0"; break;
            case Ty::Str: v = *static_cast<const std::wstring*>(p); break;
            case Ty::Color: { COLORREF c = *static_cast<const COLORREF*>(p); v = Fmt(L"%u,%u,%u", GetRValue(c), GetGValue(c), GetBValue(c)); } break;
        }
        WriteStr(iniPath, f.sec, f.key, v);
    }
    WritePrivateProfileSectionW(L"MRU", L"\0\0", iniPath.c_str());
    for (size_t i = 0; i < mruDirs.size(); ++i) WriteStr(iniPath, L"MRU", Fmt(L"Dir%d", int(i)).c_str(), mruDirs[i]);
    for (size_t i = 0; i < mruFiles.size(); ++i) WriteStr(iniPath, L"MRU", Fmt(L"File%d", int(i)).c_str(), mruFiles[i]);
    for (int i = 0; i < 9; ++i)
        WriteStr(iniPath, L"OtherApp", Fmt(L"App%d", i + 1).c_str(), apps[i].name + L"|" + apps[i].path + L"|" + apps[i].args);
    std::wstring sec;
    for (auto& kv : keys) { sec += kv.first + L"=" + kv.second; sec.push_back(0); }
    sec.push_back(0);
    WritePrivateProfileSectionW(L"Keys", sec.c_str(), iniPath.c_str());
}

} // namespace lv
