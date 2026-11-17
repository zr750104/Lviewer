#include "LangFile.h"
namespace lv {
static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
std::map<std::string, std::string> ParseLangFile(const std::string& in) {
    std::map<std::string, std::string> m;
    size_t pos = 0;
    if (in.size() >= 3 && (unsigned char)in[0] == 0xEF && (unsigned char)in[1] == 0xBB && (unsigned char)in[2] == 0xBF) pos = 3;
    while (pos < in.size()) {
        size_t e = in.find('\n', pos);
        if (e == std::string::npos) e = in.size();
        std::string line = Trim(in.substr(pos, e - pos));
        pos = e + 1;
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        std::string out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '\\' && i + 1 < v.size()) {
                char c = v[i + 1];
                if (c == 'n') { out += '\n'; ++i; continue; }
                if (c == 't') { out += '\t'; ++i; continue; }
                if (c == '\\') { out += '\\'; ++i; continue; }
            }
            out += v[i];
        }
        m[k] = out;
    }
    return m;
}
}
