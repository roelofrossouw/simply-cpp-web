// sc::web::style: CSS declarations read like a map: names case-insensitive, values as written,
// semicolons in quotes and parentheses, comments, !important, repeats, and element::style().
#include <style.h>
#include <xml.h>

#include "sc_test.h"

#include <ranges>
#include <string>
#include <vector>

static_assert(std::ranges::forward_range<sc::web::style>);

int main() {
    SECTION("Names and values, read like a map");
    {
        const sc::web::style style{"fill: red; Stroke-Width : 2px ;font-family: 'DejaVu Sans', sans-serif"};
        CHECK_EQ(style.size(), std::size_t{3});
        CHECK_EQ(style["fill"], std::string_view{"red"});
        CHECK_EQ(style["stroke-width"], std::string_view{"2px"}); // names lowercased, trimmed
        CHECK_EQ(style["STROKE-WIDTH"], std::string_view{"2px"}); // looked up case-insensitively
        CHECK_EQ(style["font-family"], std::string_view{"'DejaVu Sans', sans-serif"}); // values as written
        CHECK(style.contains("fill"));
        CHECK(!style.contains("color"));
        CHECK(style["color"].empty());
        CHECK(!style.value("color").has_value());
        std::vector<std::string> names;
        for (const auto &[name, value]: style) names.push_back(name);
        CHECK_EQ(names, (std::vector<std::string>{"fill", "stroke-width", "font-family"})); // in order
    }

    SECTION("Semicolons in quotes and parentheses, comments, !important, repeats, mistakes");
    {
        const sc::web::style style{R"(background: url(data:image/png;base64,AAAA); content: "a;b"; /* gone; */ color: blue !important;)"
                                   R"( color: green; nonsense; : empty; margin:)"};
        CHECK_EQ(style["background"], std::string_view{"url(data:image/png;base64,AAAA)"});
        CHECK_EQ(style["content"], std::string_view{"\"a;b\""});
        CHECK_EQ(style["color"], std::string_view{"green"}); // the last one wins
        CHECK(style.contains("margin"));                   // given, though empty
        CHECK(style["margin"].empty());
        CHECK_EQ(style.size(), std::size_t{4}); // background, content, color, margin: no "nonsense", no nameless one
        CHECK_EQ(sc::web::style{"fill: red !IMPORTANT"}["fill"], std::string{"red"}); // from a temporary: a copy, safe
        CHECK(sc::web::style{""}.empty());
        CHECK(sc::web::style{" ; ;; "}.empty());
    }

    SECTION("An element's style attribute");
    {
        const auto doc = sc::web::document::parse(R"(<svg><rect style="fill:#2a6f97;stroke:none"/><circle/></svg>)");
        const auto rect = doc.root().child("rect");
        CHECK(rect.has_value());
        if (rect) {
            CHECK(rect->style()["fill"] == "#2a6f97");
            const auto fill = rect->style()["fill"]; // a std::string: the temporary style can go
            CHECK_EQ(fill, std::string{"#2a6f97"});
            CHECK(rect->style().contains("stroke"));
        }
        CHECK(doc.root().child("circle")->style().empty()); // none: empty
    }

    TEST_SUMMARY();
}
