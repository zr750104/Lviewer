#include "Commands.h"
#include "I18n.h"
#include "Settings.h"
#include <cstring>

namespace lv {

Keymap g_keymap;

const std::vector<CmdInfo>& AllCommands() {
    static const std::vector<CmdInfo> v = {
#define X(id, name, keys) {Cmd##id, name, keys},
#include "CommandList.inc"
#undef X
    };
    return v;
}

const CmdInfo* FindCommand(int id) {
    if (id <= CmdFirst || id >= CmdLast) return nullptr;
    return &AllCommands()[id - CmdFirst - 1];
}

int CommandByName(const std::wstring& name) {
    std::string n = WToUtf8(name);
    for (auto& c : AllCommands()) if (n == c.name) return c.id;
    return 0;
}

std::wstring StripMnemonic(const std::wstring& s) {
    std::wstring o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'(' && i + 3 < s.size() + 1 && i + 2 < s.size() && s[i + 1] == L'&' && i + 3 < s.size() && s[i + 3] == L')') { i += 3; continue; }
        if (s[i] == L'&') { if (i + 1 < s.size() && s[i + 1] == L'&') { o += L'&'; ++i; } continue; }
        o += s[i];
    }
    return Trim(o);
}

std::wstring CommandLabel(int id) {
    const CmdInfo* c = FindCommand(id);
    if (!c) return L"";
    return StripMnemonic(T((std::string("cmd.") + c->name).c_str()));
}

// ---------------------------------------------------------------- key names
namespace {
struct KeyName { unsigned vk; const wchar_t* name; };
const KeyName kKeyNames[] = {
    {VK_BACK, L"BackSpace"}, {VK_TAB, L"Tab"}, {VK_RETURN, L"Enter"}, {VK_PAUSE, L"Pause"},
    {VK_ESCAPE, L"Esc"}, {VK_SPACE, L"Space"}, {VK_PRIOR, L"PageUp"}, {VK_NEXT, L"PageDown"},
    {VK_END, L"End"}, {VK_HOME, L"Home"}, {VK_LEFT, L"Left"}, {VK_UP, L"Up"}, {VK_RIGHT, L"Right"},
    {VK_DOWN, L"Down"}, {VK_INSERT, L"Insert"}, {VK_DELETE, L"Delete"}, {VK_APPS, L"Apps"},
    {VK_NUMPAD0, L"Num0"}, {VK_NUMPAD1, L"Num1"}, {VK_NUMPAD2, L"Num2"}, {VK_NUMPAD3, L"Num3"},
    {VK_NUMPAD4, L"Num4"}, {VK_NUMPAD5, L"Num5"}, {VK_NUMPAD6, L"Num6"}, {VK_NUMPAD7, L"Num7"},
    {VK_NUMPAD8, L"Num8"}, {VK_NUMPAD9, L"Num9"}, {VK_MULTIPLY, L"Num*"}, {VK_ADD, L"Num+"},
    {VK_SUBTRACT, L"Num-"}, {VK_DECIMAL, L"Num."}, {VK_DIVIDE, L"Num/"},
    {VK_OEM_1, L"OEM1"}, {VK_OEM_PLUS, L"OEMPlus"}, {VK_OEM_COMMA, L","}, {VK_OEM_MINUS, L"OEMMinus"},
    {VK_OEM_PERIOD, L"."}, {VK_OEM_2, L"/"}, {VK_OEM_3, L"OEM3"}, {VK_OEM_4, L"["}, {VK_OEM_5, L"\\"},
    {VK_OEM_6, L"]"}, {VK_OEM_7, L"OEM7"}, {VK_OEM_102, L"OEM102"},
    {MWheelUp, L"WheelUp"}, {MWheelDown, L"WheelDown"}, {MWheelLeft, L"WheelLeft"}, {MWheelRight, L"WheelRight"},
    {MLClick, L"LClick"}, {MLDbl, L"LDbl"}, {MRClick, L"RClick"}, {MRDbl, L"RDbl"},
    {MMClick, L"MClick"}, {MMDbl, L"MDbl"}, {MX1Click, L"X1Click"}, {MX1Dbl, L"X1Dbl"},
    {MX2Click, L"X2Click"}, {MX2Dbl, L"X2Dbl"},
};
} // namespace

