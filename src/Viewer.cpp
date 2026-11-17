// Lviewer - main window: layout, painting, input, loading and navigation
#include "Viewer.h"
#include "I18n.h"
#include "Settings.h"
#include "Form.h"
#include "Updater.h"
#include <imm.h>
#include <cmath>

namespace lv {

static Viewer* g_viewer = nullptr;
Viewer* Viewer::Instance() { return g_viewer; }

static const wchar_t* kMainClass = L"LviewerMainWnd";
static const wchar_t* kViewClass = L"LviewerView";
enum { TimerAnim = 1, TimerSlide = 2, TimerCursor = 3 };
enum { IdToolbar = 10, IdStatus = 11, IdView = 12 };
static const ULONG_PTR kCopyDataOpen = 0x4C56;

// ------------------------------------------------------------------ toolbar
namespace {
struct TbButton { int cmd; wchar_t glyph; const wchar_t* text; bool mirror; const wchar_t* fallback = L"?"; };
const TbButton kTb[] = {
    {CmdOpen, 0xE8E5, nullptr, false, L"O"}, {CmdSave, 0xE74E, nullptr, false, L"S"}, {0, 0, nullptr, false},
    {CmdPrevFile, 0xE76B, nullptr, false, L"<"}, {CmdNextFile, 0xE76C, nullptr, false, L">"}, {0, 0, nullptr, false},
    {CmdZoomIn, 0xE8A3, nullptr, false, L"+"}, {CmdZoomOut, 0xE71F, nullptr, false, L"-"}, {CmdOriginal, 0, L"1:1", false},
    {CmdFitWindow, 0xE9A6, nullptr, false, L"[ ]"}, {0, 0, nullptr, false},
    {CmdRotL, 0xE7AD, nullptr, true, L"L"}, {CmdRotR, 0xE7AD, nullptr, false, L"R"}, {0, 0, nullptr, false},
    {CmdFullScreen, 0xE740, nullptr, false, L"FS"}, {CmdSlide, 0xE786, nullptr, false, L">>"}, {CmdComic, 0xE736, nullptr, false, L"||"},
    {0, 0, nullptr, false}, {CmdImageInfo, 0xE946, nullptr, false, L"i"}, {CmdDelete, 0xE74D, nullptr, false, L"X"},
    {0, 0, nullptr, false}, {CmdPreferences, 0xE713, nullptr, false, L"*"},
};

bool HasFont(const wchar_t* face) {
    LOGFONTW lf = {}; lf.lfCharSet = DEFAULT_CHARSET; wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    bool f = false; HDC dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int { *reinterpret_cast<bool*>(p) = true; return 0; }, LPARAM(&f), 0);
    ReleaseDC(nullptr, dc);
    return f;
}

HIMAGELIST MakeToolbarImages(int px, COLORREF color) {
    HIMAGELIST il = ImageList_Create(px, px, ILC_COLOR32, 0, 24);
    const wchar_t* iconFace = HasFont(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : (HasFont(L"Segoe MDL2 Assets") ? L"Segoe MDL2 Assets" : nullptr);
    for (auto& b : kTb) {
        if (!b.cmd) continue;
        void* bits = nullptr;
        HBITMAP bmp = CreateDib32(px, px, &bits);
        HDC dc = CreateCompatibleDC(nullptr);
        HGDIOBJ ob = SelectObject(dc, bmp);
        RECT rc = {0, 0, px, px};
        FillRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        bool useText = b.text || !iconFace;
        HFONT f = CreateFontW(useText ? -px * 6 / 10 : -px * 8 / 10, 0, 0, 0, useText ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                              ANTIALIASED_QUALITY, 0, useText ? L"Segoe UI" : iconFace);
        HGDIOBJ of = SelectObject(dc, f);
        SetTextColor(dc, RGB(255, 255, 255)); SetBkMode(dc, TRANSPARENT);
        std::wstring t;
        if (b.text) t = b.text;
        else if (iconFace) t = std::wstring(1, b.glyph);
        else t = b.fallback;
        DrawTextW(dc, t.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        GdiFlush();
        uint32_t* p = static_cast<uint32_t*>(bits);
        if (b.mirror && iconFace && !b.text) for (int y = 0; y < px; ++y) std::reverse(p + y * px, p + y * px + px);
        for (int i = 0; i < px * px; ++i) {
            int a = (p[i] >> 16) & 0xFF;   // red channel as coverage
            p[i] = (uint32_t(a) << 24) | (uint32_t(GetRValue(color) * a / 255) << 16) | (uint32_t(GetGValue(color) * a / 255) << 8) | uint32_t(GetBValue(color) * a / 255);
        }
        SelectObject(dc, of); DeleteObject(f);
        SelectObject(dc, ob); DeleteDC(dc);
        ImageList_Add(il, bmp, nullptr);
        DeleteObject(bmp);
    }
    return il;
}
} // namespace

void Viewer::CreateToolbar() {
    if (tb_) DestroyWindow(tb_);
    if (tbImages_) ImageList_Destroy(tbImages_);
    tb_ = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                          WS_CHILD | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NODIVIDER | CCS_TOP | TBSTYLE_TRANSPARENT,
                          0, 0, 0, 0, hwnd_, (HMENU)IdToolbar, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(tb_, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    int px = Scale(20, dpi_);
    tbImages_ = MakeToolbarImages(px, GetSysColor(COLOR_BTNTEXT));
    SendMessageW(tb_, TB_SETIMAGELIST, 0, LPARAM(tbImages_));
    std::vector<TBBUTTON> btns;
    int img = 0;
    for (auto& b : kTb) {
        TBBUTTON t = {};
        if (!b.cmd) { t.fsStyle = BTNS_SEP; }
        else { t.iBitmap = img++; t.idCommand = b.cmd; t.fsState = TBSTATE_ENABLED; t.fsStyle = BTNS_BUTTON; }
        btns.push_back(t);
    }
    SendMessageW(tb_, TB_ADDBUTTONSW, btns.size(), LPARAM(btns.data()));
    SendMessageW(tb_, TB_SETBUTTONSIZE, 0, MAKELPARAM(px + Scale(10, dpi_), px + Scale(8, dpi_)));
    SendMessageW(tb_, TB_AUTOSIZE, 0, 0);
    ShowWindow(tb_, (g_set.toolBar && !full_) ? SW_SHOW : SW_HIDE);
}

// ------------------------------------------------------------------- menus
HMENU Viewer::BuildMenu(const MenuNode& n, bool popup) {
    HMENU m = popup ? CreatePopupMenu() : CreateMenu();
    for (auto& k : n.kids) {
        if (k.cmd == -1) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); continue; }
        if (k.dynamic) {
            HMENU sub = CreatePopupMenu();
            FillDynamic(sub, k.dynamic);
            dynMenus_[sub] = k.dynamic;
            AppendMenuW(m, MF_POPUP, UINT_PTR(sub), T(k.label).c_str());
            continue;
        }
        if (k.label) {
            HMENU sub = BuildMenu(k, true);
            AppendMenuW(m, MF_POPUP, UINT_PTR(sub), T(k.label).c_str());
            continue;
        }
        std::wstring text = T((std::string("cmd.") + FindCommand(k.cmd)->name).c_str());
        std::wstring keys = g_keymap.DisplayKeysOf(k.cmd);
        if (!keys.empty()) text += L"\t" + keys;
        AppendMenuW(m, MF_STRING, UINT_PTR(k.cmd), text.c_str());
    }
    return m;
}

void Viewer::FillDynamic(HMENU m, int kind) {
    while (GetMenuItemCount(m) > 0) DeleteMenu(m, 0, MF_BYPOSITION);
    if (kind == 1 || kind == 2) {
        auto& v = kind == 1 ? g_set.mruDirs : g_set.mruFiles;
        int base = kind == 1 ? CmdMruDir0 : CmdMruFile0;
        if (v.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 0, T("menu.empty").c_str());
        for (size_t i = 0; i < v.size() && i < 10; ++i) {
            std::wstring s = Fmt(L"&%d  ", int((i + 1) % 10)) + v[i];
            std::wstring esc;
            for (wchar_t c : s) { esc += c; }
            AppendMenuW(m, MF_STRING, base + UINT(i), esc.c_str());
        }
    } else if (kind == 3) {
        for (int i = 0; i < 9; ++i) {
            std::wstring name = g_set.apps[i].name.empty() ? (g_set.apps[i].path.empty() ? T("menu.notset") : PathFileName(g_set.apps[i].path)) : g_set.apps[i].name;
            std::wstring text = Fmt(L"(&%d) ", i + 1) + name;
            std::wstring keys = g_keymap.DisplayKeysOf(CmdApp1 + i);
            if (!keys.empty()) text += L"\t" + keys;
            AppendMenuW(m, MF_STRING | (g_set.apps[i].path.empty() ? MF_GRAYED : 0), CmdApp1 + i, text.c_str());
        }
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, CmdAppSettings, T("cmd.appsettings").c_str());
    }
}

