#pragma once
#include <string>
namespace lv {
// Case-insensitive compare. numeric=true compares embedded digit runs by value ("2" < "10").
int CompareNames(const std::wstring& a, const std::wstring& b, bool numeric);
}