unsigned CurrentMods() {
    unsigned m = 0;
    if (GetKeyState(VK_CONTROL) < 0) m |= ModCtrl;
    if (GetKeyState(VK_SHIFT) < 0) m |= ModShift;
    if (GetKeyState(VK_MENU) < 0) m |= ModAlt;
    return m;
}

bool ParseChord(const std::wstring& in, Chord& out) {
    std::wstring s = Trim(in);
    out = Chord{};
    if (s.empty()) return false;
    for (;;) {
        size_t p = s.find(L'+');
        if (p == std::wstring::npos || p == 0 || p + 1 >= s.size()) break;
        std::wstring m = ToLower(s.substr(0, p));
        if (m == L"ctrl") out.mods |= ModCtrl;
        else if (m == L"shift") out.mods |= ModShift;
        else if (m == L"alt") out.mods |= ModAlt;
        else break;
        s = s.substr(p + 1);
    }
    for (auto& k : kKeyNames) if (lstrcmpiW(k.name, s.c_str()) == 0) { out.code = k.vk; return true; }
    if (s.size() == 1) {
        wchar_t c = s[0];
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) { out.code = c; return true; }
        if (c >= L'a' && c <= L'z') { out.code = c - L'a' + L'A'; return true; }
        SHORT r = VkKeyScanW(c);         // layout dependent (e.g. ';' on JIS keyboards)
        if (r != -1) { out.code = r & 0xFF; if (r & 0x100) out.mods |= ModShift; return true; }
        return false;
    }
    if ((s[0] == L'F' || s[0] == L'f') && s.size() <= 3) {
        int n = _wtoi(s.c_str() + 1);
        if (n >= 1 && n <= 24) { out.code = VK_F1 + n - 1; return true; }
    }
    if (s.compare(0, 2, L"VK") == 0) { out.code = wcstoul(s.c_str() + 2, nullptr, 16); return out.code != 0; }
    return false;
}

static std::wstring CodeName(unsigned code) {
    for (auto& k : kKeyNames) if (k.vk == code) return k.name;
    if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9')) return std::wstring(1, wchar_t(code));
    if (code >= VK_F1 && code <= VK_F24) return Fmt(L"F%u", code - VK_F1 + 1);
    return Fmt(L"VK%02X", code);
}

std::wstring ChordToString(const Chord& c) {
    std::wstring s;
    if (c.mods & ModCtrl) s += L"Ctrl+";
    if (c.mods & ModShift) s += L"Shift+";
    if (c.mods & ModAlt) s += L"Alt+";
    return s + CodeName(c.code);
}

std::wstring ChordToDisplay(const Chord& c) {
    std::wstring s;
    if (c.mods & ModCtrl) s += L"Ctrl+";
    if (c.mods & ModShift) s += L"Shift+";
    if (c.mods & ModAlt) s += L"Alt+";
    unsigned code = c.code;
    if (code < MouseBase) {
        UINT ch = MapVirtualKeyW(code, MAPVK_VK_TO_CHAR) & 0x7FFF;
        if (code >= VK_OEM_1 && code <= VK_OEM_102 && ch >= 0x21) return s + std::wstring(1, wchar_t(ch));
        if (code == VK_LEFT) return s + L"←";
        if (code == VK_RIGHT) return s + L"→";
        if (code == VK_UP) return s + L"↑";
        if (code == VK_DOWN) return s + L"↓";
        return s + CodeName(code);
    }
    return s + T((std::string("mouse.") + WToUtf8(CodeName(code))).c_str());
}

void Keymap::Build() {
    map_.clear(); rev_.clear();
    for (auto& c : AllCommands()) {
        std::wstring keys = Utf8ToW(c.defKeys);
        auto it = g_set.keys.find(Utf8ToW(c.name));
        if (it != g_set.keys.end()) keys = it->second;
        for (auto& part : Split(keys, L',')) {
            Chord ch;
            if (!ParseChord(part, ch)) continue;
            if (map_.count(ch)) continue;     // first binding wins
            map_[ch] = c.id;
            rev_[c.id].push_back(ch);
        }
    }
}

int Keymap::Lookup(const Chord& c) const {
    auto it = map_.find(c);
    return it == map_.end() ? 0 : it->second;
}

std::vector<Chord> Keymap::ChordsOf(int cmd) const {
    auto it = rev_.find(cmd);
    return it == rev_.end() ? std::vector<Chord>{} : it->second;
}

