// Reads XML with sc::web: the root, attributes, children as ranges (filtered by name, with
// std::ranges algorithms), text, a style attribute, and what a mistake reports. Each line shows a call and what it
// returned. With no FILE it reads a small SVG of its own; --help lists the options.

#include <algorithm>
#include <iostream>
#include <string>

#include <params.h>
#include <sc.h>

#include <xml.h>

int main(int argc, char **argv) {
    // sc::params brings -h/--help, and reports mistakes and exits.
    const sc::params args{argc, argv, {
        {"name", "sc-web-xml"},
        {"description", "Reads an XML file (or, without FILE, a small SVG) with sc::web and shows its root, attributes, children and text."},
        {"positional", {
            {"file", {{"required", false}, {"help", "the XML file to read"}}}}}}};

    try {
        sc::console::title("simply-cpp web: XML through ranges");
        const auto *file = args.lookup("file");

        // [readme]
        sc::console::heading("Parsing");
        const auto doc = file ? sc::web::document::load(file->get<std::string>())
                              : sc::web::document::parse(R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="200" height="100">)"
                                                         R"(<title>A badge &amp; a dot</title>)"
                                                         R"(<rect width="200" height="100" rx="12" style="fill: steelblue; stroke: none"/>)"
                                                         R"(<circle cx="50" cy="50" r="20" fill="gold"/>)"
                                                         R"(<use xlink:href="#dot" x="100"/>)"
                                                         R"(<text x="90" y="60">simply <tspan font-weight="bold">cpp</tspan></text></svg>)");
        sc::console::step(file ? "auto doc = sc::web::document::load(file);" : "auto doc = sc::web::document::parse(svg);");
        const auto root = doc.root();
        SC_SHOW(root.name());
        SC_SHOW(root["width"]);                 // an attribute by key, empty when it has none
        SC_SHOW(root.attribute("depth", "?"));  // or with a fallback; attribute(name) is an optional

        sc::console::heading("Attributes: a range of {name, value}");
        for (const auto &[name, value]: root.attributes()) sc::console::note(std::string{name} + " = " + std::string{value});

        sc::console::heading("Children: a range of elements");
        for (const auto child: root.children()) sc::console::note(std::string{child.name()} + " on line " + std::to_string(child.line()));
        SC_SHOW(std::ranges::distance(root.children("rect")));                                 // filtered by name
        const auto children = root.children();
        const auto gold = std::ranges::find_if(children, [](const auto &e) { return e.attribute("fill") == "gold"; });
        if (gold != children.end()) SC_SHOW((*gold).name());

        sc::console::heading("Text, entities decoded");
        if (const auto title = root.child("title")) SC_SHOW(title->text());
        if (const auto text = root.child("text")) SC_SHOW(text->text());                       // its tspan's text too
        if (const auto use = root.child("use")) SC_SHOW((*use)["xlink:href"]);                 // a prefixed attribute

        sc::console::heading("A style attribute, read like a map");
        if (const auto rect = root.child("rect")) {
            const auto style = rect->style();
            SC_SHOW(style["fill"]);
            SC_SHOW(style.contains("stroke"));
            for (const auto &[name, value]: style) sc::console::note(name + ": " + value);
        }

        sc::console::heading("A mistake");
        try {
            (void) sc::web::document::parse("<svg><rect></svg>");
        } catch (const sc::web::error &mistake) {
            sc::console::note(mistake.what());
        }
        // [/readme]
    } catch (const std::exception &error) {
        std::cerr << "sc-web-xml: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
