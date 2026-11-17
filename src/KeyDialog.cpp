// Lviewer - key assignment dialog and jump (file picker) dialog
#include "Viewer.h"
#include "I18n.h"
#include "Settings.h"
#include "Form.h"
#include "core/NaturalSort.h"

namespace lv {

namespace {

// Small modal window helper shared by both dialogs
struct Modal {
    HWND hwnd = nullptr, owner = nullptr;
    bool done = false, ok = false;
    HFONT font = nullptr;
    int dpi = 96;
    std::function<LRESULT(UINT, WPARAM, LPARAM, bool&)> handler;

    static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        Modal* d = reinterpret_cast<Modal*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) {
            d = static_cast<Modal*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, LONG_PTR(d));
            d->hwnd = h;
        }
        if (d && d->handler) {
            bool handled = false;
            LRESULT r = d->handler(m, w, l, handled);
            if (handled) return r;
        }
        if (m == WM_CLOSE && d) { d->done = true; return 0; }
        return DefWindowProcW(h, m, w, l);
    }
    bool Create(HWND own, const std::wstring& title, int w, int h) {
        static bool reg = false;
        if (!reg) {
            WNDCLASSEXW wc = {sizeof(wc)};
            wc.lpfnWndProc = Proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"LviewerModal";
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
            RegisterClassExW(&wc); reg = true;
        }
        owner = own;
        dpi = DpiOf(own);
        font = CreateUiFont(dpi);
        RECT orc; GetWindowRect(own, &orc);
        int W = Scale(w, dpi), H = Scale(h, dpi);
        CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"LviewerModal", title.c_str(),
                        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN,
                        (orc.left + orc.right - W) / 2, (orc.top + orc.bottom - H) / 2, W, H, own, nullptr, GetModuleHandleW(nullptr), this);
        return hwnd != nullptr;
    }
    HWND Ctl(const wchar_t* cls, const std::wstring& text, DWORD style, DWORD ex, int id) {
        HWND c = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, WM_SETFONT, WPARAM(font), TRUE);
        return c;
    }
    bool Run(std::function<bool(MSG&)> pre = nullptr) {
        EnableWindow(owner, FALSE);
        ShowWindow(hwnd, SW_SHOW);
        MSG msg;
        while (!done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (pre && pre(msg)) continue;
            if (IsDialogMessageW(hwnd, &msg)) continue;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        EnableWindow(owner, TRUE);
        DestroyWindow(hwnd);
        DeleteObject(font);
        SetForegroundWindow(owner);
        return ok;
    }
};

std::vector<int> CommandsInMenuOrder() {
    std::vector<int> v;
    std::function<void(const MenuNode&)> walk = [&](const MenuNode& n) {
        for (auto& k : n.kids) {
            if (k.cmd > 0) v.push_back(k.cmd);
            else if (k.dynamic == 3) for (int i = 0; i < 9; ++i) v.push_back(CmdApp1 + i);
            walk(k);
        }
    };
    walk(MenuTree());
    for (auto& c : AllCommands()) if (std::find(v.begin(), v.end(), c.id) == v.end()) v.push_back(c.id);
    return v;
}

} // namespace