void Viewer::UpdateMenuStates(HMENU m) {
    int n = GetMenuItemCount(m);
    for (int i = 0; i < n; ++i) {
        UINT id = GetMenuItemID(m, i);
        if (id == UINT(-1) || id == 0) continue;
        if (int(id) > CmdFirst && int(id) < CmdLast) {
            EnableMenuItem(m, i, MF_BYPOSITION | (IsEnabled(int(id)) ? MF_ENABLED : MF_GRAYED));
            CheckMenuItem(m, i, MF_BYPOSITION | (IsChecked(int(id)) ? MF_CHECKED : MF_UNCHECKED));
        }
    }
}

void Viewer::ShowPopup(const char* label, POINT pt) {
    const MenuNode* node = label ? FindSubmenu(label) : &MenuTree();
    if (!node) return;
    auto savedDyn = dynMenus_;
    HMENU m;
    if (node->dynamic) { m = CreatePopupMenu(); FillDynamic(m, node->dynamic); }
    else m = BuildMenu(*node, true);
    if (pt.x == -1 && pt.y == -1) { RECT r; GetWindowRect(view_, &r); pt = {r.left + 20, r.top + 20}; }
    int cmd = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, hwnd_, nullptr);
    DestroyMenu(m);
    dynMenus_ = savedDyn;
    if (cmd) Execute(cmd);
}

void Viewer::RebuildUi() {
    dpi_ = DpiOf(hwnd_);
    if (uiFont_) DeleteObject(uiFont_);
    uiFont_ = CreateUiFont(dpi_);
    HMENU old = menuBar_;
    dynMenus_.clear();
    menuBar_ = BuildMenu(MenuTree(), false);
    SetMenu(hwnd_, (g_set.menuBar && !full_) ? menuBar_ : nullptr);
    if (old) DestroyMenu(old);
    CreateToolbar();
    if (sb_) SendMessageW(sb_, WM_SETFONT, WPARAM(uiFont_), TRUE);
    Layout();
    UpdateTitle();
    UpdateStatus();
}

// ------------------------------------------------------------------ layout
void Viewer::Layout() {
    if (!hwnd_) return;
    RECT rc; GetClientRect(hwnd_, &rc);
    int top = 0, bottom = rc.bottom;
    bool showTb = g_set.toolBar && !full_, showSb = g_set.statusBar && !full_;
    if (tb_) {
        ShowWindow(tb_, showTb ? SW_SHOW : SW_HIDE);
        if (showTb) { SendMessageW(tb_, TB_AUTOSIZE, 0, 0); RECT r; GetWindowRect(tb_, &r); top = r.bottom - r.top; }
    }
    if (sb_) {
        ShowWindow(sb_, showSb ? SW_SHOW : SW_HIDE);
        if (showSb) {
            SendMessageW(sb_, WM_SIZE, 0, 0);
            RECT r; GetWindowRect(sb_, &r); bottom -= r.bottom - r.top;
            int w = rc.right;
            int parts[6];
            int fixed[5] = {Scale(150, dpi_), Scale(90, dpi_), Scale(80, dpi_), Scale(70, dpi_), Scale(170, dpi_)};
            int x = 0;
            for (int i = 0; i < 5; ++i) { x += fixed[i]; parts[i] = x; }
            parts[5] = -1;
            (void)w;
            SendMessageW(sb_, SB_SETPARTS, 6, LPARAM(parts));
        }
    }
    MoveWindow(view_, 0, top, rc.right, std::max(0, int(bottom - top)), TRUE);
}

