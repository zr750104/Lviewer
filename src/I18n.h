#pragma once
#include "Win.h"
namespace lv {
struct LangInfo { const wchar_t* code; const wchar_t* nativeName; };
extern const LangInfo kLanguages[3];
std::wstring DetectOsLanguage();                 // "ja" / "en" / "th"
void I18nLoad(const std::wstring& code);         // loads embedded table (+ override file)
const std::wstring& CurrentLanguage();
const std::wstring& T(const char* key);          // returns key itself when missing
std::wstring UiFontFace();
HFONT CreateUiFont(int dpi, int pt = 9, bool bold = false);
}
