#pragma once
#include <map>
#include <string>
namespace lv {
// Parses "key=value" lines (UTF-8, optional BOM). '#' or ';' starts a comment line.
// "\n" and "\t" escapes in values are expanded.
std::map<std::string, std::string> ParseLangFile(const std::string& utf8);
}