void Viewer::ApplyWindowStyles() {
    if (full_) return;
    LONG_PTR st = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    st &= ~(WS_CAPTION | WS_THICKFRAME | WS_POPUP);
    st |= WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    if (g_set.caption) st |= WS_CAPTION;
    else st |= WS_POPUP;
    if (g_set.outerFrame) st |= WS_THICKFRAME;
    else if (!g_set.caption) st |= WS_BORDER;
    SetWindowLongPtrW(hwnd_, GWL_STYLE, st);
    LONG_PTR vs = GetWindowLongPtrW(view_, GWL_EXSTYLE);
    vs = g_set.innerFrame ? (vs | WS_EX_CLIENTEDGE) : (vs & ~WS_EX_CLIENTEDGE);
    SetWindowLongPtrW(view_, GWL_EXSTYLE, vs);
    SetMenu(hwnd_, g_set.menuBar ? menuBar_ : nullptr);
    SetWindowPos(view_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    SetWindowPos(hwnd_, g_set.topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    Layout();
    UpdateScrollBars();
}

// ------------------------------------------------------------------ create
bool Viewer::Create(const StartupOptions& opt) {
    g_viewer = this;
    opt_ = opt;
    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = MainProc; wc.hInstance = hi; wc.lpszClassName = kMainClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    wc.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);
    WNDCLASSEXW vc = {sizeof(vc)};
    vc.style = CS_DBLCLKS;
    vc.lpfnWndProc = ViewProc; vc.hInstance = hi; vc.lpszClassName = kViewClass;
    vc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&vc);

    DWORD st = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    hwnd_ = CreateWindowExW(WS_EX_ACCEPTFILES, kMainClass, L"Lviewer", st, g_set.wx, g_set.wy, g_set.ww, g_set.wh,
                            nullptr, nullptr, hi, this);
    if (!hwnd_) return false;
    dpi_ = DpiOf(hwnd_);
    view_ = CreateWindowExW(0, kViewClass, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 10, hwnd_, (HMENU)IdView, hi, this);
    ImmAssociateContext(view_, nullptr);
    sb_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | SBARS_SIZEGRIP, 0, 0, 0, 0, hwnd_, (HMENU)IdStatus, hi, nullptr);
    RebuildUi();
    ApplyWindowStyles();
    // keep the restored window on a visible monitor
    RECT wr; GetWindowRect(hwnd_, &wr);
    HMONITOR mon = MonitorFromRect(&wr, MONITOR_DEFAULTTONULL);
    if (!mon) SetWindowPos(hwnd_, nullptr, 100, 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(hwnd_, g_set.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    SetFocus(view_);

    if (opt.getclip && !opt.blank) {
        Image im;
        if (PasteImageFromClipboard(hwnd_, im)) {
            img_ = std::move(im); untitled_ = T("misc.untitled"); modified_ = true;
            ApplyDisplayOption(true); UpdateTitle(); UpdateStatus(); Redraw();
        }
    } else if (!opt.blank) {
        std::wstring p = opt.path;
        if (p.empty() && g_set.restoreLast) p = g_set.lastFile;
        if (!p.empty()) OpenPath(p);
    }
    if (opt.slide) StartSlide(true);
    StartBackgroundUpdateCheck();
    UpdateTitle();
    UpdateStatus();
    return true;
}

int Viewer::Run() {
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (PreTranslate(m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return int(m.wParam);
}

bool Viewer::PreTranslate(MSG& m) {
    if (m.message != WM_KEYDOWN && m.message != WM_SYSKEYDOWN) return false;
    if (m.hwnd != hwnd_ && m.hwnd != view_ && m.hwnd != tb_) return false;
    if (!IsWindowEnabled(hwnd_)) return false;
    unsigned vk = unsigned(m.wParam);
    if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN || vk == VK_PROCESSKEY) return false;
    Chord c{CurrentMods(), vk};
    int cmd = g_keymap.Lookup(c);
    if (!cmd) return false;
    Execute(cmd);
    return true;
}

// ---------------------------------------------------------------- main proc
LRESULT CALLBACK Viewer::MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) {
        auto* v = static_cast<Viewer*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, LONG_PTR(v));
        v->hwnd_ = h;
    }
    auto* v = reinterpret_cast<Viewer*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    return v ? v->OnMain(m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT Viewer::OnMain(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_SIZE:
            Layout();
            if (fitRule_) ApplyDisplayOption(false);
            ClampScroll(); UpdateScrollBars(); Redraw(); UpdateStatus();
            if (w == SIZE_RESTORED && !full_) g_set.maximized = false;
            if (w == SIZE_MAXIMIZED) g_set.maximized = true;
            return 0;
        case WM_SETFOCUS: SetFocus(view_); return 0;
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id >= CmdMruDir0 && id < CmdMruDir0 + 10) {
                size_t i = size_t(id - CmdMruDir0);
                if (i < g_set.mruDirs.size()) { std::wstring p = g_set.mruDirs[i]; if (ConfirmDiscard()) OpenPath(p); }
                return 0;
            }
            if (id >= CmdMruFile0 && id < CmdMruFile0 + 10) {
                size_t i = size_t(id - CmdMruFile0);
                if (i < g_set.mruFiles.size()) { std::wstring p = g_set.mruFiles[i]; if (ConfirmDiscard()) OpenPath(p); }
                return 0;
            }
            if (id > CmdFirst && id < CmdLast) { Execute(id); return 0; }
        } break;
        case WM_INITMENUPOPUP: {
            auto it = dynMenus_.find(HMENU(w));
            if (it != dynMenus_.end()) FillDynamic(it->first, it->second);
            UpdateMenuStates(HMENU(w));
            return 0;
        }
        case WM_NOTIFY: {
            auto* nm = reinterpret_cast<NMHDR*>(l);
            if (nm->code == TTN_GETDISPINFOW) {
                auto* di = reinterpret_cast<NMTTDISPINFOW*>(l);
                static std::wstring tip;
                tip = CommandLabel(int(nm->idFrom));
                std::wstring k = g_keymap.DisplayKeysOf(int(nm->idFrom));
                if (!k.empty()) tip += L" (" + k + L")";
                di->lpszText = const_cast<wchar_t*>(tip.c_str());
                return 0;
            }
        } break;
        case WM_DROPFILES: {
            HDROP d = HDROP(w);
            wchar_t buf[32768];
            if (DragQueryFileW(d, 0, buf, 32768) && ConfirmDiscard()) { SetForegroundWindow(hwnd_); OpenPath(buf); }
            DragFinish(d);
            return 0;
        }
        case WM_COPYDATA: {
            auto* cd = reinterpret_cast<COPYDATASTRUCT*>(l);
            if (cd->dwData == kCopyDataOpen && cd->lpData) {
                std::wstring p(static_cast<const wchar_t*>(cd->lpData), cd->cbData / sizeof(wchar_t));
                while (!p.empty() && p.back() == 0) p.pop_back();
                if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
                SetForegroundWindow(hwnd_);
                if (!p.empty() && ConfirmDiscard()) OpenPath(p);
                return TRUE;
            }
        } break;
        case WM_TIMER:
            if (w == TimerAnim && animating_ && !anim_.empty()) {
                animFrame_ = (animFrame_ + 1) % int(anim_.size());
                img_ = anim_[animFrame_];
                InvalidateCache(); Redraw();
                SetTimer(hwnd_, TimerAnim, UINT(std::max(20, delays_[animFrame_])), nullptr);
            } else if (w == TimerSlide && sliding_) {
                int before = list_.Index(); int pb = page_;
                if (g_set.slideRandom && list_.Count() > 1) { GoTo(int(GetTickCount() % unsigned(list_.Count()))); }
                else Navigate(+1);
                if (list_.Index() == before && page_ == pb && !g_set.slideRandom) {
                    if (g_set.slideNextDir) Execute(CmdNextFolder);
                    if (list_.Index() == before && page_ == pb) StartSlide(false);
                }
            } else if (w == TimerCursor) {
                if (full_ && g_set.fsHideCursor && !cursorHidden_ && GetTickCount() - lastMouseMove_ > 1500) {
                    POINT p; GetCursorPos(&p);
                    if (WindowFromPoint(p) == view_) { ShowCursor(FALSE); cursorHidden_ = true; }
                }
            }
            return 0;
        case WM_APP + 10: {
            std::unique_ptr<ReleaseInfo> r(reinterpret_cast<ReleaseInfo*>(l));
            if (r && IsWindowEnabled(hwnd_)) OfferUpdate(*r);
            return 0;
        }
        case WM_DPICHANGED: {
            RECT* r = reinterpret_cast<RECT*>(l);
            SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            RebuildUi();
            return 0;
        }
        case WM_SETTINGCHANGE: case WM_SYSCOLORCHANGE: CreateToolbar(); Layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto* mi = reinterpret_cast<MINMAXINFO*>(l);
            mi->ptMinTrackSize = {Scale(120, dpi_), Scale(80, dpi_)};
            return 0;
        }
        case WM_CLOSE:
            if (!ConfirmDiscard()) return 0;
            StartSlide(false);
            if (full_) ToggleFullScreen();
            {
                WINDOWPLACEMENT wp = {sizeof(wp)};
                GetWindowPlacement(hwnd_, &wp);
                g_set.wx = wp.rcNormalPosition.left; g_set.wy = wp.rcNormalPosition.top;
                g_set.ww = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
                g_set.wh = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
                g_set.maximized = wp.showCmd == SW_SHOWMAXIMIZED;
                g_set.lastFile = list_.CurrentDisplayPath();
                g_set.Save();
            }
            DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            if (cursorHidden_) ShowCursor(TRUE);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

// ----------------------------------------------------------------- view math
RECT Viewer::ViewRect() const { RECT r = {0, 0, 0, 0}; if (view_) GetClientRect(view_, &r); return r; }

// view size as it would be without scrollbars (used for fitting, so the result does not depend on
// whether scrollbars happen to be visible at the moment)
static RECT FullViewRect(HWND view) {
    RECT r = {0, 0, 0, 0};
    if (!view) return r;
    GetClientRect(view, &r);
    LONG_PTR st = GetWindowLongPtrW(view, GWL_STYLE);
    if (st & WS_VSCROLL) r.right += GetSystemMetrics(SM_CXVSCROLL);
    if (st & WS_HSCROLL) r.bottom += GetSystemMetrics(SM_CYHSCROLL);
    return r;
}
int Viewer::DispW() const { return img_.valid() ? std::max(1, int(std::lround(img_.w * zoom_))) : 0; }
int Viewer::DispH() const { return img_.valid() ? std::max(1, int(std::lround(img_.h * zoom_))) : 0; }

POINT Viewer::ImageOrigin() const {
    RECT r = ViewRect();
    int vw = r.right, vh = r.bottom, dw = DispW(), dh = DispH();
    POINT p;
    p.x = dw <= vw ? (vw - dw) / 2 : -sx_;
    p.y = dh <= vh ? (vh - dh) / 2 : -sy_;
    return p;
}

bool Viewer::ViewToImage(POINT v, double& ix, double& iy) const {
    if (!img_.valid()) return false;
    POINT o = ImageOrigin();
    ix = (v.x - o.x) / zoom_;
    iy = (v.y - o.y) / zoom_;
    return true;
}

double Viewer::FitZoom(int vw, int vh) const {
    if (!img_.valid() || vw <= 0 || vh <= 0) return 1.0;
    double zx = double(vw) / img_.w, zy = double(vh) / img_.h, z;
    switch (g_set.fitOpt) {
        case FitWidth: z = zx; break;
        case FitHeight: z = zy; break;
        case FitEither: z = std::max(zx, zy); break;
        default: z = std::min(zx, zy); break;
    }
    // with width/height fit a scrollbar may appear: keep one-axis fits stable
    double cap = std::max(1.0, g_set.maxFitZoom / 100.0);
    return std::max(0.001, std::min(z, cap));
}

void Viewer::ClampScroll() {
    RECT r = ViewRect();
    int mx = std::max(0, DispW() - int(r.right)), my = std::max(0, DispH() - int(r.bottom));
    sx_ = std::max(0, std::min(sx_, mx));
    sy_ = std::max(0, std::min(sy_, my));
}

