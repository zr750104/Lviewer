#include "Form.h"
#include "I18n.h"
#include "Commands.h"
#include <commdlg.h>

namespace lv {

static const wchar_t* kFormClass = L"LviewerForm";
enum { IdOk = 1, IdCancel = 2, IdTab = 50, IdPreview = 51, IdBase = 100 };

Form::Form(std::wstring title) : title_(StripMnemonic(title)) {
    while (title_.size() > 3 && title_.compare(title_.size() - 3, 3, L"...") == 0) title_.resize(title_.size() - 3);
}
Form::~Form() { if (font_) DeleteObject(font_); }

static int CurPage(const std::vector<std::wstring>& pages) { return pages.empty() ? 0 : int(pages.size()) - 1; }

void Form::Page(const std::wstring& name) { pages_.push_back(name); }
void Form::Label(const std::wstring& t) { Item it; it.k = K::Label; it.text = t; it.page = CurPage(pages_); items_.push_back(it); }
void Form::Edit(const std::wstring& l, std::wstring* v, int chars) { Item it; it.k = K::Edit; it.label = l; it.s = v; it.chars = chars; it.page = CurPage(pages_); items_.push_back(it); }
void Form::Int(const std::wstring& l, int* v, int lo, int hi, const std::wstring& unit) {
    Item it; it.k = K::Int; it.label = l; it.i = v; it.lo = lo; it.hi = hi; it.unit = unit; it.chars = 8; it.page = CurPage(pages_); items_.push_back(it);
}
void Form::Slider(const std::wstring& l, int* v, int lo, int hi, int div) {
    Item it; it.k = K::Slider; it.label = l; it.i = v; it.lo = lo; it.hi = hi; it.div = std::max(1, div); it.chars = 30; it.page = CurPage(pages_); items_.push_back(it);
}
void Form::Check(const std::wstring& l, bool* v) { Item it; it.k = K::Check; it.label = l; it.b = v; it.page = CurPage(pages_); items_.push_back(it); }
void Form::Combo(const std::wstring& l, int* v, const std::vector<std::wstring>& items) {
    Item it; it.k = K::Combo; it.label = l; it.i = v; it.items = items; it.page = CurPage(pages_); items_.push_back(it);
}
void Form::Color(const std::wstring& l, COLORREF* v) { Item it; it.k = K::Color; it.label = l; it.c = v; it.page = CurPage(pages_); items_.push_back(it); }
void Form::Path(const std::wstring& l, std::wstring* v, bool folder, int chars) {
    Item it; it.k = K::Path; it.label = l; it.s = v; it.folder = folder; it.chars = chars; it.page = CurPage(pages_); items_.push_back(it);
}
void Form::Text(const std::wstring& l, const std::wstring& text, int lines, int chars) {
    Item it; it.k = K::Text; it.label = l; it.text = text; it.lines = lines; it.chars = chars; it.page = CurPage(pages_); items_.push_back(it);
}
void Form::Button(const std::wstring& t, std::function<void()> fn) { Item it; it.k = K::Button; it.text = t; it.fn = std::move(fn); it.page = CurPage(pages_); items_.push_back(it); }

static SIZE TextSize(HDC dc, const std::wstring& s) {
    SIZE sz = {0, 0};
    for (auto& line : Split(s, L'\n')) {
        SIZE l = {0, 0};
        GetTextExtentPoint32W(dc, line.c_str(), int(line.size()), &l);
        sz.cx = std::max(sz.cx, l.cx);
        sz.cy += std::max<LONG>(l.cy, 1);
    }
    return sz;
}

void Form::UpdateSliderText(Item& it) {
    int v = int(SendMessageW(it.hCtl, TBM_GETPOS, 0, 0));
    std::wstring s = it.div == 1 ? Fmt(L"%d", v) : Fmt(L"%.2f", double(v) / it.div);
    SetWindowTextW(it.hAux, s.c_str());
}

void Form::Build() {
    building_ = true;
    HINSTANCE hi = GetModuleHandleW(nullptr);
    dpi_ = DpiOf(owner_);
    font_ = CreateUiFont(dpi_);
    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, font_);
    TEXTMETRICW tm; GetTextMetricsW(dc, &tm);
    int ch = tm.tmAveCharWidth, lh = tm.tmHeight;
    int pad = Scale(10, dpi_), gap = Scale(6, dpi_), rowH = lh + Scale(10, dpi_);
    int labelW = 0, ctlW = Scale(160, dpi_);
    for (auto& it : items_) {
        if (it.k == K::Label || it.k == K::Button) { ctlW = std::max<int>(ctlW, TextSize(dc, it.text).cx - labelW); continue; }
        if (it.k == K::Check) { ctlW = std::max<int>(ctlW, TextSize(dc, it.label).cx + Scale(24, dpi_)); continue; }
        labelW = std::max<int>(labelW, TextSize(dc, it.label).cx + Scale(4, dpi_));
        int w = 0;
        switch (it.k) {
            case K::Edit: case K::Path: case K::Text: w = ch * it.chars; break;
            case K::Int: w = ch * 10 + TextSize(dc, it.unit).cx + gap; break;
            case K::Slider: w = Scale(220, dpi_) + ch * 7; break;
            case K::Combo: { for (auto& s : it.items) w = std::max<int>(w, TextSize(dc, s).cx); w += Scale(32, dpi_); } break;
            case K::Color: w = Scale(80, dpi_); break;
            default: break;
        }
        if (it.k == K::Path) w += Scale(32, dpi_) + gap;
        ctlW = std::max(ctlW, w);
    }
    int left = pad, top = pad;
    int tabTop = 0;
    if (pages_.size() > 1) {
        tab_ = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP, 0, 0, 10, 10, hwnd_, (HMENU)IdTab, hi, nullptr);
        SendMessageW(tab_, WM_SETFONT, WPARAM(font_), TRUE);
        for (size_t i = 0; i < pages_.size(); ++i) {
            TCITEMW t = {}; t.mask = TCIF_TEXT; t.pszText = const_cast<wchar_t*>(pages_[i].c_str());
            SendMessageW(tab_, TCM_INSERTITEMW, i, LPARAM(&t));
        }
        tabTop = top;
        top += lh + Scale(14, dpi_);
        left += pad;
    }
    int x2 = left + (labelW ? labelW + gap * 2 : 0);
    int maxY = top;
    int np = std::max<int>(1, int(pages_.size()));
    int id = IdBase;
    for (int p = 0; p < np; ++p) {
        int y = top;
        for (auto& it : items_) {
            if (it.page != p) continue;
            DWORD vis = (p == 0) ? WS_VISIBLE : 0;
            auto mk = [&](const wchar_t* cls, const std::wstring& txt, DWORD st, DWORD ex, int x, int yy, int w, int h) {
                HWND c = CreateWindowExW(ex, cls, txt.c_str(), WS_CHILD | vis | st, x, yy, w, h, hwnd_, (HMENU)(INT_PTR)id++, hi, nullptr);
                SendMessageW(c, WM_SETFONT, WPARAM(font_), TRUE);
                return c;
            };
            int cy = y + (rowH - lh) / 2 - Scale(1, dpi_);
            int ctlH = lh + Scale(8, dpi_);
            if (it.k != K::Label && it.k != K::Check && it.k != K::Button && !it.label.empty())
                it.hLabel = mk(L"STATIC", it.label, SS_LEFT | SS_NOPREFIX, 0, left, cy + Scale(3, dpi_), labelW, lh);
            switch (it.k) {
                case K::Label: {
                    SIZE s = TextSize(dc, it.text);
                    it.hCtl = mk(L"STATIC", it.text, SS_LEFT | SS_NOPREFIX, 0, left, y + Scale(2, dpi_), std::max<int>(s.cx, labelW + ctlW), s.cy);
                    y += s.cy + Scale(8, dpi_);
                    continue;
                }
                case K::Edit:
                    it.hCtl = mk(L"EDIT", *it.s, WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, x2, y, ctlW, ctlH); break;
                case K::Path:
                    it.hCtl = mk(L"EDIT", *it.s, WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, x2, y, ctlW - Scale(32, dpi_) - gap, ctlH);
                    it.hAux = mk(L"BUTTON", L"...", WS_TABSTOP | BS_PUSHBUTTON, 0, x2 + ctlW - Scale(32, dpi_), y, Scale(32, dpi_), ctlH); break;
                case K::Int: {
                    it.hCtl = mk(L"EDIT", Fmt(L"%d", *it.i), WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, WS_EX_CLIENTEDGE, x2, y, ch * 10, ctlH);
                    if (it.lo < 0) SetWindowLongPtrW(it.hCtl, GWL_STYLE, GetWindowLongPtrW(it.hCtl, GWL_STYLE) & ~ES_NUMBER);
                    HWND ud = mk(UPDOWN_CLASSW, L"", UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0, 0);
                    SendMessageW(ud, UDM_SETBUDDY, WPARAM(it.hCtl), 0);
                    SendMessageW(ud, UDM_SETRANGE32, it.lo, it.hi);
                    SendMessageW(ud, UDM_SETPOS32, 0, *it.i);
                    if (!it.unit.empty()) it.hAux = mk(L"STATIC", it.unit, SS_LEFT | SS_NOPREFIX, 0, x2 + ch * 10 + gap, cy + Scale(3, dpi_), TextSize(dc, it.unit).cx + 2, lh);
                } break;
                case K::Slider:
                    it.hCtl = mk(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 0, x2, y, ctlW - ch * 7, ctlH);
                    SendMessageW(it.hCtl, TBM_SETRANGEMIN, FALSE, it.lo);
                    SendMessageW(it.hCtl, TBM_SETRANGEMAX, FALSE, it.hi);
                    SendMessageW(it.hCtl, TBM_SETPAGESIZE, 0, std::max(1, (it.hi - it.lo) / 20));
                    SendMessageW(it.hCtl, TBM_SETPOS, TRUE, *it.i);
                    it.hAux = mk(L"STATIC", L"", SS_RIGHT, 0, x2 + ctlW - ch * 7, cy + Scale(3, dpi_), ch * 7, lh);
                    UpdateSliderText(it);
                    break;
                case K::Check:
                    it.hCtl = mk(L"BUTTON", it.label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, x2, y, ctlW, ctlH);
                    SendMessageW(it.hCtl, BM_SETCHECK, *it.b ? BST_CHECKED : BST_UNCHECKED, 0);
                    break;
                case K::Combo: {
                    it.hCtl = mk(WC_COMBOBOXW, L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, x2, y, ctlW, ctlH * 12);
                    for (auto& s : it.items) SendMessageW(it.hCtl, CB_ADDSTRING, 0, LPARAM(s.c_str()));
                    SendMessageW(it.hCtl, CB_SETCURSEL, *it.i, 0);
                } break;
                case K::Color:
                    it.hCtl = mk(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 0, x2, y, Scale(80, dpi_), ctlH); break;
                case K::Text: {
                    int h = lh * it.lines + Scale(8, dpi_);
                    std::wstring t = it.text;
                    std::wstring crlf;
                    for (wchar_t c : t) { if (c == L'\n') crlf += L"\r\n"; else crlf += c; }
                    it.hCtl = mk(L"EDIT", crlf, WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | WS_HSCROLL,
                                 WS_EX_CLIENTEDGE, it.label.empty() ? left : x2, y, it.label.empty() ? labelW + ctlW + gap * 2 : ctlW, h);
                    y += h + gap;
                    continue;
                }
                case K::Button: {
                    SIZE s = TextSize(dc, it.text);
                    it.hCtl = mk(L"BUTTON", it.text, WS_TABSTOP | BS_PUSHBUTTON, 0, x2, y, std::max<int>(s.cx + Scale(24, dpi_), Scale(90, dpi_)), ctlH);
                } break;
            }
            y += rowH;
        }
        maxY = std::max(maxY, y);
    }
    int contentW = x2 + ctlW - (pages_.size() > 1 ? pad : 0);
    if (labelW == 0) contentW = left + ctlW;
    int btnW = Scale(88, dpi_), btnH = lh + Scale(12, dpi_);
    for (const std::wstring* s : {&T("btn.ok"), &T("btn.cancel"), &T("btn.preview")}) btnW = std::max<int>(btnW, TextSize(dc, *s).cx + Scale(24, dpi_));
    int by = maxY + Scale(8, dpi_);
    if (tab_) {
        MoveWindow(tab_, pad, tabTop, contentW + pad - pad / 2, maxY - tabTop + Scale(6, dpi_), FALSE);
        by += Scale(10, dpi_);
    }
    int prevW = previewToggle ? TextSize(dc, T("btn.preview")).cx + Scale(30, dpi_) : 0;
    int totalW = std::max(contentW + pad + (tab_ ? pad : 0), btnW * 2 + gap + pad * 2 + (previewToggle ? prevW + gap * 2 : 0));
    ok_ = CreateWindowExW(0, L"BUTTON", T("btn.ok").c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          totalW - pad - btnW * 2 - gap, by, btnW, btnH, hwnd_, (HMENU)IdOk, hi, nullptr);
    cancel_ = CreateWindowExW(0, L"BUTTON", T("btn.cancel").c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              totalW - pad - btnW, by, btnW, btnH, hwnd_, (HMENU)IdCancel, hi, nullptr);
    SendMessageW(ok_, WM_SETFONT, WPARAM(font_), TRUE);
    SendMessageW(cancel_, WM_SETFONT, WPARAM(font_), TRUE);
    if (previewToggle) {
        prev_ = CreateWindowExW(0, L"BUTTON", T("btn.preview").c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                pad, by, prevW, btnH, hwnd_, (HMENU)IdPreview, hi, nullptr);
        SendMessageW(prev_, WM_SETFONT, WPARAM(font_), TRUE);
        preview_ = previewFlag ? *previewFlag : true;
        SendMessageW(prev_, BM_SETCHECK, preview_ ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);
    RECT rc = {0, 0, totalW, by + btnH + pad};
    DWORD st = DWORD(GetWindowLongPtrW(hwnd_, GWL_STYLE)), ex = DWORD(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE));
    AdjustWindowRectEx(&rc, st, FALSE, ex);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    RECT orc; GetWindowRect(owner_ ? owner_ : GetDesktopWindow(), &orc);
    HMONITOR mon = MonitorFromWindow(owner_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
    int x = (orc.left + orc.right - w) / 2, y = (orc.top + orc.bottom - h) / 2;
    x = std::max<int>(mi.rcWork.left, std::min<int>(x, mi.rcWork.right - w));
    y = std::max<int>(mi.rcWork.top, std::min<int>(y, mi.rcWork.bottom - h));
    SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER);
    building_ = false;
}

