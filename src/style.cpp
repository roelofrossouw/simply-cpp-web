#include "style.h"

#include <algorithm>
#include <cctype>

namespace sc::web {
    namespace {
        std::string_view trimmed(std::string_view text) {
            const auto first = text.find_first_not_of(" \t\n\r\f");
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(" \t\n\r\f") - first + 1);
        }

        std::string lowercased(std::string_view text) {
            std::string out{text};
            std::ranges::transform(out, out.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        bool same_name(std::string_view a, std::string_view b) {
            return a.size() == b.size() && std::ranges::equal(a, b, [](const unsigned char x, const unsigned char y) { return std::tolower(x) == std::tolower(y); });
        }

        // The text without /* comments */ (outside quotes).
        std::string without_comments(std::string_view text) {
            std::string out;
            char quote = 0;
            for (std::size_t i = 0; i < text.size(); ++i) {
                const char c = text[i];
                if (quote) {
                    out += c;
                    if (c == '\\' && i + 1 < text.size()) out += text[++i];
                    else if (c == quote) quote = 0;
                } else if (c == '"' || c == '\'') {
                    quote = c;
                    out += c;
                } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
                    const auto end = text.find("*/", i + 2);
                    if (end == std::string_view::npos) break;
                    i = end + 1;
                    out += ' ';
                } else {
                    out += c;
                }
            }
            return out;
        }
    }

    style::style(const std::string_view declarations) {
        const std::string text = without_comments(declarations);
        // Split at the semicolons that aren't inside quotes or parentheses.
        std::size_t start = 0;
        int depth = 0;
        char quote = 0;
        const auto take = [&](const std::size_t end) {
            const std::string_view part = trimmed(std::string_view{text}.substr(start, end - start));
            start = end + 1;
            const auto colon = part.find(':');
            if (colon == std::string_view::npos) return; // not a declaration: left out, as CSS does
            const std::string name = lowercased(trimmed(part.substr(0, colon)));
            std::string_view value = trimmed(part.substr(colon + 1));
            if (name.empty()) return;
            if (const auto bang = value.rfind('!'); bang != std::string_view::npos && same_name(trimmed(value.substr(bang + 1)), "important"))
                value = trimmed(value.substr(0, bang));
            const auto found = std::ranges::find(declarations_, name, &declaration::name);
            if (found != declarations_.end()) found->value = std::string{value}; // the last one wins
            else declarations_.push_back({name, std::string{value}});
        };
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            if (quote) {
                if (c == '\\') ++i;
                else if (c == quote) quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == '(') {
                ++depth;
            } else if (c == ')') {
                if (depth > 0) --depth;
            } else if (c == ';' && depth == 0) {
                take(i);
            }
        }
        take(text.size());
    }

    std::optional<std::string_view> style::value(const std::string_view name) const & {
        const auto found = std::ranges::find_if(declarations_, [&](const declaration &each) { return same_name(each.name, name); });
        if (found == declarations_.end()) return std::nullopt;
        return std::string_view{found->value};
    }

    std::string_view style::operator[](const std::string_view name) const & { return value(name).value_or(std::string_view{}); }
}
