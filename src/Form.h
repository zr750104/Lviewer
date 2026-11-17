#pragma once
#include "Win.h"

namespace lv {

// Runtime-laid-out modal dialog. Layout is computed from the current UI font and
// string widths, so every language (ja/en/th) gets a properly sized dialog.
class Form {
public:
    explicit Form(std::wstring title);
    ~Form();

    void Page(const std::wstring& name);                       // start a new tab page
    void Label(const std::wstring& text);
    void Edit(const std::wstring& label, std::wstring* v, int chars = 28);
    void Int(const std::wstring& label, int* v, int lo, int hi, const std::wstring& unit = L"");
    void Slider(const std::wstring& label, int* v, int lo, int hi, int div = 1);   // div: display value = v/div
    void Check(const std::wstring& label, bool* v);
    void Combo(const std::wstring& label, int* v, const std::vector<std::wstring>& items);
    void Color(const std::wstring& label, COLORREF* v);
    void Path(const std::wstring& label, std::wstring* v, bool folder, int chars = 36);
    void Text(const std::wstring& label, const std::wstring& text, int lines = 12, int chars = 60);  // read-only, scrollable
    void Button(const std::wstring& text, std::function<void()> fn);

    std::function<void()> onChange;      // live preview hook (values already written)
    std::function<bool()> onValidate;    // return false to keep the dialog open
    bool previewToggle = false;          // show a "Preview" checkbox controlling onChange
    bool* previewFlag = nullptr;

    bool Show(HWND owner);               // modal; true when OK pressed
    void Reload();                       // re-read bound values into controls
    HWND hwnd() const { return hwnd_; }

private:
    enum class K { Label, Edit, Int, Slider, Check, Combo, Color, Path, Text, Button };
    struct Item {
        K k; int page = 0;
        std::wstring label, text, unit;
        std::wstring* s = nullptr; int* i = nullptr; bool* b = nullptr; COLORREF* c = nullptr;
        int lo = 0, hi = 0, div = 1, chars = 20, lines = 1;
        bool folder = false;
        std::vector<std::wstring> items;
        std::function<void()> fn;
        HWND hLabel = nullptr, hCtl = nullptr, hAux = nullptr;
        // originals for cancel
        std::wstring s0; int i0 = 0; bool b0 = false; COLORREF c0 = 0;
    };
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT, WPARAM, LPARAM);
    void Build();
    void ShowPage(int p);
    void ReadControls();
    void Changed();
    void Restore();
    void UpdateSliderText(Item& it);

    std::wstring title_;
    std::vector<std::wstring> pages_;
    std::vector<Item> items_;
    HWND hwnd_ = nullptr, owner_ = nullptr, tab_ = nullptr, ok_ = nullptr, cancel_ = nullptr, prev_ = nullptr;
    HFONT font_ = nullptr;
    bool done_ = false, result_ = false, building_ = false, preview_ = true;
    int dpi_ = 96;
};

// Simple prompts built on Form
bool PromptText(HWND owner, const std::wstring& title, const std::wstring& label, std::wstring& value);
bool PromptInt(HWND owner, const std::wstring& title, const std::wstring& label, int& value, int lo, int hi);
int MsgBox(HWND owner, const std::wstring& text, UINT flags);

} // namespace lv