std::wstring Keymap::DisplayKeysOf(int cmd) const {
    for (auto& c : ChordsOf(cmd)) if (!c.isMouse()) return ChordToDisplay(c);
    return L"";
}

void Keymap::SetChords(int cmd, const std::vector<Chord>& chords) {
    const CmdInfo* ci = FindCommand(cmd);
    if (!ci) return;
    std::wstring s;
    for (auto& c : chords) { if (!s.empty()) s += L","; s += ChordToString(c); }
    if (s == Utf8ToW(ci->defKeys)) g_set.keys.erase(Utf8ToW(ci->name));
    else g_set.keys[Utf8ToW(ci->name)] = s;
    // remove the chords from other commands to keep bindings unique
    for (auto& c : chords) {
        int other = Lookup(c);
        if (other && other != cmd) {
            auto v = ChordsOf(other);
            v.erase(std::remove(v.begin(), v.end(), c), v.end());
            const CmdInfo* oi = FindCommand(other);
            std::wstring os;
            for (auto& x : v) { if (!os.empty()) os += L","; os += ChordToString(x); }
            g_set.keys[Utf8ToW(oi->name)] = os;
        }
    }
    Build();
}

void Keymap::Bind(const Chord& c, int cmd) {
    int cur = Lookup(c);
    if (cur == cmd) return;
    if (cur) {
        auto v = ChordsOf(cur);
        v.erase(std::remove(v.begin(), v.end(), c), v.end());
        SetChords(cur, v);
    }
    if (cmd) {
        auto v = ChordsOf(cmd);
        v.push_back(c);
        SetChords(cmd, v);
    }
}

void Keymap::ResetAll() { g_set.keys.clear(); Build(); }

// ------------------------------------------------------------------ menu tree
static MenuNode C(int cmd) { MenuNode n; n.cmd = cmd; return n; }
static MenuNode Sep() { MenuNode n; n.cmd = -1; return n; }
static MenuNode Sub(const char* label, std::vector<MenuNode> kids) { MenuNode n; n.label = label; n.kids = std::move(kids); return n; }
static MenuNode Dyn(const char* label, int kind) { MenuNode n; n.label = label; n.dynamic = kind; return n; }