void Viewer::UpdateScrollBars() {
    if (!view_) return;
    bool want = g_set.scrollBar && !full_ && img_.valid();
    RECT r = ViewRect();
    // compute with scrollbars present to avoid oscillation
    int dw = DispW(), dh = DispH();
    bool needH = want && dw > r.right, needV = want && dh > r.bottom;
    SCROLLINFO si = {sizeof(si)};
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = needH ? dw - 1 : 0; si.nPage = needH ? UINT(r.right) : 0; si.nPos = sx_;
    SetScrollInfo(view_, SB_HORZ, &si, TRUE);
    ShowScrollBar(view_, SB_HORZ, needH);
    si.nMax = needV ? dh - 1 : 0; si.nPage = needV ? UINT(r.bottom) : 0; si.nPos = sy_;
    SetScrollInfo(view_, SB_VERT, &si, TRUE);
    ShowScrollBar(view_, SB_VERT, needV);
}

void Viewer::ApplyDisplayOption(bool initial) {
    if (!img_.valid()) return;
    RECT r = FullViewRect(view_);
    int vw = r.right, vh = r.bottom;
    if (!initial && fitRule_) {
        double z = FitZoom(vw, vh);
        if (fitRule_ == 2) z = std::min(1.0, z);
        if (std::fabs(z - zoom_) > 1e-9) { zoom_ = z; InvalidateCache(); }
        ClampScroll();
        return;
    }
    if (initial && g_set.zoomLock) { fitRule_ = 0; sx_ = sy_ = 0; ClampScroll(); InvalidateCache(); return; }
    int opt = full_ ? g_set.dispOptFull : g_set.dispOpt;
    bool canResize = !full_ && !IsZoomed(hwnd_) && !IsIconic(hwnd_);
    if (!canResize) {
        if (opt == DispWndToImg || opt == DispBigToDesk) opt = DispBigToWnd;
        if (opt == DispImgToDesk) opt = DispImgToWnd;
    }
    switch (opt) {
        case DispAsIs: zoom_ = 1.0; fitRule_ = 0; break;
        case DispImgToWnd: fitRule_ = 1; zoom_ = FitZoom(vw, vh); break;
        case DispBigToWnd: fitRule_ = 2; zoom_ = std::min(1.0, FitZoom(vw, vh)); break;
        case DispWndToImg: FitWindowToImage(false); break;
        case DispImgToDesk: FitWindowToImage(true); break;
        case DispBigToDesk: FitWindowToImage(false); break;
    }
    sx_ = 0; sy_ = 0;
    if (g_set.comic && g_set.comicRtoL) sx_ = std::max(0, DispW() - int(ViewRect().right));
    ClampScroll();
    InvalidateCache();
}

void Viewer::FitWindowToImage(bool desk) {
    HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
    RECT wr; GetWindowRect(hwnd_, &wr);
    RECT cr; GetClientRect(hwnd_, &cr);
    RECT vr = FullViewRect(view_);
    // extra = window size - view size
    int exW = (wr.right - wr.left) - vr.right, exH = (wr.bottom - wr.top) - vr.bottom;
    int maxVW = (mi.rcWork.right - mi.rcWork.left) - exW, maxVH = (mi.rcWork.bottom - mi.rcWork.top) - exH;
    int vw, vh;
    if (desk) {
        vw = maxVW; vh = maxVH;
        zoom_ = FitZoom(vw, vh);
        fitRule_ = 1;
        vw = int(std::lround(img_.w * zoom_)); vh = int(std::lround(img_.h * zoom_));
    } else if (img_.w <= maxVW && img_.h <= maxVH) {
        zoom_ = 1.0; fitRule_ = 0; vw = img_.w; vh = img_.h;
    } else {
        zoom_ = std::min(double(maxVW) / img_.w, double(maxVH) / img_.h);
        fitRule_ = 0;
        vw = std::max(1, int(img_.w * zoom_)); vh = std::max(1, int(img_.h * zoom_));
    }
    int ww = vw + exW, wh = vh + exH;
    if (g_set.fitWindowToToolbar && tb_ && IsWindowVisible(tb_)) {
        SIZE s = {}; SendMessageW(tb_, TB_GETMAXSIZE, 0, LPARAM(&s));
        ww = std::max<int>(ww, s.cx + (wr.right - wr.left) - cr.right + Scale(8, dpi_));
    }
    ww = std::max(ww, Scale(120, dpi_)); wh = std::max(wh, Scale(80, dpi_));
    int x = wr.left, y = wr.top;
    if (g_set.centerWindow || desk) {
        x = (mi.rcWork.left + mi.rcWork.right - ww) / 2; y = (mi.rcWork.top + mi.rcWork.bottom - wh) / 2;
    }
    x = std::max<int>(mi.rcWork.left, std::min<int>(x, mi.rcWork.right - ww));
    y = std::max<int>(mi.rcWork.top, std::min<int>(y, mi.rcWork.bottom - wh));
    int saveRule = fitRule_; double saveZoom = zoom_;
    fitRule_ = 0;   // prevent WM_SIZE from re-fitting with stale size
    SetWindowPos(hwnd_, nullptr, x, y, ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);
    fitRule_ = saveRule; zoom_ = saveZoom;
    if (fitRule_) { RECT nr = FullViewRect(view_); zoom_ = FitZoom(nr.right, nr.bottom); }
}

void Viewer::SetZoom(double z, POINT* anchor, bool user) {
    if (!img_.valid()) return;
    z = std::max(0.01, std::min(64.0, z));
    RECT r = ViewRect();
    POINT a = anchor ? *anchor : POINT{r.right / 2, r.bottom / 2};
    double ix, iy; ViewToImage(a, ix, iy);
    zoom_ = z;
    if (user) fitRule_ = 0;
    int dw = DispW(), dh = DispH();
    sx_ = dw > r.right ? int(std::lround(ix * zoom_ - a.x)) : 0;
    sy_ = dh > r.bottom ? int(std::lround(iy * zoom_ - a.y)) : 0;
    ClampScroll();
    InvalidateCache();
    UpdateScrollBars();
    Redraw();
    UpdateTitle(); UpdateStatus();
}

void Viewer::ZoomStep(int dir, POINT* anchor) {
    static const double steps[] = {0.02, 0.03, 0.05, 0.0625, 0.08, 0.1, 0.125, 0.15, 0.2, 0.25, 0.3, 0.333333, 0.4, 0.5, 0.6, 0.666667, 0.75, 0.8, 0.9,
                                   1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 16.0, 20.0, 24.0, 32.0, 48.0, 64.0};
    double z = zoom_;
    double nz = z;
    if (dir > 0) { for (double s : steps) if (s > z * 1.0001) { nz = s; break; } }
    else { for (int i = int(sizeof(steps) / sizeof(steps[0])) - 1; i >= 0; --i) if (steps[i] < z * 0.9999) { nz = steps[i]; break; } }
    SetZoom(nz, anchor);
}

void Viewer::ScrollBy(int dx, int dy) {
    int ox = sx_, oy = sy_;
    sx_ += dx; sy_ += dy;
    ClampScroll();
    if (ox != sx_ || oy != sy_) { UpdateScrollBars(); Redraw(); }
}

void Viewer::ScrollTo(double fx, double fy) {
    RECT r = ViewRect();
    sx_ = int((DispW() - r.right) * fx);
    sy_ = int((DispH() - r.bottom) * fy);
    ClampScroll(); UpdateScrollBars(); Redraw();
}

void Viewer::HoverScroll(POINT p) {
    RECT r = ViewRect();
    double m = std::max(0, std::min(50, g_set.hoverMargin)) / 100.0;
    int ox = sx_, oy = sy_;
    if (g_set.hoverMinMove > 0) {
        // relative mode: follow mouse movement, at least hoverMinMove image pixels per mouse pixel
        auto rate = [&](int range, int size) {
            double span = size * (1 - 2 * m);
            double k = span > 1 ? double(range) / span : double(range);
            return std::max(k, double(g_set.hoverMinMove));
        };
        sx_ += int(std::lround((p.x - lastPt_.x) * rate(DispW() - r.right, r.right)));
        sy_ += int(std::lround((p.y - lastPt_.y) * rate(DispH() - r.bottom, r.bottom)));
    } else {
        auto frac = [&](int v, int size) {
            double a = size * m, b = size * (1 - m);
            if (b <= a) return v < size / 2 ? 0.0 : 1.0;
            return std::max(0.0, std::min(1.0, (v - a) / (b - a)));
        };
        sx_ = int((DispW() - r.right) * frac(p.x, r.right));
        sy_ = int((DispH() - r.bottom) * frac(p.y, r.bottom));
    }
    ClampScroll();
    if (ox != sx_ || oy != sy_) { UpdateScrollBars(); Redraw(); }
}

