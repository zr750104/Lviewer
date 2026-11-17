// Lviewer - parameter dialogs (all built at runtime with Form, so they follow the UI language)
#include "Viewer.h"
#include "I18n.h"
#include "Settings.h"
#include "Form.h"
#include <cmath>

namespace lv {

// Run a Form with live preview: apply(img) is called on a fresh copy of the original image.
template <class F> void Viewer::PreviewFilter(Form& f, F apply) {
    if (!img_.valid()) return;
    Image base = img_;
    static bool preview = true;
    f.previewToggle = true;
    f.previewFlag = &preview;
    f.onChange = [&] {
        img_ = base;
        if (preview) apply(img_);
        InvalidateCache(); Redraw(); UpdateWindow(view_);
    };
    bool ok = f.Show(hwnd_);
    img_ = base;
    if (ok) { PushUndo(); apply(img_); Modified(); }
    else { InvalidateCache(); Redraw(); }
}

void Viewer::DlgResize() {
    if (!img_.valid()) return;
    int w = img_.w, h = img_.h, pct = 100, q = 2;
    bool aspect = true;
    int pw = w, ph = h, ppct = pct;
    double ratio = double(img_.h) / img_.w;
    Form f(T("cmd.resize"));
    f.Int(T("dlg.width"), &w, 1, 60000, L"px");
    f.Int(T("dlg.height"), &h, 1, 60000, L"px");
    f.Int(T("dlg.percent"), &pct, 1, 10000, L"%");
    f.Check(T("dlg.keepaspect"), &aspect);
    f.Combo(T("dlg.quality"), &q, {T("q.nearest"), T("q.bilinear"), T("q.bicubic"), T("q.area")});
    Image& im = img_;
    f.onChange = [&] {
        bool upd = false;
        if (pct != ppct) { w = std::max(1, int(std::lround(im.w * pct / 100.0))); h = std::max(1, int(std::lround(im.h * pct / 100.0))); upd = true; }
        else if (w != pw && aspect) { h = std::max(1, int(std::lround(w * ratio))); pct = int(std::lround(w * 100.0 / im.w)); upd = true; }
        else if (h != ph && aspect) { w = std::max(1, int(std::lround(h / ratio))); pct = int(std::lround(h * 100.0 / im.h)); upd = true; }
        pw = w; ph = h; ppct = pct;
        if (upd) f.Reload();
    };
    if (!f.Show(hwnd_)) return;
    ResizeQuality rq = ResizeQuality(q);
    if (rq == ResizeQuality::Area && (w > img_.w || h > img_.h)) rq = ResizeQuality::Bicubic;
    Edit([&](Image& m) { m = Resize(m, w, h, rq); });
    sel_ = Rect{};
}

void Viewer::DlgCanvas() {
    if (!img_.valid()) return;
    int w = img_.w, h = img_.h, anchor = 4;
    static COLORREF col = RGB(255, 255, 255);
    Form f(T("cmd.canvassize"));
    f.Int(T("dlg.width"), &w, 1, 60000, L"px");
    f.Int(T("dlg.height"), &h, 1, 60000, L"px");
    f.Combo(T("dlg.anchor"), &anchor, {T("anchor.0"), T("anchor.1"), T("anchor.2"), T("anchor.3"), T("anchor.4"), T("anchor.5"),
                                       T("anchor.6"), T("anchor.7"), T("anchor.8")});
    f.Color(T("dlg.color"), &col);
    if (!f.Show(hwnd_)) return;
    Edit([&](Image& m) { m = Canvas(m, w, h, anchor, ARGB(255, GetRValue(col), GetGValue(col), GetBValue(col))); });
    sel_ = Rect{};
}

void Viewer::DlgRotAngle() {
    if (!img_.valid()) return;
    static int deg10 = 0;
    static COLORREF col = RGB(255, 255, 255);
    static bool transparent = false;
    Form f(T("cmd.rotangle"));
    f.Slider(T("dlg.angle"), &deg10, -1800, 1800, 10);
    f.Color(T("dlg.color"), &col);
    f.Check(T("dlg.transparentbg"), &transparent);
    PreviewFilter(f, [&](Image& m) {
        if (deg10 == 0) return;
        Pixel bg = transparent ? 0 : ARGB(255, GetRValue(col), GetGValue(col), GetBValue(col));
        m = RotateAngle(m, deg10 / 10.0, bg);
    });
    sel_ = Rect{};
}

void Viewer::DlgLevels() {
    static int black = 0, white = 255, gamma = 100;
    Rect s = Sel();
    Form f(T("cmd.levels"));
    f.Slider(T("dlg.black"), &black, 0, 254);
    f.Slider(T("dlg.white"), &white, 1, 255);
    f.Slider(T("dlg.gamma"), &gamma, 10, 500, 100);
    f.Button(T("btn.reset"), [&] { black = 0; white = 255; gamma = 100; });
    PreviewFilter(f, [&](Image& m) { Levels(m, black, std::max(black + 1, white), gamma / 100.0, s); });
}

void Viewer::DlgBrightness() {
    static int b = 0, c = 0, g = 100;
    Rect s = Sel();
    Form f(T("cmd.brightness"));
    f.Slider(T("dlg.brightness"), &b, -255, 255);
    f.Slider(T("dlg.contrast"), &c, -100, 100);
    f.Slider(T("dlg.gamma"), &g, 10, 500, 100);
    f.Button(T("btn.reset"), [&] { b = 0; c = 0; g = 100; });
    PreviewFilter(f, [&](Image& m) { Brightness(m, b, c, g / 100.0, s); });
}

void Viewer::DlgHueSat() {
    static int hu = 0, sa = 0, li = 0;
    Rect s = Sel();
    Form f(T("cmd.huesat"));
    f.Slider(T("dlg.hue"), &hu, -180, 180);
    f.Slider(T("dlg.saturation"), &sa, -100, 100);
    f.Slider(T("dlg.lightness"), &li, -100, 100);
    f.Button(T("btn.reset"), [&] { hu = 0; sa = 0; li = 0; });
    PreviewFilter(f, [&](Image& m) { HueSatLight(m, hu, sa, li, s); });
}

void Viewer::DlgUnsharp() {
    static int radius = 20, amount = 100, thr = 0;
    Rect s = Sel();
    Form f(T("cmd.unsharp"));
    f.Slider(T("dlg.radius"), &radius, 1, 200, 10);
    f.Slider(T("dlg.amount"), &amount, 1, 500);
    f.Slider(T("dlg.threshold"), &thr, 0, 255);
    PreviewFilter(f, [&](Image& m) { UnsharpMask(m, radius / 10.0, amount, thr, s); });
}

void Viewer::DlgGaussian() {
    static int radius = 20;
    Rect s = Sel();
    Form f(T("cmd.gaussian"));
    f.Slider(T("dlg.radius"), &radius, 1, 300, 10);
    PreviewFilter(f, [&](Image& m) { GaussianBlur(m, radius / 10.0, s); });
}

void Viewer::DlgColorDepth() {
    static int depth = 0; static bool dither = true;
    Form f(T("cmd.colordepth"));
    f.Combo(T("dlg.depth"), &depth, {T("depth.bw"), T("depth.gray16"), T("depth.gray256"), T("depth.c256"), T("depth.c65536"), T("depth.full")});
    f.Check(T("dlg.dither"), &dither);
    f.Label(T("dlg.depthnote"));
    PreviewFilter(f, [&](Image& m) { ReduceColors(m, Depth(depth), dither); });
}

void Viewer::DlgMaskOptions() {
    Form f(T("cmd.mask.options"));
    f.Int(T("dlg.glasssize"), &g_set.glassSize, 2, 256, L"px");
    f.Check(T("dlg.glassrem"), &g_set.glassRemainder);
    f.Label(T("dlg.q0opts"));
    f.Check(T("cmd.mask.glassv"), &g_set.q0GlassV);
    f.Check(T("cmd.mask.glassh"), &g_set.q0GlassH);
    f.Check(T("cmd.mask.nega"), &g_set.q0Nega);
    f.Check(T("cmd.mask.xor"), &g_set.q0Xor);
    f.Check(T("cmd.flipv"), &g_set.q0FlipV);
    f.Check(T("cmd.fliph"), &g_set.q0FlipH);
    f.Show(hwnd_);
}

void Viewer::DlgSelect() {
    if (!img_.valid()) return;
    Rect r = sel_.empty() ? img_.full() : sel_;
    Form f(T("cmd.selectdialog"));
    f.Int(L"X", &r.x, 0, img_.w - 1, L"px");
    f.Int(L"Y", &r.y, 0, img_.h - 1, L"px");
    f.Int(T("dlg.width"), &r.w, 1, img_.w, L"px");
    f.Int(T("dlg.height"), &r.h, 1, img_.h, L"px");
    if (!f.Show(hwnd_)) return;
    selMode_ = true;
    sel_ = ClipRect(img_, r);
    Redraw(); UpdateStatus();
}

void Viewer::DlgSelUnit() {
    int u = std::max(1, g_set.selUnit);
    if (PromptInt(hwnd_, T("cmd.selunit.custom"), T("dlg.unit"), u, 1, 4096)) g_set.selUnit = u;
}

void Viewer::DlgZoom() {
    if (!img_.valid()) return;
    int z = int(std::lround(zoom_ * 100));
    if (PromptInt(hwnd_, T("cmd.zoomdialog"), T("dlg.zoompct"), z, 1, 6400)) SetZoom(z / 100.0);
}

void Viewer::DlgSlide() {
    int sec10 = std::max(1, g_set.slideMs / 100);
    Form f(T("cmd.slide"));
    f.Slider(T("dlg.interval"), &sec10, 1, 600, 10);
    f.Check(T("dlg.slideloop"), &g_set.slideLoop);
    f.Check(T("dlg.sliderandom"), &g_set.slideRandom);
    f.Check(T("dlg.slidefull"), &g_set.slideFull);
    f.Check(T("dlg.slidenextdir"), &g_set.slideNextDir);
    if (!f.Show(hwnd_)) return;
    g_set.slideMs = sec10 * 100;
    StartSlide(true);
}

void Viewer::DlgPreferences() {
    Settings old = g_set;
    int lang = CurrentLanguage() == L"ja" ? 0 : (CurrentLanguage() == L"th" ? 2 : 1);
    std::vector<std::wstring> disp = {T("cmd.disp.wndtoimg"), T("cmd.disp.imgtownd"), T("cmd.disp.bigtownd"),
                                      T("cmd.disp.imgtodesk"), T("cmd.disp.bigtodesk"), T("cmd.disp.asis")};
    for (auto& s : disp) { std::wstring o; for (wchar_t c : s) if (c != L'&') o += c; s = o; }
    Form f(T("cmd.preferences"));
    f.Page(T("pref.general"));
    f.Combo(T("pref.language"), &lang, {L"日本語 (Japanese)", L"English", L"ภาษาไทย (Thai)"});
    f.Check(T("pref.multi"), &g_set.multiInstance);
    f.Check(T("pref.restore"), &g_set.restoreLast);
    f.Int(T("pref.undomax"), &g_set.undoMax, 0, 50);
    f.Check(T("pref.confirmdelete"), &g_set.confirmDelete);
    f.Check(T("pref.confirmoverwrite"), &g_set.confirmOverwrite);
    f.Check(T("pref.nextafterdelete"), &g_set.moveNextAfterDelete);
    f.Check(T("pref.keeptime"), &g_set.keepTimestamp);
    f.Check(T("pref.updatecheck"), &g_set.updateCheckAtStart);
    f.Page(T("pref.view"));
    f.Combo(T("pref.dispopt"), &g_set.dispOpt, disp);
    f.Combo(T("pref.dispoptfull"), &g_set.dispOptFull, disp);
    f.Combo(T("pref.fitopt"), &g_set.fitOpt, {T("fit.inside"), T("fit.width"), T("fit.height"), T("fit.either")});
    f.Int(T("pref.maxfit"), &g_set.maxFitZoom, 100, 6400, L"%");
    f.Combo(T("pref.quality"), &g_set.quality, {T("q.nearest"), T("q.bilinear"), T("q.high")});
    f.Check(T("pref.smoothin"), &g_set.smoothZoomIn);
    f.Check(T("pref.exifrotate"), &g_set.exifRotate);
    f.Check(T("pref.centerwnd"), &g_set.centerWindow);
    f.Check(T("pref.fittoolbar"), &g_set.fitWindowToToolbar);
    f.Int(T("pref.hovermargin"), &g_set.hoverMargin, 0, 50, L"%");
    f.Page(T("pref.background"));
    f.Color(T("pref.bg1"), &g_set.bgColor1);
    f.Color(T("pref.bg2"), &g_set.bgColor2);
    f.Check(T("pref.checker"), &g_set.bgChecker);
    f.Int(T("pref.checkersize"), &g_set.checkerSize, 2, 64, L"px");
    f.Check(T("pref.showalpha"), &g_set.showTransparency);
    f.Color(T("pref.fsbg"), &g_set.fsBgColor);
    f.Check(T("pref.fshidecursor"), &g_set.fsHideCursor);
    f.Page(T("pref.list"));
    f.Combo(T("pref.sort"), &g_set.sortKey, {T("cmd.sort.name"), T("cmd.sort.date"), T("cmd.sort.size"), T("cmd.sort.ext"), T("cmd.sort.random")});
    f.Check(T("cmd.sort.reverse"), &g_set.sortReverse);
    f.Check(T("cmd.sort.numeric"), &g_set.sortNumeric);
    f.Check(T("pref.loop"), &g_set.loopList);
    f.Check(T("pref.beep"), &g_set.beepAtEnd);
    f.Check(T("pref.pagesinnav"), &g_set.pagesInNav);
    f.Int(T("pref.skip"), &g_set.skipCount, 2, 1000);
    f.Check(T("pref.hidden"), &g_set.includeHidden);
    f.Page(T("pref.save"));
    f.Slider(T("dlg.jpegquality"), &g_set.jpegQuality, 1, 100);
    f.Combo(T("dlg.tiffcomp"), &g_set.tiffCompression, {T("tiff.auto"), T("tiff.none"), L"LZW", L"ZIP (Deflate)", L"CCITT Group 4", L"PackBits"});
    f.Label(T("dlg.tiffnote"));
    if (!f.Show(hwnd_)) { g_set = old; return; }
    static const wchar_t* codes[] = {L"ja", L"en", L"th"};
    if (codes[lang] != CurrentLanguage()) SetLanguage(codes[lang]);
    if (g_set.sortKey != old.sortKey || g_set.sortReverse != old.sortReverse || g_set.sortNumeric != old.sortNumeric) list_.Sort();
    if (g_set.includeHidden != old.includeHidden) list_.Refresh();
    if (g_set.exifRotate != old.exifRotate && list_.Current() && untitled_.empty() && !modified_) LoadCurrent(page_);
    ApplyDisplayOption(true);
    InvalidateCache(); UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
    g_set.Save();
}

bool Viewer::DlgMouseOptions() {
    Form f(T("mouse.page.options"));
    f.Int(T("mouse.wheelscroll"), &g_set.wheelScroll, 1, 1000, L"px");
    f.Int(T("mouse.dragscroll"), &g_set.dragScrollRate, 1, 10, L"x");
    f.Combo(T("mouse.dragzoomspeed"), &g_set.dragZoomSpeed, {T("mouse.speed.slow"), T("mouse.speed.medium"), T("mouse.speed.fast")});
    f.Combo(T("mouse.dragzoomdir"), &g_set.dragZoomDir, {T("mouse.dir.ltor"), T("mouse.dir.rtol"), T("mouse.dir.btot"), T("mouse.dir.ttob")});
    f.Int(T("mouse.dragmargin"), &g_set.dragMargin, 0, 100, T("mouse.dragmarginunit"));
    f.Int(T("pref.hovermargin"), &g_set.hoverMargin, 0, 50, L"% (0 - 50)");
    f.Int(T("mouse.hoverminmove"), &g_set.hoverMinMove, 0, 10, L"px");
    f.Check(T("mouse.activatenoclick"), &g_set.activateNoClick);
    f.Check(T("mouse.wheeldull"), &g_set.wheelDull);
    f.Button(T("btn.defaults"), [] {
        Settings d;
        g_set.wheelScroll = d.wheelScroll; g_set.dragScrollRate = d.dragScrollRate; g_set.dragZoomSpeed = d.dragZoomSpeed;
        g_set.dragZoomDir = d.dragZoomDir; g_set.dragMargin = d.dragMargin; g_set.hoverMargin = d.hoverMargin;
        g_set.hoverMinMove = d.hoverMinMove; g_set.activateNoClick = d.activateNoClick; g_set.wheelDull = d.wheelDull;
    });
    f.Label(T("mouse.note"));
    return f.Show(hwnd_);
}

void Viewer::DlgApps() {
    OtherApp apps[9];
    for (int i = 0; i < 9; ++i) apps[i] = g_set.apps[i];
    int slot = 0, prevSlot = 0;
    std::wstring name = apps[0].name, path = apps[0].path, args = apps[0].args;
    std::vector<std::wstring> slots;
    for (int i = 0; i < 9; ++i) slots.push_back(Fmt(L"%d", i + 1));
    Form f(T("cmd.appsettings"));
    f.Combo(T("apps.slot"), &slot, slots);
    f.Edit(T("apps.name"), &name, 30);
    f.Path(T("apps.path"), &path, false, 40);
    f.Edit(T("apps.args"), &args, 40);
    f.Label(T("apps.note"));
    f.onChange = [&] {
        if (slot != prevSlot) {
            name = apps[slot].name; path = apps[slot].path; args = apps[slot].args;
            prevSlot = slot;
            f.Reload();
        } else {
            apps[slot].name = name; apps[slot].path = path; apps[slot].args = args;
        }
    };
    if (!f.Show(hwnd_)) return;
    apps[slot].name = name; apps[slot].path = path; apps[slot].args = args;
    for (int i = 0; i < 9; ++i) g_set.apps[i] = apps[i];
    RebuildUi();
    g_set.Save();
}

// ------------------------------------------------------------- association
namespace {
struct AssocGroup { const wchar_t* label; std::vector<const wchar_t*> exts; };
const AssocGroup kAssoc[] = {
    {L"JPEG (jpg, jpeg, jpe, jfif)", {L"jpg", L"jpeg", L"jpe", L"jfif"}},
    {L"PNG", {L"png"}}, {L"GIF", {L"gif"}}, {L"BMP (bmp, dib)", {L"bmp", L"dib"}},
    {L"TIFF (tif, tiff)", {L"tif", L"tiff"}}, {L"ICO", {L"ico"}}, {L"WebP", {L"webp"}},
    {L"HEIF (heic, heif)", {L"heic", L"heif"}}, {L"JPEG XR (jxr, wdp)", {L"jxr", L"wdp"}},
    {L"ZIP / CBZ", {L"zip", L"cbz"}},
};
const wchar_t* kProgId = L"Lviewer.Image";

bool IsAssociated(const wchar_t* ext) {
    std::wstring k = std::wstring(L"Software\\Classes\\.") + ext + L"\\OpenWithProgids";
    HKEY h;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, k.c_str(), 0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS) return false;
    bool r = RegQueryValueExW(h, kProgId, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(h);
    return r;
}

void SetString(HKEY root, const std::wstring& key, const wchar_t* name, const std::wstring& val) {
    HKEY h;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(h, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(val.c_str()), DWORD((val.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
}

void Associate(const wchar_t* ext, bool on) {
    std::wstring k = std::wstring(L"Software\\Classes\\.") + ext + L"\\OpenWithProgids";
    std::wstring cap = L"Software\\Lviewer\\Capabilities\\FileAssociations";
    if (on) {
        HKEY h;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, k.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(h, kProgId, 0, REG_NONE, nullptr, 0);
            RegCloseKey(h);
        }
        SetString(HKEY_CURRENT_USER, cap, (std::wstring(L".") + ext).c_str(), kProgId);
    } else {
        HKEY h;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, k.c_str(), 0, KEY_SET_VALUE, &h) == ERROR_SUCCESS) { RegDeleteValueW(h, kProgId); RegCloseKey(h); }
        if (RegOpenKeyExW(HKEY_CURRENT_USER, cap.c_str(), 0, KEY_SET_VALUE, &h) == ERROR_SUCCESS) { RegDeleteValueW(h, (std::wstring(L".") + ext).c_str()); RegCloseKey(h); }
    }
}
} // namespace

void Viewer::DlgAssociate() {
    const int n = int(sizeof(kAssoc) / sizeof(kAssoc[0]));
    bool st[n], orig[n];
    for (int i = 0; i < n; ++i) { st[i] = IsAssociated(kAssoc[i].exts[0]); orig[i] = st[i]; }
    Form f(T("cmd.associate"));
    f.Label(T("assoc.note"));
    for (int i = 0; i < n; ++i) f.Check(kAssoc[i].label, &st[i]);
    if (!f.Show(hwnd_)) return;
    std::wstring exe = ExePath();
    std::wstring cls = std::wstring(L"Software\\Classes\\") + kProgId;
    SetString(HKEY_CURRENT_USER, cls, nullptr, L"Lviewer Image");
    SetString(HKEY_CURRENT_USER, cls + L"\\DefaultIcon", nullptr, L"\"" + exe + L"\",0");
    SetString(HKEY_CURRENT_USER, cls + L"\\shell\\open\\command", nullptr, L"\"" + exe + L"\" \"%1\"");
    SetString(HKEY_CURRENT_USER, L"Software\\Lviewer\\Capabilities", L"ApplicationName", L"Lviewer");
    SetString(HKEY_CURRENT_USER, L"Software\\Lviewer\\Capabilities", L"ApplicationDescription", T("about.short"));
    SetString(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"Lviewer", L"Software\\Lviewer\\Capabilities");
    SetString(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\Lviewer.exe\\shell\\open\\command", nullptr, L"\"" + exe + L"\" \"%1\"");
    bool any = false;
    for (int i = 0; i < n; ++i) {
        if (st[i]) any = true;
        if (st[i] == orig[i]) continue;
        for (auto e : kAssoc[i].exts) Associate(e, st[i]);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    if (any && MsgBox(hwnd_, T("assoc.opensettings"), MB_YESNO | MB_ICONINFORMATION) == IDYES)
        ShellExecuteW(hwnd_, nullptr, L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace lv