const MenuNode& MenuTree() {
    static const MenuNode root = Sub("menu.root", {
        Sub("menu.file", {
            C(CmdOpen), C(CmdReload), Sep(), C(CmdRename), C(CmdMove), C(CmdCopyFile), C(CmdDelete), Sep(),
            C(CmdSave), C(CmdSaveAs), Sep(), C(CmdPrint), Sep(),
            Dyn("menu.mrudirs", 1), Dyn("menu.mrufiles", 2), Sep(),
            Sub("menu.newwnd", {C(CmdNewWndBlank), Sep(), C(CmdNewWndCurrent), C(CmdNewWndSendClip), Sep(), C(CmdNewWndPasteClip)}),
            Dyn("menu.apps", 3), Sep(), C(CmdExit)}),
        Sub("menu.edit", {
            C(CmdUndo), Sep(), C(CmdSelectMode), C(CmdSelectDialog),
            Sub("menu.selunit", {C(CmdSelUnitFree), C(CmdSelUnit2), C(CmdSelUnit4), C(CmdSelUnit8), C(CmdSelUnit16), C(CmdSelUnit32), C(CmdSelUnitCustom)}),
            Sep(), C(CmdCut), C(CmdCopy), C(CmdPaste), Sep(), C(CmdCropSel), Sep(),
            C(CmdNewImage), C(CmdPasteNew), C(CmdCloseImage), Sep(),
            Sub("menu.copyname", {C(CmdCopyName), C(CmdCopyFullPath), Sep(), C(CmdCopyFolderName), C(CmdCopyFolderPath), Sep(), C(CmdCopyShortPath)})}),
        Sub("menu.image", {
            C(CmdImageInfo), Sep(), C(CmdResize), C(CmdCanvasSize),
            Sub("menu.rotate", {C(CmdRotL), C(CmdRotR), Sep(), C(CmdFlipV), C(CmdFlipH), Sep(), C(CmdRotAngle)}),
            Sep(), C(CmdAutoContrast), C(CmdAutoLevels), Sep(), C(CmdLevels), C(CmdBrightness), C(CmdHueSat), Sep(),
            C(CmdNegative), C(CmdGrayscale),
            Sub("menu.sharpblur", {C(CmdSharpen), C(CmdSharpenStrong), C(CmdSharpenEdge), C(CmdUnsharp), Sep(),
                                   C(CmdBlurN), C(CmdBlurStrong), C(CmdBlurSoft), C(CmdGaussian)}),
            Sub("menu.mask", {C(CmdMaskGlassV), C(CmdMaskGlassH), C(CmdMaskNega), C(CmdMaskXor), C(CmdMaskRGB),
                              C(CmdMaskQ0), C(CmdMaskWin), Sep(), C(CmdMaskOptions)}),
            Sep(), C(CmdColorDepth),
            Sub("menu.swap", {C(CmdSwapRG), C(CmdSwapRB), C(CmdSwapGB), C(CmdSwapRot), C(CmdSwapRotRev)})}),
        Sub("menu.list", {
            C(CmdPrevFile), C(CmdNextFile), C(CmdSkipPrev), C(CmdSkipNext), C(CmdFirstFile), C(CmdLastFile), Sep(),
            C(CmdPrevPage), C(CmdNextPage), Sep(),
            C(CmdPrevFolder), C(CmdNextFolder), C(CmdPrevSibling), C(CmdNextSibling), C(CmdParentFolder), Sep(),
            C(CmdJump), C(CmdSlide), Sep(), C(CmdRefresh),
            Sub("menu.sort", {C(CmdSortName), C(CmdSortDate), C(CmdSortSize), C(CmdSortExt), C(CmdSortRandom), Sep(),
                              C(CmdSortReverse), C(CmdSortNumeric)})}),
        Sub("menu.view", {
            C(CmdFullScreen),
            Sub("menu.window", {C(CmdWndRestore), C(CmdWndMinimize), C(CmdWndMaximize), Sep(), C(CmdWndTopMost), Sep(),
                                C(CmdWndCaption), C(CmdWndOuterFrame), C(CmdWndInnerFrame), C(CmdWndMenuBar),
                                C(CmdWndToolBar), C(CmdWndStatusBar), C(CmdWndScrollBar)}),
            Sep(),
            Sub("menu.disp", {C(CmdDisp1), C(CmdDisp2), C(CmdDisp3), C(CmdDisp4), C(CmdDisp5), C(CmdDisp0)}),
            Sub("menu.fit", {C(CmdFitInside), C(CmdFitWidth), C(CmdFitHeight), C(CmdFitEither)}),
            Sub("menu.rotopt", {C(CmdRotOptNone), Sep(), C(CmdRotOptL), C(CmdRotOptR), Sep(), C(CmdRotOptPortL), C(CmdRotOptPortR), Sep(),
                                C(CmdRotOptLandL), C(CmdRotOptLandR), Sep(), C(CmdRotOpt180)}),
            Sep(), C(CmdZoomIn), C(CmdZoomOut), C(CmdZoomDialog), C(CmdZoomLock), Sep(),
            C(CmdOriginal), C(CmdFitWindow), Sep(), C(CmdHoverScroll), Sep(), C(CmdComic),
            Sub("menu.comic", {C(CmdComicBack1), C(CmdComicFwd1), Sep(), C(CmdComicRtoL), C(CmdComicLtoR), Sep(), C(CmdComicLandSingle)}),
            Sep(),
            Sub("menu.lang", {C(CmdLangJa), C(CmdLangEn), C(CmdLangTh)})}),
        Sub("menu.other", {
            C(CmdPreferences), C(CmdKeySettings), C(CmdMouseSettings), C(CmdFormats), C(CmdAssociate), C(CmdCheckUpdate), Sep(),
            Sub("menu.wall", {C(CmdWallCenter), C(CmdWallTile), C(CmdWallStretch), Sep(), C(CmdWallFitW), C(CmdWallFitH), Sep(), C(CmdWallDelete)}),
            Sep(), C(CmdHelp), Sep(), C(CmdAbout)}),
    });
    return root;
}

static const MenuNode* FindIn(const MenuNode& n, const char* label) {
    if (n.label && strcmp(n.label, label) == 0) return &n;
    for (auto& k : n.kids) if (auto f = FindIn(k, label)) return f;
    return nullptr;
}
const MenuNode* FindSubmenu(const char* label) { return FindIn(MenuTree(), label); }

} // namespace lv