// ------------------------------------------------------------------- render
void Viewer::Redraw() { if (view_) InvalidateRect(view_, nullptr, FALSE); }

void Viewer::EnsureCache() {
    if (cacheValid_ && cacheZoom_ == zoom_) return;
    cacheValid_ = false;
    cache_ = Image();
    if (zoom_ >= 1.0 || g_set.quality == 0) return;
    long long n = (long long)DispW() * DispH();
    if (n > 60000000LL) return;
    try {
        cache_ = Resize(img_, DispW(), DispH(), g_set.quality >= 2 ? ResizeQuality::Area : ResizeQuality::Bilinear);
        cacheValid_ = true; cacheZoom_ = zoom_;
    } catch (...) { cache_ = Image(); }
}

static inline uint32_t Blend(uint32_t fg, uint32_t bg) {
    uint32_t a = fg >> 24;
    if (a == 255) return fg | 0xFF000000;
    if (a == 0) return bg;
    uint32_t ia = 255 - a;
    uint32_t r = (((fg >> 16) & 255) * a + ((bg >> 16) & 255) * ia) / 255;
    uint32_t g = (((fg >> 8) & 255) * a + ((bg >> 8) & 255) * ia) / 255;
    uint32_t b = ((fg & 255) * a + (bg & 255) * ia) / 255;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void Viewer::RenderTo(uint32_t* dst, int vw, int vh, int stride) {
    COLORREF c1 = full_ ? g_set.fsBgColor : g_set.bgColor1, c2 = full_ ? g_set.fsBgColor : g_set.bgColor2;
    uint32_t b1 = 0xFF000000 | (GetRValue(c1) << 16) | (GetGValue(c1) << 8) | GetBValue(c1);
    uint32_t b2 = 0xFF000000 | (GetRValue(c2) << 16) | (GetGValue(c2) << 8) | GetBValue(c2);
    int cs = Scale(std::max(2, g_set.checkerSize), dpi_);
    bool checkerBg = g_set.bgChecker && !full_;
    // transparency checker shown under transparent pixels
    const uint32_t t1 = 0xFFFFFFFF, t2 = 0xFFCCCCCC;
    bool showT = g_set.showTransparency && img_.hasAlpha;
    POINT o = ImageOrigin();
    int dw = DispW(), dh = DispH();
    bool have = img_.valid();
    if (have) EnsureCache();
    bool useCache = have && cacheValid_;
    bool nearest = zoom_ >= 1.0 ? !g_set.smoothZoomIn || g_set.quality == 0 : true;
    double inv = have ? 1.0 / zoom_ : 1.0;
    RunParallel(vh, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            uint32_t* row = dst + size_t(y) * stride;
            int iy = y - o.y;
            bool inY = have && iy >= 0 && iy < dh;
            for (int x = 0; x < vw; ++x) {
                int ix = x - o.x;
                uint32_t bg = checkerBg ? ((((x / cs) + (y / cs)) & 1) ? b2 : b1) : b1;
                if (!inY || ix < 0 || ix >= dw) { row[x] = bg; continue; }
                uint32_t p;
                if (useCache) p = cache_.at(ix, iy);
                else if (zoom_ == 1.0) p = img_.at(ix, iy);
                else if (nearest) {
                    int sx = std::min(img_.w - 1, int(ix * inv)), sy = std::min(img_.h - 1, int(iy * inv));
                    p = img_.at(sx, sy);
                } else {
                    double fx = (ix + 0.5) * inv - 0.5, fy = (iy + 0.5) * inv - 0.5;
                    int x0 = int(std::floor(fx)), yy0 = int(std::floor(fy));
                    double ax = fx - x0, ay = fy - yy0;
                    int xa = std::max(0, std::min(img_.w - 1, x0)), xb = std::max(0, std::min(img_.w - 1, x0 + 1));
                    int ya = std::max(0, std::min(img_.h - 1, yy0)), yb = std::max(0, std::min(img_.h - 1, yy0 + 1));
                    uint32_t q00 = img_.at(xa, ya), q10 = img_.at(xb, ya), q01 = img_.at(xa, yb), q11 = img_.at(xb, yb);
                    uint32_t out = 0;
                    for (int sh = 0; sh < 32; sh += 8) {
                        double v = ((q00 >> sh) & 255) * (1 - ax) * (1 - ay) + ((q10 >> sh) & 255) * ax * (1 - ay) +
                                   ((q01 >> sh) & 255) * (1 - ax) * ay + ((q11 >> sh) & 255) * ax * ay;
                        out |= uint32_t(Clamp255(v)) << sh;
                    }
                    p = out;
                }
                if (showT && (p >> 24) != 255) {
                    int tc = Scale(8, dpi_);
                    uint32_t tb = (((ix / tc) + (iy / tc)) & 1) ? t2 : t1;
                    row[x] = Blend(p, tb);
                } else if ((p >> 24) != 255) row[x] = Blend(p, bg);
                else row[x] = p;
            }
        }
    });
}

void Viewer::Paint(HDC dc) {
    RECT r = ViewRect();
    int vw = r.right, vh = r.bottom;
    if (vw <= 0 || vh <= 0) return;
    void* bits = nullptr;
    HBITMAP bmp = CreateDib32(vw, vh, &bits);
    if (!bmp) return;
    RenderTo(static_cast<uint32_t*>(bits), vw, vh, vw);
    HDC mdc = CreateCompatibleDC(dc);
    HGDIOBJ ob = SelectObject(mdc, bmp);
    if (!img_.valid()) {
        SetBkMode(mdc, TRANSPARENT);
        COLORREF bg = full_ ? g_set.fsBgColor : g_set.bgColor1;
        int lum = (GetRValue(bg) * 3 + GetGValue(bg) * 6 + GetBValue(bg)) / 10;
        SetTextColor(mdc, lum > 128 ? RGB(60, 60, 60) : RGB(200, 200, 200));
        HGDIOBJ of = SelectObject(mdc, uiFont_);
        std::wstring msg = error_.empty() ? T("misc.dropfile") : error_;
        DrawTextW(mdc, msg.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(mdc, of);
    }
    if (selMode_) DrawSelection(mdc);
    BitBlt(dc, 0, 0, vw, vh, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, ob);
    DeleteDC(mdc);
    DeleteObject(bmp);
}

void Viewer::DrawSelection(HDC dc) {
    if (sel_.empty() || !img_.valid()) return;
    POINT o = ImageOrigin();
    RECT r = {o.x + int(std::lround(sel_.x * zoom_)), o.y + int(std::lround(sel_.y * zoom_)),
              o.x + int(std::lround(sel_.right() * zoom_)), o.y + int(std::lround(sel_.bottom() * zoom_))};
    HPEN p1 = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN p2 = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ op = SelectObject(dc, p1);
    SetBkMode(dc, OPAQUE); SetBkColor(dc, RGB(255, 255, 255));
    Rectangle(dc, r.left, r.top, r.right, r.bottom);
    SelectObject(dc, p2);
    Rectangle(dc, r.left, r.top, r.right, r.bottom);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(p1); DeleteObject(p2);
    std::wstring s = Fmt(L"%d, %d  %d x %d", sel_.x, sel_.y, sel_.w, sel_.h);
    SetBkMode(dc, OPAQUE); SetBkColor(dc, RGB(0, 0, 0)); SetTextColor(dc, RGB(255, 255, 255));
    HGDIOBJ of = SelectObject(dc, uiFont_);
    TextOutW(dc, r.left + 2, std::max<int>(0, r.top - Scale(18, dpi_)), s.c_str(), int(s.size()));
    SelectObject(dc, of);
}

// ---------------------------------------------------------------- view proc
LRESULT CALLBACK Viewer::ViewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) {
        auto* v = static_cast<Viewer*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, LONG_PTR(v));
        v->view_ = h;
    }
    auto* v = reinterpret_cast<Viewer*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    return v ? v->OnView(m, w, l) : DefWindowProcW(h, m, w, l);
}

bool Viewer::DispatchChord(const Chord& c) {
    int cmd = g_keymap.Lookup(c);
    if (!cmd) return false;
    Execute(cmd);
    return true;
}

static unsigned MouseMods(WPARAM w) {
    unsigned m = 0;
    if (w & MK_CONTROL) m |= ModCtrl;
    if (w & MK_SHIFT) m |= ModShift;
    if (GetKeyState(VK_MENU) < 0) m |= ModAlt;
    return m;
}

