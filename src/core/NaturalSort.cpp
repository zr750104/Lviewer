#include "NaturalSort.h"
#include <cwctype>
namespace lv {
static inline wchar_t Fold(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return wchar_t(c - L'A' + L'a');
    if (c >= 0xFF21 && c <= 0xFF3A) return wchar_t(c - 0xFF21 + L'a');    // full-width A-Z
    if (c >= 0xFF41 && c <= 0xFF5A) return wchar_t(c - 0xFF41 + L'a');    // full-width a-z
    return wchar_t(std::towlower(c));
}
static inline bool IsDigitW(wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= 0xFF10 && c <= 0xFF19); }
static inline int DigitVal(wchar_t c) { return c <= L'9' ? c - L'0' : c - 0xFF10; }

int CompareNames(const std::wstring& a, const std::wstring& b, bool numeric) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (numeric && IsDigitW(a[i]) && IsDigitW(b[j])) {
            size_t i0 = i, j0 = j;
            while (i < a.size() && a[i] == L'0') ++i;
            while (j < b.size() && b[j] == L'0') ++j;
            size_t is = i, js = j;
            while (i < a.size() && IsDigitW(a[i])) ++i;
            while (j < b.size() && IsDigitW(b[j])) ++j;
            size_t la = i - is, lb = j - js;
            if (la != lb) return la < lb ? -1 : 1;
            for (size_t k = 0; k < la; ++k) {
                int da = DigitVal(a[is + k]), db = DigitVal(b[js + k]);
                if (da != db) return da < db ? -1 : 1;
            }
            size_t za = is - i0, zb = js - j0;           // more leading zeros sorts first
            if (za != zb) return za > zb ? -1 : 1;
            continue;
        }
        wchar_t ca = Fold(a[i]), cb = Fold(b[j]);
        if (ca != cb) return ca < cb ? -1 : 1;
        ++i; ++j;
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}
}
