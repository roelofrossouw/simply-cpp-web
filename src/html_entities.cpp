#include <sc/web/html_entities.h>
#include "html_entities_tables.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace sc::web {
    namespace {
        constexpr char32_t replacement = 0xFFFD;

        // The code point that starts at text[at] and how many bytes it takes; a byte that doesn't
        // start valid UTF-8 is itself, one byte long, with valid false.
        struct decoded {
            char32_t code_point;
            std::size_t length;
            bool valid;
        };

        decoded next(const std::string_view text, const std::size_t at) {
            const auto byte = [&](const std::size_t i) { return static_cast<unsigned char>(text[i]); };
            const unsigned char lead = byte(at);
            if (lead < 0x80) return {lead, 1, true};
            std::size_t length;
            char32_t value;
            if ((lead & 0xE0) == 0xC0) length = 2, value = lead & 0x1F;
            else if ((lead & 0xF0) == 0xE0) length = 3, value = lead & 0x0F;
            else if ((lead & 0xF8) == 0xF0) length = 4, value = lead & 0x07;
            else return {lead, 1, false};
            if (at + length > text.size()) return {lead, 1, false};
            for (std::size_t i = 1; i < length; ++i) {
                if ((byte(at + i) & 0xC0) != 0x80) return {lead, 1, false};
                value = (value << 6) | (byte(at + i) & 0x3F);
            }
            // Overlong forms, surrogates and what's past Unicode aren't UTF-8.
            static constexpr char32_t least[] = {0, 0, 0x80, 0x800, 0x10000};
            if (value < least[length] || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return {lead, 1, false};
            return {value, length, true};
        }

        void append_utf8(std::string &out, char32_t c) {
            if (c == 0 || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) c = replacement;
            if (c < 0x80) {
                out += static_cast<char>(c);
            } else if (c < 0x800) {
                out += static_cast<char>(0xC0 | (c >> 6));
                out += static_cast<char>(0x80 | (c & 0x3F));
            } else if (c < 0x10000) {
                out += static_cast<char>(0xE0 | (c >> 12));
                out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (c & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (c >> 18));
                out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (c & 0x3F));
            }
        }

        template <std::size_t N>
        bool in(const detail::code_range (&ranges)[N], const char32_t c) {
            const auto found = std::upper_bound(std::begin(ranges), std::end(ranges), c,
                                                [](const char32_t value, const detail::code_range &range) { return value < range.first; });
            return found != std::begin(ranges) && c <= std::prev(found)->last;
        }

        bool word(const char32_t c) { return in(detail::word_ranges, c); }

        const detail::named_reference *named(const std::string_view name) {
            const auto found = std::lower_bound(std::begin(detail::named_references), std::end(detail::named_references), name,
                                                [](const detail::named_reference &each, const std::string_view key) { return std::string_view{each.name} < key; });
            return found != std::end(detail::named_references) && std::string_view{found->name} == name ? found : nullptr;
        }

        bool ascii_digit(const char c) { return c >= '0' && c <= '9'; }
        bool ascii_hex(const char c) { return ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

        // A run of digits as a code point; one too long for a code point gives one past Unicode.
        char32_t number(const std::string_view digits, const int base) {
            char32_t value = 0;
            for (const char c: digits) {
                const int digit = ascii_digit(c) ? c - '0' : (c | 0x20) - 'a' + 10;
                value = value * base + digit;
                if (value > 0x10FFFF) return 0x110000;
            }
            return value;
        }
    }

    std::string encode_entities(const std::string_view text) {
        std::string out;
        out.reserve(text.size() + text.size() / 8);
        for (std::size_t at = 0; at < text.size();) {
            const auto [c, length, valid] = next(text, at);
            if (!valid || !in(detail::unsafe_ranges, c)) {
                out.append(text.substr(at, length));
            } else {
                const auto found = std::lower_bound(std::begin(detail::encode_names), std::end(detail::encode_names), c,
                                                    [](const detail::encode_name &each, const char32_t key) { return each.code_point < key; });
                if (found != std::end(detail::encode_names) && found->code_point == c) {
                    out += found->name;
                } else {
                    char reference[16];
                    std::snprintf(reference, sizeof reference, "&#x%x;", static_cast<unsigned>(c));
                    out += reference;
                }
            }
            at += length;
        }
        return out;
    }

    std::string decode_entities(const std::string_view text) {
        std::string out;
        out.reserve(text.size());
        std::size_t at = 0;
        while (at < text.size()) {
            const auto amp = text.find('&', at);
            if (amp == std::string_view::npos) {
                out.append(text.substr(at));
                break;
            }
            out.append(text.substr(at, amp - at));
            std::size_t end = amp + 1;
            // &#169 or &#xA9
            if (end < text.size() && text[end] == '#') {
                std::size_t from = end + 1;
                int base = 10;
                if (from < text.size() && (text[from] == 'x' || text[from] == 'X') && from + 1 < text.size() && ascii_hex(text[from + 1])) {
                    base = 16;
                    ++from;
                }
                std::size_t to = from;
                while (to < text.size() && (base == 16 ? ascii_hex(text[to]) : ascii_digit(text[to]))) ++to;
                if (to > from) {
                    append_utf8(out, number(text.substr(from, to - from), base));
                    at = to < text.size() && text[to] == ';' ? to + 1 : to;
                    continue;
                }
                out += '&'; // no number after #: the & is only an &
                at = amp + 1;
                continue;
            }
            // &name; the name runs over every word character
            while (end < text.size()) {
                const auto [c, length, valid] = next(text, end);
                if (!valid || !word(c)) break;
                end += length;
            }
            if (end == amp + 1) {
                out += '&';
                at = amp + 1;
                continue;
            }
            const auto name = text.substr(amp + 1, end - amp - 1);
            const bool semicolon = end < text.size() && text[end] == ';';
            if (const auto *found = named(name)) {
                append_utf8(out, found->first);
                if (found->second != 0) append_utf8(out, found->second);
            } else {
                out.append(text.substr(amp, end - amp + (semicolon ? 1 : 0)));
            }
            at = semicolon ? end + 1 : end;
        }
        return out;
    }
}

// The 2.0 names, kept exported for programs built against them (see html_entities.h).
namespace sc::html {
    std::string encode_entities(const std::string_view text) { return web::encode_entities(text); }
    std::string decode_entities(const std::string_view text) { return web::decode_entities(text); }
}