unsigned Viewer::HeldMods() const {
    unsigned m = 0;
    for (int b = 0; b < 5; ++b) if (btnDown_ & (1u << b)) m |= HoldBit(b);
    return m;
}

void Viewer::RunMouseCommand(int cmd, POINT clientPt) {
    if (cmd == CmdPopupMenu) { POINT s = clientPt; ClientToScreen(view_, &s); ShowPopup(nullptr, s); return; }
    if (cmd == CmdZoomIn || cmd == CmdZoomOut) { ZoomStep(cmd == CmdZoomIn ? 1 : -1, &clientPt); return; }
    Execute(cmd);
}

void Viewer::MouseDown(int b, POINT p, WPARAM w, bool dbl) {
    SetFocus(view_);
    static const unsigned dblCode[5] = {MLDbl, MRDbl, MMDbl, MX1Dbl, MX2Dbl};
    unsigned bit = 1u << b;
    unsigned heldBefore = HeldMods();
    btnDown_ |= bit;
    btnUsed_ &= ~bit;
    if (dbl && !(b == 0 && selMode_)) {
        int cmd = g_keymap.Lookup({MouseMods(w) | heldBefore, dblCode[b]});
        if (cmd) {
            btnUsed_ |= bit | (btnDown_ & ~bit);   // the second click must not fire a click
            SetCapture(view_);
            RunMouseCommand(cmd, p);
            return;
        }
    }
    if (dragBtn_ >= 0) { SetCapture(view_); return; }   // another button already drives a drag
    // first button down: it may become a drag
    dragBtn_ = b; dragging_ = false;
    downPt_ = lastPt_ = p;
    dragStartZoom_ = zoom_;
    POINT sp = p; ClientToScreen(view_, &sp);
    winStartScreen_ = sp; GetWindowRect(hwnd_, &winStartRect_);
    int act = g_set.dragAction[b];
    if (b == 0 && !selMode_ && ((w & MK_CONTROL) || act == DragMoveWindow) && !full_) {
        // left button: let the system move the window
        btnDown_ &= ~bit; dragBtn_ = -1;
        ReleaseCapture();
        SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return;
    }
    SetCapture(view_);
    if (act == DragScroll && selMode_ && img_.valid()) {
        double ix, iy; ViewToImage(p, ix, iy);
        int u = std::max(1, g_set.selUnit);
        selAx_ = std::max(0.0, std::min<double>(img_.w, std::floor(ix / u) * u));
        selAy_ = std::max(0.0, std::min<double>(img_.h, std::floor(iy / u) * u));
        selecting_ = true;
        btnUsed_ |= bit;
        sel_ = Rect{int(selAx_), int(selAy_), 0, 0};
        Redraw();
    }
}

void Viewer::MouseUp(int b, POINT p, WPARAM w) {
    static const unsigned clickCode[5] = {MLClick, MRClick, MMClick, MX1Click, MX2Click};
    unsigned bit = 1u << b;
    if (!(btnDown_ & bit)) return;
    btnDown_ &= ~bit;
    bool used = (btnUsed_ & bit) != 0;
    btnUsed_ &= ~bit;
    if (b == dragBtn_) {
        dragBtn_ = -1;
        bool wasDrag = dragging_;
        dragging_ = false;
        if (selecting_) {
            selecting_ = false;
            if (sel_.w == 0 || sel_.h == 0) sel_ = Rect{};
            Redraw(); UpdateStatus();
        }
        if (wasDrag) used = true;
    }
    if (!btnDown_ && GetCapture() == view_) ReleaseCapture();
    if (suppressClick_) { suppressClick_ = false; return; }
    if (used) return;
    unsigned held = HeldMods();
    int cmd = g_keymap.Lookup({MouseMods(w) | held, clickCode[b]});
    if (!cmd) return;
    btnUsed_ |= btnDown_;                // the still-held buttons acted as modifiers
    RunMouseCommand(cmd, p);
}