void Form::ShowPage(int p) {
    for (auto& it : items_) {
        int sw = it.page == p ? SW_SHOW : SW_HIDE;
        for (HWND h : {it.hLabel, it.hCtl, it.hAux}) if (h) ShowWindow(h, sw);
        if (it.k == K::Int && it.hCtl) {   // up-down follows its buddy
            HWND ud = GetWindow(it.hCtl, GW_HWNDNEXT);
            if (ud) ShowWindow(ud, sw);
        }
    }
}

void Form::ReadControls() {
    for (auto& it : items_) {
        if (!it.hCtl) continue;
        switch (it.k) {
            case K::Edit: case K::Path: {
                int n = GetWindowTextLengthW(it.hCtl);
                std::wstring s(n + 1, 0); GetWindowTextW(it.hCtl, &s[0], n + 1); s.resize(n);
                *it.s = s;
            } break;
            case K::Int: {
                wchar_t b[64]; GetWindowTextW(it.hCtl, b, 64);
                int v = _wtoi(b);
                *it.i = std::max(it.lo, std::min(it.hi, v));
            } break;
            case K::Slider: *it.i = int(SendMessageW(it.hCtl, TBM_GETPOS, 0, 0)); break;
            case K::Check: *it.b = SendMessageW(it.hCtl, BM_GETCHECK, 0, 0) == BST_CHECKED; break;
            case K::Combo: { LRESULT s = SendMessageW(it.hCtl, CB_GETCURSEL, 0, 0); if (s >= 0) *it.i = int(s); } break;
            default: break;
        }
    }
}