// ================================================================== keys
void Viewer::DlgKeys() {
    auto backup = g_set.keys;
    Modal d;
    if (!d.Create(hwnd_, T("cmd.keysettings"), 620, 520)) return;
    enum { IdList = 100, IdChords, IdCapture, IdAdd, IdRemove, IdDefault, IdResetAll, IdMouse, IdMouseAdd, IdOK, IdCancel, IdHint };
    HWND list = d.Ctl(WC_LISTVIEWW, L"", WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, IdList);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH;
    std::wstring c0 = T("keys.command"), c1 = T("keys.keys");
    col.pszText = &c0[0]; col.cx = Scale(300, d.dpi); ListView_InsertColumn(list, 0, &col);
    col.pszText = &c1[0]; col.cx = Scale(260, d.dpi); ListView_InsertColumn(list, 1, &col);
    HWND chords = d.Ctl(WC_COMBOBOXW, L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, IdChords);
    HWND hint = d.Ctl(L"STATIC", T("keys.capturehint"), SS_LEFT, 0, IdHint);
    HWND cap = d.Ctl(L"EDIT", L"", WS_TABSTOP | ES_READONLY | ES_CENTER, WS_EX_CLIENTEDGE, IdCapture);
    HWND add = d.Ctl(L"BUTTON", T("keys.add"), WS_TABSTOP | BS_PUSHBUTTON, 0, IdAdd);
    HWND rem = d.Ctl(L"BUTTON", T("keys.remove"), WS_TABSTOP | BS_PUSHBUTTON, 0, IdRemove);
    HWND mouse = d.Ctl(WC_COMBOBOXW, L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, IdMouse);
    HWND madd = d.Ctl(L"BUTTON", T("keys.addmouse"), WS_TABSTOP | BS_PUSHBUTTON, 0, IdMouseAdd);
    HWND def = d.Ctl(L"BUTTON", T("keys.default"), WS_TABSTOP | BS_PUSHBUTTON, 0, IdDefault);
    HWND all = d.Ctl(L"BUTTON", T("keys.resetall"), WS_TABSTOP | BS_PUSHBUTTON, 0, IdResetAll);
    HWND okb = d.Ctl(L"BUTTON", T("btn.ok"), WS_TABSTOP | BS_DEFPUSHBUTTON, 0, IdOK);
    HWND cnb = d.Ctl(L"BUTTON", T("btn.cancel"), WS_TABSTOP | BS_PUSHBUTTON, 0, IdCancel);
    static const unsigned mouseCodes[] = {MWheelUp, MWheelDown, MWheelLeft, MWheelRight, MLClick, MLDbl, MRClick, MRDbl, MMClick, MMDbl, MX1Click, MX1Dbl, MX2Click, MX2Dbl};
    for (unsigned mc : mouseCodes) {
        std::wstring s = ChordToDisplay({0, mc});
        SendMessageW(mouse, CB_ADDSTRING, 0, LPARAM(s.c_str()));
    }
    SendMessageW(mouse, CB_SETCURSEL, 0, 0);

    std::vector<int> cmds = CommandsInMenuOrder();
    Chord captured;
    auto keysText = [](int cmd) {
        std::wstring s;
        for (auto& c : g_keymap.ChordsOf(cmd)) { if (!s.empty()) s += L", "; s += ChordToDisplay(c); }
        return s;
    };
    auto fill = [&] {
        int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        SendMessageW(list, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(list);
        for (size_t i = 0; i < cmds.size(); ++i) {
            std::wstring label = CommandLabel(cmds[i]);
            if (cmds[i] >= CmdApp1 && cmds[i] <= CmdApp9) label = Fmt(L"%ls %d", StripMnemonic(T("menu.apps")).c_str(), cmds[i] - CmdApp1 + 1);
            LVITEMW it = {}; it.mask = LVIF_TEXT | LVIF_PARAM; it.iItem = int(i); it.pszText = &label[0]; it.lParam = cmds[i];
            ListView_InsertItem(list, &it);
            std::wstring k = keysText(cmds[i]);
            ListView_SetItemText(list, int(i), 1, &k[0]);
        }
        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
        if (sel >= 0) { ListView_SetItemState(list, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); ListView_EnsureVisible(list, sel, FALSE); }
    };
    auto current = [&]() -> int {
        int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        return sel >= 0 && sel < int(cmds.size()) ? cmds[sel] : 0;
    };
    auto fillChords = [&] {
        SendMessageW(chords, CB_RESETCONTENT, 0, 0);
        for (auto& c : g_keymap.ChordsOf(current())) { std::wstring s = ChordToDisplay(c); SendMessageW(chords, CB_ADDSTRING, 0, LPARAM(s.c_str())); }
        SendMessageW(chords, CB_SETCURSEL, 0, 0);
    };
    auto assign = [&](const Chord& c) {
        int cmd = current();
        if (!cmd || !c.code) return;
        int other = g_keymap.Lookup(c);
        if (other && other != cmd) {
            std::wstring q = Fmt(T("keys.conflict").c_str(), ChordToDisplay(c).c_str(), CommandLabel(other).c_str());
            if (MessageBoxW(d.hwnd, q.c_str(), L"Lviewer", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        }
        g_keymap.Bind(c, cmd);
        fill(); fillChords();
    };
    auto layout = [&] {
        RECT rc; GetClientRect(d.hwnd, &rc);
        int p = Scale(8, d.dpi), bh = Scale(26, d.dpi), bw = Scale(120, d.dpi), W = rc.right, H = rc.bottom;
        int y3 = H - p - bh, y2 = y3 - p - bh, y1 = y2 - p - bh, y0 = y1 - p - bh;
        MoveWindow(list, p, p, W - 2 * p, y0 - 2 * p, TRUE);
        MoveWindow(hint, p, y0 + Scale(4, d.dpi), W - 2 * p, bh, TRUE);
        MoveWindow(cap, p, y1, W - 3 * p - bw * 2, bh, TRUE);
        MoveWindow(add, W - 2 * p - bw * 2, y1, bw, bh, TRUE);
        MoveWindow(chords, p, y2, W - 3 * p - bw * 2, bh * 8, TRUE);
        MoveWindow(rem, W - 2 * p - bw * 2, y2, bw, bh, TRUE);
        MoveWindow(mouse, W - p - bw, y1, bw, bh * 10, TRUE);
        MoveWindow(madd, W - p - bw, y2, bw, bh, TRUE);
        MoveWindow(def, p, y3, bw, bh, TRUE);
        MoveWindow(all, p * 2 + bw, y3, bw, bh, TRUE);
        MoveWindow(okb, W - 2 * p - bw * 2, y3, bw, bh, TRUE);
        MoveWindow(cnb, W - p - bw, y3, bw, bh, TRUE);
    };
    d.handler = [&](UINT m, WPARAM w, LPARAM l, bool& handled) -> LRESULT {
        if (m == WM_SIZE) { layout(); handled = true; return 0; }
        if (m == WM_NOTIFY) {
            auto* nm = reinterpret_cast<NMHDR*>(l);
            if (nm->hwndFrom == list && nm->code == LVN_ITEMCHANGED) fillChords();
        }
        if (m == WM_COMMAND) {
            int id = LOWORD(w);
            handled = true;
            switch (id) {
                case IdAdd: assign(captured); SetFocus(cap); return 0;
                case IdMouseAdd: {
                    int i = int(SendMessageW(mouse, CB_GETCURSEL, 0, 0));
                    if (i >= 0) assign({CurrentMods(), mouseCodes[i]});
                    return 0;
                }
                case IdRemove: {
                    int cmd = current(); int i = int(SendMessageW(chords, CB_GETCURSEL, 0, 0));
                    auto v = g_keymap.ChordsOf(cmd);
                    if (cmd && i >= 0 && i < int(v.size())) { v.erase(v.begin() + i); g_keymap.SetChords(cmd, v); fill(); fillChords(); }
                    return 0;
                }
                case IdDefault: {
                    int cmd = current(); const lv::CmdInfo* ci = FindCommand(cmd);
                    if (ci) { std::vector<Chord> v; for (auto& s : Split(Utf8ToW(ci->defKeys), L',')) { Chord c; if (ParseChord(s, c)) v.push_back(c); } g_keymap.SetChords(cmd, v); fill(); fillChords(); }
                    return 0;
                }
                case IdResetAll:
                    if (MessageBoxW(d.hwnd, T("keys.resetallq").c_str(), L"Lviewer", MB_YESNO | MB_ICONQUESTION) == IDYES) { g_keymap.ResetAll(); fill(); fillChords(); }
                    return 0;
                case IdOK: d.ok = true; d.done = true; return 0;
                case IdCancel: d.done = true; return 0;
            }
            handled = false;
        }
        return 0;
    };
    fill();
    ListView_SetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    layout();
    bool ok = d.Run([&](MSG& msg) {
        if (msg.hwnd == cap && (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)) {
            unsigned vk = unsigned(msg.wParam);
            if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) return true;
            captured = {CurrentMods(), vk};
            SetWindowTextW(cap, ChordToDisplay(captured).c_str());
            return true;
        }
        if (msg.hwnd == cap && (msg.message == WM_CHAR || msg.message == WM_SYSCHAR || msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP)) return true;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && msg.hwnd != cap) { d.done = true; return true; }
        return false;
    });
    if (!ok) { g_set.keys = backup; g_keymap.Build(); }
    else g_set.Save();
    RebuildUi();
}

// ================================================================== jump
void Viewer::DlgJump() {
    if (!list_.Count()) return;
    Modal d;
    if (!d.Create(hwnd_, T("cmd.jump"), 480, 520)) return;
    enum { IdFilter = 100, IdList, IdOK, IdCancel, IdLabel };
    HWND lbl = d.Ctl(L"STATIC", T("jump.filter"), SS_LEFT, 0, IdLabel);
    HWND filter = d.Ctl(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IdFilter);
    HWND lb = d.Ctl(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE, IdList);
    HWND okb = d.Ctl(L"BUTTON", T("btn.ok"), WS_TABSTOP | BS_DEFPUSHBUTTON, 0, IdOK);
    HWND cnb = d.Ctl(L"BUTTON", T("btn.cancel"), WS_TABSTOP | BS_PUSHBUTTON, 0, IdCancel);
    std::vector<int> shown;
    int result = -1;
    auto fill = [&] {
        int n = GetWindowTextLengthW(filter);
        std::wstring f(n + 1, 0); GetWindowTextW(filter, &f[0], n + 1); f.resize(n);
        std::wstring lf = ToLower(f);
        SendMessageW(lb, WM_SETREDRAW, FALSE, 0);
        SendMessageW(lb, LB_RESETCONTENT, 0, 0);
        shown.clear();
        int selRow = 0;
        for (int i = 0; i < list_.Count(); ++i) {
            std::wstring name = list_.At(i).name;
            if (!lf.empty() && ToLower(name).find(lf) == std::wstring::npos && Fmt(L"%d", i + 1) != f) continue;
            if (i == list_.Index()) selRow = int(shown.size());
            shown.push_back(i);
            std::wstring row = Fmt(L"%5d  ", i + 1) + name;
            SendMessageW(lb, LB_ADDSTRING, 0, LPARAM(row.c_str()));
        }
        SendMessageW(lb, WM_SETREDRAW, TRUE, 0);
        SendMessageW(lb, LB_SETCURSEL, lf.empty() ? selRow : 0, 0);
    };
    auto layout = [&] {
        RECT rc; GetClientRect(d.hwnd, &rc);
        int p = Scale(8, d.dpi), bh = Scale(26, d.dpi), bw = Scale(100, d.dpi);
        int lw = Scale(90, d.dpi);
        MoveWindow(lbl, p, p + Scale(4, d.dpi), lw, bh, TRUE);
        MoveWindow(filter, p * 2 + lw, p, rc.right - p * 3 - lw, bh, TRUE);
        MoveWindow(lb, p, p * 2 + bh, rc.right - 2 * p, rc.bottom - bh * 2 - p * 4, TRUE);
        MoveWindow(okb, rc.right - p * 2 - bw * 2, rc.bottom - p - bh, bw, bh, TRUE);
        MoveWindow(cnb, rc.right - p - bw, rc.bottom - p - bh, bw, bh, TRUE);
    };
    auto choose = [&] {
        int s = int(SendMessageW(lb, LB_GETCURSEL, 0, 0));
        if (s >= 0 && s < int(shown.size())) { result = shown[s]; d.ok = true; }
        d.done = true;
    };
    d.handler = [&](UINT m, WPARAM w, LPARAM l, bool& handled) -> LRESULT {
        if (m == WM_SIZE) { layout(); handled = true; return 0; }
        if (m == WM_COMMAND) {
            int id = LOWORD(w), code = HIWORD(w);
            if (id == IdFilter && code == EN_CHANGE) { fill(); handled = true; return 0; }
            if (id == IdList && code == LBN_DBLCLK) { choose(); handled = true; return 0; }
            if (id == IdOK) { choose(); handled = true; return 0; }
            if (id == IdCancel) { d.done = true; handled = true; return 0; }
        }
        return 0;
    };
    fill();
    layout();
    SetFocus(filter);
    d.Run([&](MSG& msg) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN) { choose(); return true; }
            if (msg.wParam == VK_ESCAPE) { d.done = true; return true; }
            if (msg.hwnd == filter && (msg.wParam == VK_DOWN || msg.wParam == VK_UP || msg.wParam == VK_NEXT || msg.wParam == VK_PRIOR)) {
                SendMessageW(lb, WM_KEYDOWN, msg.wParam, msg.lParam); return true;
            }
        }
        return false;
    });
    if (result >= 0) GoTo(result);
}

} // namespace lv
