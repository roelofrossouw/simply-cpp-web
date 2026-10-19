// sc::web's XML: parsing, the root, attributes (prefixed ones too), children as ranges, text with
// entities, handles that keep their document alive, and what mistakes throw.
#include <xml.h>

#include "sc_test.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

static_assert(std::ranges::forward_range<sc::web::elements>);
static_assert(std::ranges::forward_range<sc::web::attributes>);

int main() {
    const std::string svg = R"(<?xml version="1.0" encoding="UTF-8"?>)"
                            R"(<!DOCTYPE svg [<!ENTITY custom "expanded">]>)"
                            R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="200" height="100">)"
                            "\n<!-- a comment -->\n"
                            R"(<title>Tom &amp; Jerry &#x263A; &custom;</title>)"
                            R"(<rect x="1" width="10"/><circle r="5"/><rect x="2" width="20"/>)"
                            R"(<use xlink:href="#a" label="a &custom; b"/>)"
                            R"(<text>one <tspan>two</tspan> three<![CDATA[ <four> ]]></text></svg>)";

    SECTION("The root and its attributes");
    {
        const auto doc = sc::web::document::parse(svg);
        const auto root = doc.root();
        CHECK_EQ(root.name(), std::string_view{"svg"});
        CHECK(root.is("svg"));
        CHECK(!root.is("html"));
        CHECK(root.attribute("width") == std::optional<std::string_view>{"200"});
        CHECK(!root.attribute("depth").has_value());
        CHECK_EQ(root.attribute("depth", "none"), std::string_view{"none"});
        CHECK_EQ(root["width"], std::string_view{"200"}); // by key, as a map
        CHECK(root["depth"].empty());
        CHECK(root.contains("height"));
        CHECK(!root.contains("depth"));
        std::vector<std::string> names;
        for (const auto &[name, value]: root.attributes()) names.emplace_back(name);
        CHECK_EQ(names, (std::vector<std::string>{"width", "height"})); // namespace declarations aren't attributes
        CHECK(!root.parent().has_value());
    }

    SECTION("Children: every element, or by name, as ranges");
    {
        const auto root = sc::web::document::parse(svg).root();
        std::vector<std::string> all;
        for (const auto child: root.children()) all.emplace_back(child.name());
        CHECK_EQ(all, (std::vector<std::string>{"title", "rect", "circle", "rect", "use", "text"})); // no comments or text
        const auto rects = root.children("rect");
        CHECK_EQ(std::ranges::distance(rects), 2);
        CHECK_EQ((*std::ranges::next(rects.begin())).attribute("x", ""), std::string_view{"2"});
        const auto children = root.children();
        const auto circle = std::ranges::find_if(children, [](const auto &e) { return e.attribute("r").has_value(); });
        CHECK(circle != children.end());
        if (circle != children.end()) CHECK((*circle).parent() == root);
        CHECK(root.child("circle").has_value());
        CHECK(!root.child("ellipse").has_value());
        CHECK(root.children("ellipse").empty());
        CHECK(sc::web::document::parse("<empty/>").root().children().empty());
    }

    SECTION("Text and attribute values: entities decoded, the DOCTYPE's own ones too");
    {
        const auto root = sc::web::document::parse(svg).root();
        const auto title = root.child("title");
        CHECK(title.has_value());
        if (title) CHECK_EQ(title->text(), std::string{"Tom & Jerry \u263A expanded"});
        const auto text = root.child("text");
        if (text) CHECK_EQ(text->text(), std::string{"one two three <four> "}); // descendants and CDATA too
        const auto use = root.child("use");
        CHECK(use.has_value());
        if (use) {
            CHECK(use->attribute("xlink:href") == std::optional<std::string_view>{"#a"}); // by its name as written
            CHECK_EQ((*use)["xlink:href"], std::string_view{"#a"});
            CHECK(!use->attribute("href").has_value());
            CHECK(use->attribute("label") == std::optional<std::string_view>{"a expanded b"}); // joined around the entity
            std::vector<std::string> names;
            for (const auto &[name, value]: use->attributes()) names.emplace_back(name);
            CHECK_EQ(names, (std::vector<std::string>{"xlink:href", "label"}));
        }
        CHECK_EQ(root.child("rect")->line(), 3L);
    }

    SECTION("Handles keep their document alive");
    {
        std::optional<sc::web::element> kept;
        sc::web::elements::iterator it;
        {
            const auto doc = sc::web::document::parse(svg);
            kept = doc.root().child("circle");
            it = doc.root().children("rect").begin(); // the range and the document go away here
        }
        CHECK(kept.has_value());
        if (kept) CHECK_EQ(kept->attribute("r", ""), std::string_view{"5"});
        CHECK_EQ((*it).attribute("x", ""), std::string_view{"1"});
        ++it;
        CHECK_EQ((*it).attribute("x", ""), std::string_view{"2"});
    }

    SECTION("Files and mistakes");
    {
        const auto file = std::filesystem::temp_directory_path() / "sc-web-test.xml";
        std::ofstream{file} << "<a><b/></a>";
        CHECK_EQ(sc::web::document::load(file).root().name(), std::string{"a"}); // a temporary's name is a copy: safe
        std::filesystem::remove(file);
        CHECK_THROWS_AS(sc::web::document::load(file), sc::web::error);
        CHECK_THROWS_AS(sc::web::document::parse(""), sc::web::error);
        CHECK_THROWS_AS(sc::web::document::parse("not xml"), sc::web::error);
        CHECK_THROWS_AS(sc::web::document::parse("<a><b></a>"), sc::web::error);
        try {
            (void) sc::web::document::parse("<a>\n<b>\n</a>");
        } catch (const sc::web::error &mistake) {
            CHECK(std::string{mistake.what()}.find("line 3") != std::string::npos);
        }
    }

    TEST_SUMMARY();
}