void Form::Reload() {
    building_ = true;
    for (auto& it : items_) {
        if (!it.hCtl) continue;
        switch (it.k) {
            case K::Edit: case K::Path: SetWindowTextW(it.hCtl, it.s->c_str()); break;
            case K::Int: SetWindowTextW(it.hCtl, Fmt(L"%d", *it.i).c_str()); break;
            case K::Slider: SendMessageW(it.hCtl, TBM_SETPOS, TRUE, *it.i); UpdateSliderText(it); break;
            case K::Check: SendMessageW(it.hCtl, BM_SETCHECK, *it.b ? BST_CHECKED : BST_UNCHECKED, 0); break;
            case K::Combo: SendMessageW(it.hCtl, CB_SETCURSEL, *it.i, 0); break;
            case K::Color: InvalidateRect(it.hCtl, nullptr, TRUE); break;
            default: break;
        }
    }
    building_ = false;
}

void Form::Changed() {
    if (building_) return;
    ReadControls();
    if (onChange && preview_) onChange();
}

void Form::Restore() {
    for (auto& it : items_) {
        if (it.s) *it.s = it.s0;
        if (it.i) *it.i = it.i0;
        if (it.b) *it.b = it.b0;
        if (it.c) *it.c = it.c0;
    }
}

LRESULT CALLBACK Form::Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Form* f = reinterpret_cast<Form*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        f = static_cast<Form*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, LONG_PTR(f));
        f->hwnd_ = h;
    }
    return f ? f->Handle(m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT Form::Handle(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_COMMAND: {
            int id = LOWORD(w), code = HIWORD(w);
            if (id == IdOk) {
                ReadControls();
                if (onValidate && !onValidate()) return 0;
                result_ = true; done_ = true; return 0;
            }
            if (id == IdCancel) { done_ = true; result_ = false; return 0; }
            if (id == IdPreview) {
                preview_ = SendMessageW(prev_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (previewFlag) *previewFlag = preview_;
                ReadControls();
                if (onChange) onChange();
                return 0;
            }
            for (auto& it : items_) {
                if (HWND(l) == it.hAux && it.k == K::Path && code == BN_CLICKED) {
                    std::wstring cur = *it.s;
                    Com<IFileDialog> dlg;
                    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dlg.put())))) {
                        DWORD o = 0; dlg->GetOptions(&o);
                        dlg->SetOptions(o | FOS_FORCEFILESYSTEM | (it.folder ? FOS_PICKFOLDERS : 0));
                        if (dlg->Show(hwnd_) == S_OK) {
                            Com<IShellItem> si;
                            if (SUCCEEDED(dlg->GetResult(si.put()))) {
                                wchar_t* p = nullptr;
                                if (SUCCEEDED(si->GetDisplayName(SIGDN_FILESYSPATH, &p))) { SetWindowTextW(it.hCtl, p); CoTaskMemFree(p); }
                            }
                        }
                    }
                    Changed();
                    return 0;
                }
                if (HWND(l) != it.hCtl) continue;
                if (it.k == K::Button && code == BN_CLICKED) { ReadControls(); if (it.fn) it.fn(); Reload(); return 0; }
                if (it.k == K::Color && code == BN_CLICKED) {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc = {sizeof(cc)}; cc.hwndOwner = hwnd_; cc.rgbResult = *it.c; cc.lpCustColors = custom;
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) { *it.c = cc.rgbResult; InvalidateRect(it.hCtl, nullptr, TRUE); Changed(); }
                    return 0;
                }
                if ((it.k == K::Check && code == BN_CLICKED) || (it.k == K::Combo && code == CBN_SELCHANGE) ||
                    ((it.k == K::Edit || it.k == K::Int || it.k == K::Path) && code == EN_CHANGE))
                    Changed();
                return 0;
            }
        } break;
        case WM_HSCROLL:
            for (auto& it : items_) if (HWND(l) == it.hCtl && it.k == K::Slider) { UpdateSliderText(it); Changed(); }
            return 0;
        case WM_DRAWITEM: {
            auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            for (auto& it : items_) if (di->hwndItem == it.hCtl && it.k == K::Color) {
                DrawFrameControl(di->hDC, &di->rcItem, DFC_BUTTON, DFCS_BUTTONPUSH | ((di->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0));
                RECT r = di->rcItem; InflateRect(&r, -Scale(5, dpi_), -Scale(5, dpi_));
                HBRUSH b = CreateSolidBrush(*it.c); FillRect(di->hDC, &r, b); DeleteObject(b);
                FrameRect(di->hDC, &r, (HBRUSH)GetStockObject(GRAY_BRUSH));
                if (di->itemState & ODS_FOCUS) { InflateRect(&r, Scale(2, dpi_), Scale(2, dpi_)); DrawFocusRect(di->hDC, &r); }
                return TRUE;
            }
        } break;
        case WM_NOTIFY: {
            auto* nm = reinterpret_cast<NMHDR*>(l);
            if (nm->hwndFrom == tab_ && nm->code == TCN_SELCHANGE) ShowPage(int(SendMessageW(tab_, TCM_GETCURSEL, 0, 0)));
            if (nm->code == UDN_DELTAPOS) PostMessageW(hwnd_, WM_APP + 1, 0, 0);
        } break;
        case WM_APP + 1: Changed(); return 0;
        case WM_CTLCOLORSTATIC: {
            // controls on a tab page sit on the tab body (window colour); others on the dialog face
            int idx = tab_ ? COLOR_WINDOW : COLOR_BTNFACE;
            HWND c = HWND(l);
            wchar_t cls[16] = {};
            GetClassNameW(c, cls, 16);
            if (!lstrcmpiW(cls, L"EDIT")) idx = (GetWindowLongPtrW(c, GWL_STYLE) & ES_READONLY) ? COLOR_BTNFACE : COLOR_WINDOW;
            SetBkColor(HDC(w), GetSysColor(idx));
            SetTextColor(HDC(w), GetSysColor(COLOR_WINDOWTEXT));
            return LRESULT(GetSysColorBrush(idx));
        }
        case WM_CLOSE: done_ = true; result_ = false; return 0;
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

bool Form::Show(HWND owner) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = Proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = kFormClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        RegisterClassExW(&wc);
        reg = true;
    }
    owner_ = owner;
    for (auto& it : items_) {
        if (it.s) it.s0 = *it.s;
        if (it.i) it.i0 = *it.i;
        if (it.b) it.b0 = *it.b;
        if (it.c) it.c0 = *it.c;
    }
    DWORD ex = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT | (GetWindowLongPtrW(owner, GWL_EXSTYLE) & WS_EX_TOPMOST);
    CreateWindowExW(ex, kFormClass, title_.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                    0, 0, 100, 100, owner, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;
    Build();
    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(hwnd_, SW_SHOW);
    // focus first input
    for (auto& it : items_) if (it.hCtl && it.k != K::Label && it.page == 0) { SetFocus(it.hCtl); if (it.k == K::Edit || it.k == K::Int) SendMessageW(it.hCtl, EM_SETSEL, 0, -1); break; }
    if (onChange && preview_) onChange();
    MSG msg;
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
            wchar_t cls[32] = {}; GetClassNameW(msg.hwnd, cls, 32);
            bool multi = !lstrcmpiW(cls, L"EDIT") && (GetWindowLongPtrW(msg.hwnd, GWL_STYLE) & ES_MULTILINE) && msg.wParam == VK_RETURN;
            bool comboOpen = !lstrcmpiW(cls, WC_COMBOBOXW) && SendMessageW(msg.hwnd, CB_GETDROPPEDSTATE, 0, 0);
            if (!multi && !comboOpen && (msg.hwnd == hwnd_ || IsChild(hwnd_, msg.hwnd))) {
                SendMessageW(hwnd_, WM_COMMAND, msg.wParam == VK_RETURN ? IdOk : IdCancel, 0);
                continue;
            }
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0 && tab_) {
            int n = int(SendMessageW(tab_, TCM_GETITEMCOUNT, 0, 0));
            int c = int(SendMessageW(tab_, TCM_GETCURSEL, 0, 0));
            c = (c + (GetKeyState(VK_SHIFT) < 0 ? n - 1 : 1)) % n;
            SendMessageW(tab_, TCM_SETCURSEL, c, 0);
            ShowPage(c);
            continue;
        }
        if (IsDialogMessageW(hwnd_, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (!result_) { Restore(); if (onChange) onChange(); }
    if (owner) EnableWindow(owner, TRUE);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    if (owner) SetForegroundWindow(owner);
    return result_;
}

bool PromptText(HWND owner, const std::wstring& title, const std::wstring& label, std::wstring& value) {
    Form f(title);
    f.Edit(label, &value, 40);
    return f.Show(owner);
}
bool PromptInt(HWND owner, const std::wstring& title, const std::wstring& label, int& value, int lo, int hi) {
    Form f(title);
    f.Int(label, &value, lo, hi);
    return f.Show(owner);
}
int MsgBox(HWND owner, const std::wstring& text, UINT flags) {
    return MessageBoxW(owner, text.c_str(), L"Lviewer", flags);
}

} // namespace lv
