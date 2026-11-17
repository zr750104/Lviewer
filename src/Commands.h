#pragma once
#include "Win.h"
#include <map>

namespace lv {

enum Cmd : int {
    CmdNone = 0,
    CmdFirst = 1000,
#define X(id, name, keys) Cmd##id,
#include "CommandList.inc"
#undef X
    CmdLast,
    CmdMruDir0 = 9000,      // .. 9009
    CmdMruFile0 = 9100,     // .. 9109
};

struct CmdInfo { int id; const char* name; const char* defKeys; };
const std::vector<CmdInfo>& AllCommands();
const CmdInfo* FindCommand(int id);
int CommandByName(const std::wstring& name);
std::wstring CommandLabel(int id);           // localized, without '&'
std::wstring StripMnemonic(const std::wstring& s);   // removes '&' and trailing "(&X)"

// ---- key / mouse chords -----------------------------------------------------
enum : unsigned { ModCtrl = 1, ModShift = 2, ModAlt = 4 };
enum : unsigned {   // pseudo virtual-key codes for mouse input
    MouseBase = 0x1000,
    MWheelUp = MouseBase + 1, MWheelDown, MWheelLeft, MWheelRight,
    MLClick, MLDbl, MRClick, MRDbl, MMClick, MMDbl, MX1Click, MX1Dbl, MX2Click, MX2Dbl,
};
struct Chord {
    unsigned mods = 0, code = 0;
    bool operator<(const Chord& o) const { return mods != o.mods ? mods < o.mods : code < o.code; }
    bool operator==(const Chord& o) const { return mods == o.mods && code == o.code; }
    bool isMouse() const { return code >= MouseBase; }
};
bool ParseChord(const std::wstring& s, Chord& out);
std::wstring ChordToString(const Chord& c);           // canonical (INI) form
std::wstring ChordToDisplay(const Chord& c);          // for menus
unsigned CurrentMods();

class Keymap {
public:
    void Build();                                      // defaults + g_set.keys overrides
    int Lookup(const Chord& c) const;
    std::vector<Chord> ChordsOf(int cmd) const;
    std::wstring DisplayKeysOf(int cmd) const;         // first keyboard chord, for menu text
    void SetChords(int cmd, const std::vector<Chord>& chords);   // writes override to g_set.keys
    void Bind(const Chord& c, int cmd);                          // move one chord to cmd (0 = unbind)
    void ResetAll();
private:
    std::map<Chord, int> map_;
    std::map<int, std::vector<Chord>> rev_;
};
extern Keymap g_keymap;

// ---- menu definition ----------------------------------------------------------
struct MenuNode {
    const char* label = nullptr;   // lang key for sub-menus (nullptr for command items)
    int cmd = 0;                   // -1 = separator; 0 with kids = submenu
    std::vector<MenuNode> kids;
    int dynamic = 0;               // 1 = MRU folders, 2 = MRU files, 3 = other apps
};
const MenuNode& MenuTree();        // top level: File, Edit, Image, List, View, Other
const MenuNode* FindSubmenu(const char* label);

} // namespace lv
