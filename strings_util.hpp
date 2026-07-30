#ifndef STRINGS_UTIL_HPP
#define STRINGS_UTIL_HPP

#include <string>
#include <sstream>
#include <optional>
#include <string_view>
#include <span>
#include <compare>
#include <algorithm>
#include <cctype>

namespace strings {

    // Joins elements of a string container into a single string with a delimiter,
    // optionally using a different delimiter before the last element.
    inline const std::string join(
        std::span<const std::string> elements, std::string_view delimiter,
        std::optional<std::string_view> last_delimiter_opt = std::nullopt
    )
    {
        if (elements.empty()) return "";
        if (elements.size() == 1) return elements[0];

        std::string_view last_delimiter = last_delimiter_opt.value_or(delimiter);

        std::ostringstream oss;
        oss << elements[0];

        for (size_t i = 1; i < elements.size() - 1; ++i) oss << delimiter << elements[i];
        oss << last_delimiter << elements.back();

        return oss.str();
    }

    inline const std::string plural(int count, const std::string &one, const std::string &many) {
        return count != 1 ? many : one;
    }

    /**
     * @brief Strips trailing whitespace and sentence-ending periods from a clause.
     *
     * Report lines are assembled as a single sentence which the generator terminates itself,
     * e.g. "Name [1] : Fort, needs 5; description." A description that is already a full
     * sentence would otherwise produce a doubled period ("... if the guardian within is slain..").
     *
     * '!' and '?' are deliberately left alone - they carry meaning a period cannot replace.
     *
     * @param text clause about to be embedded in a generated sentence
     * @return text without trailing spaces, tabs and periods (empty if nothing else remains)
     * @example "Full treasure trove." -> "Full treasure trove"
     * @example "A ruin of old..."     -> "A ruin of old"
     * @example "Really?"              -> "Really?"
     */
    inline const std::string strip_sentence_end(const std::string& text) {
        size_t end = text.find_last_not_of(" \t.");
        return end == std::string::npos ? "" : text.substr(0, end + 1);
    }

    struct ci_traits : public std::char_traits<char> {
        using Base = std::char_traits<char>;
        using char_type = Base::char_type;
        using int_type = Base::int_type;

        static char_type normalize(char_type ch) {
            if (ch == '_') return ' ';
            return static_cast<char_type>(std::tolower(static_cast<unsigned char>(ch)));
        }

        static bool eq(char_type c1, char_type c2) noexcept {
            return normalize(c1) == normalize(c2);
        }

        static bool lt(char_type c1, char_type c2) noexcept {
            return normalize(c1) < normalize(c2);
        }

        static int compare(const char_type* s1, const char_type* s2, std::size_t n) noexcept {
            for (std::size_t i = 0; i < n; ++i) {
                char_type norm_c1 = normalize(s1[i]);
                char_type norm_c2 = normalize(s2[i]);
                if (norm_c1 < norm_c2) return -1;
                if (norm_c1 > norm_c2) return 1;
            }
            return 0;
        }

        static const char_type* find(const char_type* s, std::size_t n, char_type a) noexcept {
            char_type norm_a = normalize(a);
            for (std::size_t i = 0; i < n; ++i) {
                if (normalize(s[i]) == norm_a) {
                    return s + i;
                }
            }
            return nullptr;
        }
    };

    class ci_string : public std::basic_string<char, ci_traits> {
    public:
        using std::basic_string<char, ci_traits>::basic_string;
        using std::basic_string<char, ci_traits>::operator=;
        ci_string() = default;
        ci_string(const std::string& s) : std::basic_string<char, ci_traits>(s.data(), s.length()) {}
        ci_string(std::string_view sv) : std::basic_string<char, ci_traits>(sv.data(), sv.length()) {}
        ci_string(const std::basic_string<char, ci_traits>& s) : std::basic_string<char, ci_traits>(s) {}
        ci_string& operator=(const std::string& s) { this->assign(s.data(), s.length()); return *this; }
        ci_string& operator=(std::string_view sv) { this->assign(sv.data(), sv.length()); return *this; }

    };

    inline bool operator==(const ci_string& lhs, const std::string& rhs) { return lhs.compare(rhs.c_str()) == 0; }
    inline bool operator==(const std::string& lhs, const ci_string& rhs) { return rhs.compare(lhs.c_str()) == 0; }
    inline bool operator!=(const ci_string& lhs, const std::string& rhs) { return !(lhs == rhs); }
    inline bool operator!=(const std::string& lhs, const ci_string& rhs) { return !(lhs == rhs); }
    inline bool operator==(const ci_string& lhs, const char* rhs) { return rhs && lhs.compare(rhs) == 0; }
    inline bool operator==(const char* lhs, const ci_string& rhs) { return lhs && rhs.compare(lhs) == 0; }
    inline bool operator!=(const ci_string& lhs, const char* rhs) { return !rhs || !(lhs == rhs); }
    inline bool operator!=(const char* lhs, const ci_string& rhs) { return !lhs || !(lhs == rhs); }
} // namespace strings

#endif // STRINGS_UTIL_HPP