LRESULT Viewer::OnView(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC dc = BeginPaint(view_, &ps);
            Paint(dc);
            EndPaint(view_, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: return 0;
        case WM_MOUSEACTIVATE:
            if (g_set.activateNoClick && GetForegroundWindow() != hwnd_) { suppressClick_ = true; return MA_ACTIVATE; }
            break;
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: {
            int b = (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) ? 0 : (m == WM_RBUTTONDOWN || m == WM_RBUTTONDBLCLK) ? 1
                  : (m == WM_MBUTTONDOWN || m == WM_MBUTTONDBLCLK) ? 2 : (GET_XBUTTON_WPARAM(w) == XBUTTON1 ? 3 : 4);
            bool dbl = m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDBLCLK || m == WM_XBUTTONDBLCLK;
            MouseDown(b, POINT{GET_X_LPARAM(l), GET_Y_LPARAM(l)}, w, dbl);
            return (m == WM_XBUTTONDOWN || m == WM_XBUTTONDBLCLK) ? TRUE : 0;
        }
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: {
            int b = m == WM_LBUTTONUP ? 0 : m == WM_RBUTTONUP ? 1 : m == WM_MBUTTONUP ? 2 : (GET_XBUTTON_WPARAM(w) == XBUTTON1 ? 3 : 4);
            MouseUp(b, POINT{GET_X_LPARAM(l), GET_Y_LPARAM(l)}, w);
            return m == WM_XBUTTONUP ? TRUE : 0;
        }
        case WM_CAPTURECHANGED:
            if (HWND(l) != view_ && btnDown_) { btnDown_ = 0; btnUsed_ = 0; dragBtn_ = -1; dragging_ = false; selecting_ = false; }
            return 0;
        case WM_MOUSEMOVE: {
            POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            if (cursorHidden_ && (std::abs(p.x - lastPt_.x) > 2 || std::abs(p.y - lastPt_.y) > 2)) { ShowCursor(TRUE); cursorHidden_ = false; }
            lastMouseMove_ = GetTickCount();
            if (selecting_ && img_.valid()) {
                double ix, iy; ViewToImage(p, ix, iy);
                int u = std::max(1, g_set.selUnit);
                double bx = std::max(0.0, std::min<double>(img_.w, std::round(ix / u) * u));
                double by = std::max(0.0, std::min<double>(img_.h, std::round(iy / u) * u));
                sel_.x = int(std::min(selAx_, bx)); sel_.y = int(std::min(selAy_, by));
                sel_.w = int(std::fabs(bx - selAx_)); sel_.h = int(std::fabs(by - selAy_));
                Redraw();
                lastPt_ = p;
                return 0;
            }
            if (dragBtn_ >= 0) {
                int act = g_set.dragAction[dragBtn_];
                if (!dragging_ && (std::abs(p.x - downPt_.x) > g_set.dragMargin || std::abs(p.y - downPt_.y) > g_set.dragMargin) && act) {
                    dragging_ = true;
                    btnUsed_ |= 1u << dragBtn_;
                }
                if (dragging_) {
                    if (act == DragScroll) {
                        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                        int k = std::max(1, g_set.dragScrollRate);
                        ScrollBy((lastPt_.x - p.x) * k, (lastPt_.y - p.y) * k);
                    } else if (act == DragZoom) {
                        bool vert = g_set.dragZoomDir >= 2;
                        int d = vert ? p.y - downPt_.y : p.x - downPt_.x;
                        if (g_set.dragZoomDir == 1 || g_set.dragZoomDir == 2) d = -d;   // right->left, bottom->top
                        static const int perDouble[3] = {300, 150, 75};                 // slow / medium / fast
                        double z = dragStartZoom_ * std::pow(2.0, d / double(Scale(perDouble[std::max(0, std::min(2, g_set.dragZoomSpeed))], dpi_)));
                        POINT a = downPt_;
                        SetZoom(z, &a);
                    } else if (act == DragMoveWindow && !full_) {
                        POINT sp = p; ClientToScreen(view_, &sp);
                        SetWindowPos(hwnd_, nullptr, winStartRect_.left + sp.x - winStartScreen_.x, winStartRect_.top + sp.y - winStartScreen_.y,
                                     0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
            } else if (!btnDown_ && g_set.hoverScroll && img_.valid()) {
                HoverScroll(p);
            }
            lastPt_ = p;
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int d = GET_WHEEL_DELTA_WPARAM(w);
            WORD keys = GET_KEYSTATE_WPARAM(w);
            wheelAcc_ += d;
            int notch = g_set.wheelDull ? WHEEL_DELTA * 2 : WHEEL_DELTA;
            POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)}; ScreenToClient(view_, &p);
            while (std::abs(wheelAcc_) >= notch) {
                bool up = wheelAcc_ > 0;
                wheelAcc_ += up ? -notch : notch;
                unsigned held = HeldMods();
                int cmd = g_keymap.Lookup({MouseMods(keys) | held, up ? unsigned(MWheelUp) : unsigned(MWheelDown)});
                if (!cmd) continue;
                btnUsed_ |= btnDown_;            // buttons used as modifiers do not click on release
                RunMouseCommand(cmd, p);
            }
            return 0;
        }
        case WM_MOUSEHWHEEL: {
            int d = GET_WHEEL_DELTA_WPARAM(w);
            hwheelAcc_ += d;
            while (std::abs(hwheelAcc_) >= WHEEL_DELTA) {
                bool right = hwheelAcc_ > 0;
                hwheelAcc_ += right ? -WHEEL_DELTA : WHEEL_DELTA;
                if (!DispatchChord({MouseMods(GET_KEYSTATE_WPARAM(w)), right ? unsigned(MWheelRight) : unsigned(MWheelLeft)}))
                    ScrollBy(right ? Scale(g_set.wheelScroll * 3, dpi_) : -Scale(g_set.wheelScroll * 3, dpi_), 0);
            }
            return 0;
        }
        case WM_HSCROLL: case WM_VSCROLL: {
            bool horz = m == WM_HSCROLL;
            SCROLLINFO si = {sizeof(si)}; si.fMask = SIF_ALL;
            GetScrollInfo(view_, horz ? SB_HORZ : SB_VERT, &si);
            int pos = si.nPos, line = Scale(g_set.wheelScroll, dpi_);
            switch (LOWORD(w)) {
                case SB_LINEUP: pos -= line; break;
                case SB_LINEDOWN: pos += line; break;
                case SB_PAGEUP: pos -= int(si.nPage); break;
                case SB_PAGEDOWN: pos += int(si.nPage); break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
                case SB_TOP: pos = 0; break;
                case SB_BOTTOM: pos = si.nMax; break;
            }
            if (horz) sx_ = pos; else sy_ = pos;
            ClampScroll(); UpdateScrollBars(); Redraw();
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(l) == HTCLIENT) {
                if (cursorHidden_) { SetCursor(nullptr); return TRUE; }
                SetCursor(LoadCursor(nullptr, selMode_ ? IDC_CROSS : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_CONTEXTMENU:
            if (l == -1) { ShowPopup(nullptr, POINT{-1, -1}); return 0; }
            return 0;
    }
    return DefWindowProcW(view_, m, w, l);
}

// --------------------------------------------------------------- title/status
void Viewer::UpdateTitle() {
    std::wstring t;
    if (img_.valid() || !error_.empty()) {
        if (!untitled_.empty()) t = untitled_;
        else if (const FileEntry* e = list_.Current()) t = list_.InArchive() ? PathJoin(PathFileName(list_.ArchivePath()), e->path) : e->name;
        if (modified_) t += L" *";
        if (pages_ > 1 && !anim_.size()) t += Fmt(L" [%d/%d]", page_ + 1, pages_);
        if (fitRule_ && std::fabs(zoom_ - 1.0) > 1e-6) t += Fmt(L" <%ls %d%%>", T("title.fit").c_str(), int(std::lround(zoom_ * 100)));
        else if (std::fabs(zoom_ - 1.0) > 1e-6) t += Fmt(L" <%ls %d%%>", T("title.zoom").c_str(), int(std::lround(zoom_ * 100)));
        if (spreadCount_ == 2) t += L" <" + T("title.spread") + L">";
        if (list_.InArchive()) t += L" <" + T("title.archive") + L">";
        if (sliding_) t += L" <" + T("title.auto") + L">";
        if (img_.hasAlpha) t += L" <" + T("title.alpha") + L">";
        if (animating_) t += L" <" + T("title.anim") + L">";
        t += L" - ";
    }
    t += L"Lviewer";
    SetWindowTextW(hwnd_, t.c_str());
}

void Viewer::UpdateStatus() {
    if (!sb_ || !IsWindowVisible(sb_)) return;
    std::wstring p[6];
    if (img_.valid()) {
        p[0] = Fmt(L"%d x %d x %d", img_.w, img_.h, img_.srcBpp);
        if (list_.Count()) p[1] = Fmt(L"%d / %d", list_.Index() + 1, list_.Count());
        if (pages_ > 1) p[2] = Fmt(L"%ls %d/%d", T("status.page").c_str(), page_ + 1, pages_);
        p[3] = Fmt(L"%d%%", int(std::lround(zoom_ * 100)));
        if (const FileEntry* e = list_.Current()) {
            if (untitled_.empty()) {
                p[4] = FormatBytes(e->size) + L" / " + FormatBytes((unsigned long long)img_.w * img_.h * 4);
                p[5] = FormatFileTime(e->mtime);
            }
        }
        if (selMode_ && !sel_.empty()) p[5] = Fmt(L"%ls %d,%d %dx%d", T("status.sel").c_str(), sel_.x, sel_.y, sel_.w, sel_.h);
    } else if (!error_.empty()) p[5] = error_;
    for (int i = 0; i < 6; ++i) SendMessageW(sb_, SB_SETTEXTW, i, LPARAM(p[i].c_str()));
}

// ---------------------------------------------------------------- loading
void Viewer::ReleaseSource() { src_ = ImageSource(); srcOpen_ = false; }

void Viewer::StopAnimation() {
    if (animating_) KillTimer(hwnd_, TimerAnim);
    animating_ = false;
    anim_.clear(); delays_.clear(); animFrame_ = 0;
}

void Viewer::StartAnimation() {
    if (anim_.size() < 2) return;
    animating_ = true; animFrame_ = 0;
    SetTimer(hwnd_, TimerAnim, UINT(std::max(20, delays_[0])), nullptr);
}

void Viewer::ApplyAutoRotate(Image& im, int orient) {
    if (g_set.exifRotate) {
        switch (orient) {
            case 2: FlipH(im); break;
            case 3: Rotate180(im); break;
            case 4: FlipV(im); break;
            case 5: im = Rotate90(im, true); FlipH(im); break;
            case 6: im = Rotate90(im, true); break;
            case 7: im = Rotate90(im, false); FlipH(im); break;
            case 8: im = Rotate90(im, false); break;
        }
    }
    bool portrait = im.h > im.w, landscape = im.w > im.h;
    switch (g_set.rotOpt) {
        case RotLeft: im = Rotate90(im, false); break;
        case RotRight: im = Rotate90(im, true); break;
        case RotPortraitL: if (portrait) im = Rotate90(im, false); break;
        case RotPortraitR: if (portrait) im = Rotate90(im, true); break;
        case RotLandscapeL: if (landscape) im = Rotate90(im, false); break;
        case RotLandscapeR: if (landscape) im = Rotate90(im, true); break;
        case Rot180: Rotate180(im); break;
    }
}

bool Viewer::LoadEntry(int index, int page, Image& out, ImageSource& src, std::wstring* err) {
    if (index < 0 || index >= list_.Count()) return false;
    const FileEntry& e = list_.At(index);
    bool ok;
    if (list_.InArchive()) {
        std::vector<uint8_t> data;
        if (!list_.ReadArchiveEntry(index, data)) { if (err) *err = T("err.read"); return false; }
        ok = src.OpenMemory(std::move(data), err);
    } else ok = src.OpenFile(e.path, err);
    if (!ok) return false;
    if (page < 0) page = src.PageCount() - 1;
    int orient = 1;
    if (!src.LoadPage(std::max(0, std::min(page, src.PageCount() - 1)), out, &orient)) { if (err && err->empty()) *err = T("err.decode"); return false; }
    ApplyAutoRotate(out, orient);
    return true;
}

void Viewer::LoadCurrent(int page, bool fromNav) {
    StopAnimation();
    HCURSOR oc = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    error_.clear();
    untitled_.clear();
    modified_ = false;
    undo_.clear();
    sel_ = Rect{};
    spreadCount_ = 1;
    ReleaseSource();
    Image im;
    std::wstring err;
    bool ok = false;
    try {
        ok = LoadEntry(list_.Index(), page, im, src_, &err);
    } catch (const std::bad_alloc&) { err = T("err.memory"); ok = false; }
    if (ok) {
        pages_ = std::max(1, src_.PageCount());
        page_ = page < 0 ? pages_ - 1 : std::min(page, pages_ - 1);
        if (src_.IsAnimated()) {
            if (src_.LoadAnimation(anim_, delays_) && anim_.size() > 1) { im = anim_[0]; pages_ = 1; page_ = 0; }
            else anim_.clear();
        }
        img_ = std::move(im);
        if (g_set.comic && pages_ == 1) BuildSpread();
        // keep the decoder only when more pages may be requested (avoids locking the file)
        if (pages_ <= 1) ReleaseSource(); else srcOpen_ = true;
        if (!anim_.empty()) StartAnimation();
    } else {
        img_ = Image();
        pages_ = 1; page_ = 0;
        error_ = (list_.Current() ? list_.Current()->name + L": " : L"") + (err.empty() ? T("err.decode") : err);
    }
    if (list_.Current() && !list_.InArchive()) PushMru(g_set.mruFiles, list_.Current()->path);
    ApplyDisplayOption(!fromNav || !g_set.zoomLock);
    if (fromNav && g_set.zoomLock) { ClampScroll(); InvalidateCache(); }
    UpdateScrollBars();
    UpdateTitle();
    UpdateStatus();
    Redraw();
    UpdateWindow(view_);
    SetCursor(oc);
}

// Comic spread: put the next image beside the current one when both are portrait.
void Viewer::BuildSpread() {
    int i = list_.Index();
    if (i + 1 >= list_.Count() || !img_.valid()) return;
    if (g_set.comicLandscapeSingle && img_.w > img_.h) return;
    Image next; ImageSource s; std::wstring err;
    if (!LoadEntry(i + 1, 0, next, s, &err)) return;
    if (g_set.comicLandscapeSingle && next.w > next.h) return;
    int h = std::max(img_.h, next.h);
    Image both; both.copyMeta(img_);
    both.create(img_.w + next.w, h, 0xFF000000 | 0);
    COLORREF bgc = full_ ? g_set.fsBgColor : g_set.bgColor1;
    Pixel bg = ARGB(255, GetRValue(bgc), GetGValue(bgc), GetBValue(bgc));
    std::fill(both.px.begin(), both.px.end(), bg);
    const Image& L = g_set.comicRtoL ? next : img_;
    const Image& Rr = g_set.comicRtoL ? img_ : next;
    for (int y = 0; y < L.h; ++y) std::copy(L.row(y), L.row(y) + L.w, both.row(y + (h - L.h) / 2));
    for (int y = 0; y < Rr.h; ++y) std::copy(Rr.row(y), Rr.row(y) + Rr.w, both.row(y + (h - Rr.h) / 2) + L.w);
    both.hasAlpha = img_.hasAlpha || next.hasAlpha;
    if (img_.mode != next.mode) both.mode = ColorMode::Color;
    img_ = std::move(both);
    spreadCount_ = 2;
}

void Viewer::OpenPath(const std::wstring& path) {
    if (path.empty()) return;
    StartSlide(false);
    if (!list_.Open(path)) {
        img_ = Image(); error_ = T("err.open") + L": " + path;
        StopAnimation(); ReleaseSource();
        UpdateTitle(); UpdateStatus(); Redraw();
        return;
    }
    PushMru(g_set.mruDirs, list_.InArchive() ? list_.ArchivePath() : list_.Folder());
    LoadCurrent(0);
}

void Viewer::OpenFolder(const std::wstring& dir, bool last) {
    if (dir.empty()) { if (g_set.beepAtEnd) MessageBeep(MB_OK); return; }
    if (!list_.Open(dir)) return;
    if (last) list_.SetIndex(list_.Count() - 1);
    PushMru(g_set.mruDirs, dir);
    LoadCurrent(0);
}

bool Viewer::ConfirmDiscard() {
    if (!modified_ || !img_.valid()) return true;
    int r = MsgBox(hwnd_, T("msg.savechanges"), MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return false;
    if (r == IDYES) {
        if (!untitled_.empty() || list_.InArchive() || !list_.Current()) DoSaveAs();
        else DoSave();
        return !modified_;
    }
    modified_ = false;
    return true;
}

void Viewer::GoTo(int index, int page) {
    if (index < 0 || index >= list_.Count()) return;
    if (!ConfirmDiscard()) return;
    list_.SetIndex(index);
    LoadCurrent(page, true);
}

void Viewer::Navigate(int delta, bool usePages) {
    if (!list_.Count()) return;
    if (!ConfirmDiscard()) return;
    // multi-page TIFF stepping
    if (usePages && g_set.pagesInNav && pages_ > 1 && !error_.size()) {
        int np = page_ + delta;
        if (np >= 0 && np < pages_) {
            {
                Image im; int orient = 1;
                if (!srcOpen_) { std::wstring e; LoadEntry(list_.Index(), np, im, src_, &e); srcOpen_ = true; }
                else if (src_.LoadPage(np, im, &orient)) ApplyAutoRotate(im, orient);
                if (im.valid()) {
                    page_ = np; img_ = std::move(im); modified_ = false; undo_.clear(); sel_ = Rect{};
                    ApplyDisplayOption(!g_set.zoomLock);
                    UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
                }
            }
            return;
        }
    }
    int step = delta > 0 ? (spreadCount_ == 2 ? 2 : 1) : -1;
    if (delta < -1 || delta > 1) step = delta;
    int n = list_.Count(), i = list_.Index() + step;
    bool wrapped = false;
    if (i >= n || i < 0) {
        if (std::abs(step) > 1) i = std::max(0, std::min(n - 1, i));
        else if (g_set.loopList && !sliding_) { i = (i + n) % n; wrapped = true; }
        else if (sliding_ && g_set.slideLoop) { i = (i + n) % n; wrapped = true; }
        else { if (g_set.beepAtEnd) MessageBeep(MB_OK); return; }
    }
    if (g_set.comic && step < 0 && i > 0 && spreadCount_ <= 2) {
        // when going back in comic mode, try to land on the left page of a spread
    }
    (void)wrapped;
    if (i == list_.Index() && pages_ <= 1) return;
    list_.SetIndex(i);
    int page = (delta < 0 && usePages && g_set.pagesInNav) ? -1 : 0;
    LoadCurrent(page, true);
}

void Viewer::ShowError(const std::wstring& msg) { MsgBox(hwnd_, msg, MB_ICONWARNING); }

// -------------------------------------------------------------- fullscreen
void Viewer::ToggleFullScreen() {
    if (!full_) {
        prevPlacement_.length = sizeof(prevPlacement_);
        GetWindowPlacement(hwnd_, &prevPlacement_);
        prevStyle_ = DWORD(GetWindowLongPtrW(hwnd_, GWL_STYLE));
        HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
        full_ = true;
        SetMenu(hwnd_, nullptr);
        SetWindowLongPtrW(hwnd_, GWL_STYLE, (prevStyle_ & ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER)) | WS_POPUP);
        SetWindowLongPtrW(view_, GWL_EXSTYLE, GetWindowLongPtrW(view_, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
        SetWindowPos(view_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        SetTimer(hwnd_, TimerCursor, 500, nullptr);
        lastMouseMove_ = GetTickCount();
    } else {
        full_ = false;
        KillTimer(hwnd_, TimerCursor);
        if (cursorHidden_) { ShowCursor(TRUE); cursorHidden_ = false; }
        SetWindowLongPtrW(hwnd_, GWL_STYLE, prevStyle_);
        ApplyWindowStyles();
        SetWindowPlacement(hwnd_, &prevPlacement_);
        SetWindowPos(hwnd_, g_set.topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    }
    Layout();
    if (img_.valid()) {
        if (spreadCount_ == 2) BuildSpread();
        ApplyDisplayOption(true);
    }
    UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
}

void Viewer::SetTopMost(bool on) {
    g_set.topMost = on;
    SetWindowPos(hwnd_, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void Viewer::StartSlide(bool on) {
    if (on == sliding_) return;
    sliding_ = on;
    if (on) {
        if (g_set.slideFull && !full_) ToggleFullScreen();
        SetTimer(hwnd_, TimerSlide, UINT(std::max(100, g_set.slideMs)), nullptr);
    } else {
        KillTimer(hwnd_, TimerSlide);
    }
    UpdateTitle();
}

} // namespace lv
