#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sc::web {
    /// CSS declarations, as in a style="..." attribute: names and values, read like a std::map.
    /// Property names are case-insensitive, as CSS's are (kept lowercased); values stay as written,
    /// trimmed. A ";" inside quotes or parentheses doesn't end a declaration, so
    /// url(data:image/png;base64,...) stays whole; comments are dropped, "!important" comes off the
    /// value, and a property given twice keeps its last value, where it first appeared.
    ///
    ///     const auto style = element.style();               // or sc::web::style{"fill: red; stroke: none"}
    ///     style["fill"] == "red";
    ///     style.contains("stroke");
    ///     for (const auto &[name, value]: style) { ... }
    class style {
    public:
        struct declaration {
            std::string name; // lowercased
            std::string value;

            bool operator==(const declaration &) const = default;
        };

        using const_iterator = std::vector<declaration>::const_iterator;

        style() = default;

        explicit style(std::string_view declarations);

        /// A property's value, empty when it has none.
        [[nodiscard]] std::string_view operator[](std::string_view name) const;

        /// The value, or nothing: tells an empty value from a missing one.
        [[nodiscard]] std::optional<std::string_view> value(std::string_view name) const;

        [[nodiscard]] bool contains(std::string_view name) const { return value(name).has_value(); }

        [[nodiscard]] const_iterator begin() const { return declarations_.begin(); }
        [[nodiscard]] const_iterator end() const { return declarations_.end(); }
        [[nodiscard]] std::size_t size() const { return declarations_.size(); }
        [[nodiscard]] bool empty() const { return declarations_.empty(); }

        bool operator==(const style &) const = default;

    private:
        std::vector<declaration> declarations_;
    };
}
