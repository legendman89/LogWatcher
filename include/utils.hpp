#pragma once

#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <string>
#include <filesystem>

namespace Utils {

    #undef KB
    #undef MB
    #undef KB2B
    #undef MB2B
    #define KB(x)       ((x) / 1024ULL)
    #define MB(x)       ((x) / (1024ULL * 1024ULL))
    #define KB2B(x)     ((x) * 1024ULL)
    #define MB2B(x)     ((x) * 1024ULL * 1024ULL)

    template <class T>
    inline bool isSpace(const T& ch) {
        return (ch >= 9 && ch <= 13) || ch == 32;
    }

    inline bool isDigit(const char& ch) { return (ch ^ '0') <= 9; }

    inline void eatWS(char*& str) {
        while (isSpace(*str)) ++str;
    }

    inline char toLowerC(const char& c) { return (char)std::tolower(int(c)); }

    inline bool isWordChar(const char& c) { return std::isalnum(int(c)) || c == '_'; }

    inline std::string toUTF8(const std::filesystem::path& p) {
        auto u8 = p.u8string();
        return std::string(u8.begin(), u8.end());
    }

    inline bool isCppExt(const std::string& ext) {
        static const char* k[] = { "c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "ipp", "inl", "tpp", "ixx", "cppm" };
        for (auto* e : k)
            if (ext == e) return true;
        return false;
    }

    inline bool isDecorChar(const char& c) {
        switch (c) {
            case '*': case '>': case '<': case '-': case '=': case '_':
            case '~': case '#': case '.': case '!': case '/': case '\\':
            case '+': case '|': case ':':
                return true;
            default: return false;
        }
    }

    inline bool hasCppExtAt(const std::string& s, size_t pos, size_t* extEndOut = nullptr) {
        if (pos == std::string::npos || pos + 1 >= s.size()) return false;
        size_t i = pos + 1;
        while (i < s.size() && isWordChar(s[i])) ++i;
        std::string ext;
        ext.reserve(i - (pos + 1));
        for (size_t j = pos + 1; j < i; ++j) ext.push_back(toLowerC(s[j]));
        if (extEndOut) *extEndOut = i;
        return isCppExt(ext);
    }

    inline std::string replaceUsername(const std::string path, const std::string keyword = "HIDDEN") {
        const std::string users = "\\Users\\";
        const auto pos = path.find(users);
        if (pos == std::string::npos) {
            return path;
        }

        const size_t startUsername = pos + users.size();
        const size_t endUsername = path.find("\\", startUsername);

        if (endUsername == std::string::npos) {
            return path.substr(0, startUsername) + keyword;
        }

        return path.substr(0, startUsername) + keyword + path.substr(endUsername);
    }

    inline void stripLeadingCppPath(std::string& s) {
        if (s.empty()) return;

        size_t guard = s.find('[');
        if (guard == std::string::npos) guard = s.size();
        size_t dot = s.find('.');
        if (dot == std::string::npos || dot >= guard) return;

        size_t extEnd = dot;
        if (!hasCppExtAt(s, dot, &extEnd)) return;

        size_t i = extEnd;

        if (i < s.size() && s[i] == '(') {
            size_t j = i + 1;
            while (j < s.size() && isDigit(s[j])) ++j;
            if (j < s.size() && s[j] == ')') {
                i = j + 1;
                if (i < s.size() && s[i] == ':') ++i;
            }
        }
        else if (i < s.size() && s[i] == ':') {
            size_t j = i + 1;
            while (j < s.size() && isDigit(s[j])) ++j;
            if (j < s.size() && s[j] == ':') ++j;
            i = j;
        }

        while (i < s.size() && isSpace(s[i])) ++i;

        if (i > 0) s.erase(0, i);
    }

    inline void stripBracketedCpp(std::string& s) {
        size_t i = 0;
        while (true) {
            size_t lb = s.find('[', i);
            if (lb == std::string::npos) break;
            size_t rb = s.find(']', lb + 1);
            if (rb == std::string::npos) break;
            bool foundCpp = false;
            size_t scan = lb + 1;
            while (!foundCpp) {
                size_t dot = s.find('.', scan);
                if (dot == std::string::npos || dot >= rb) break;
                if (hasCppExtAt(s, dot)) {
                    foundCpp = true;
                    break;
                }
                scan = dot + 1;
            }
            if (foundCpp) {
                s.erase(lb, rb - lb + 1);
            }
            else {
                i = rb + 1;
            }
        }
    }

    inline void stripParendCpp(std::string& s) {
        size_t i = 0;
        while (true) {
            size_t lp = s.find('(', i);
            if (lp == std::string::npos) break;
            size_t rp = s.find(')', lp + 1);
            if (rp == std::string::npos) break;
            bool found = false;
            size_t scan = lp + 1;
            while (!found) {
                size_t dot = s.find('.', scan);
                if (dot == std::string::npos || dot >= rp) break;
                if (hasCppExtAt(s, dot)) { found = true; break; }
                scan = dot + 1;
            }
            if (found) {
                size_t eraseEnd = rp + 1;
                if (eraseEnd < s.size() && s[eraseEnd] == ':') {
                    ++eraseEnd;
                    while (eraseEnd < s.size() && isSpace(s[eraseEnd])) ++eraseEnd;
                }
                s.erase(lp, eraseEnd - lp);
            }
            else {
                i = rp + 1;
            }
        }
    }

    inline void stripDecorativeRuns(std::string& s, size_t minRun = 4) {
        std::string out; out.reserve(s.size());
        for (size_t i = 0, n = s.size(); i < n; ) {
            char c = s[i];
            if (isDecorChar(c)) {
                size_t j = i + 1;
                while (j < n && s[j] == c) ++j;
                if (j - i >= minRun) { i = j; continue; }
                out.append(s, i, j - i);
                i = j;
            }
            else {
                out.push_back(c);
                ++i;
            }
        }
        s.swap(out);
    }

    inline std::string trimLine(std::string_view s) {
        auto l = s.begin(), r = s.end();
        while (l != r && (*l == ' ' || *l == '\t' || *l == '\r' || *l == '\n')) ++l;
        while (r != l) {
            auto c = *(r - 1);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                --r;
            else
                break;
        }
        return std::string(l, r);
    }

    inline void collapseAndTrim(std::string& s) {
        std::string out;
        out.reserve(s.size());
        bool inSpace = false;
        for (char c : s) {
            if (isSpace(c)) {
                if (!inSpace) {
                    out.push_back(' ');
                    inSpace = true;
                }
            }
            else {
                out.push_back(c);
                inSpace = false;
            }
        }
        size_t b = 0, e = out.size();
        while (b < e && out[b] == ' ') ++b;
        while (e > b && out[e - 1] == ' ') --e;
        s.assign(out.begin() + b, out.begin() + e);
    }

    inline std::string spacify(const std::string& name) {
        std::string s = std::regex_replace(name, std::regex("_+"), " ");
        s = std::regex_replace(s, std::regex("([a-z])([A-Z])"), "$1 $2");
        return s;
    }

    inline std::string nukeLogLine(std::string s, const bool& removeCppStuff = true) {
        if (removeCppStuff) {
            stripLeadingCppPath(s);
            stripBracketedCpp(s);
            stripParendCpp(s);
        }

        static const std::regex ts_iso_br(
            "\\[\\d{4}-\\d{2}-\\d{2}\\s+\\d{2}[:-]\\d{2}[:-]\\d{2}(?:[.:]\\d+)?\\]",
            std::regex::icase
        );
        static const std::regex ts_iso_slash_br(
            "\\[\\d{4}-\\d{2}-\\d{2}/\\d{2}:\\d{2}:\\d{2}(?:[.:]\\d+)?\\]",
            std::regex::icase
        );
        static const std::regex ts_us_br(
            "\\[\\d{1,2}/\\d{1,2}/\\d{2,4}\\s+\\d{2}[:-]\\d{2}[:-]\\d{2}(?:[.:]\\d+)?\\]",
            std::regex::icase
        );
        static const std::regex ts_time_br(
            "\\[\\d{2}:\\d{2}:\\d{2}(?::\\d+|\\.\\d+)?(?:\\s*[+-]\\d{2}:\\d{2})?\\]",
            std::regex::icase
        );
        static const std::regex ts_us_leading(
            "^\\s*\\d{1,2}/\\d{1,2}/\\d{2,4}\\s*-\\s*\\d{2}:\\d{2}:\\d{2}(?:[.:]\\d+)?\\s*",
            std::regex::icase
        );
        static const std::regex pidtid_br(
            "\\[\\s*pid\\s*:\\s*\\d+\\s*\\|\\s*tid\\s*:\\s*\\d+\\s*\\]",
            std::regex::icase
        );
        static const std::regex word_re("\\[\\s*[A-Za-z_]+\\s*\\]");
        static const std::regex num_re("\\[\\s*\\d+\\s*\\]");

        s = std::regex_replace(s, ts_iso_br, "");
        s = std::regex_replace(s, ts_iso_slash_br, "");
        s = std::regex_replace(s, ts_us_br, "");
        s = std::regex_replace(s, ts_time_br, "");
        s = std::regex_replace(s, ts_us_leading, "");
        s = std::regex_replace(s, pidtid_br, "");
        s = std::regex_replace(s, word_re, "");
        s = std::regex_replace(s, num_re, "");

		stripDecorativeRuns(s, 4);
        collapseAndTrim(s);
        return s;
    }

}
