# simply-cpp-web

C++20 reading of the web's document formats, through ranges that feel like the
standard library's. XML now, on [libxml2](https://gitlab.gnome.org/GNOME/libxml2)
(the parser PostgreSQL, the GNOME desktop and much of Linux use); HTML, and CSS
for styles, are planned. sc-graphics reads SVG with it into an `sc::drawing`.

**Documentation:** the [simply-cpp wiki](https://github.com/roelofrossouw/simply-cpp/wiki) has a [Web formats guide](https://github.com/roelofrossouw/simply-cpp/wiki/Web) and the [sc-web reference](https://github.com/roelofrossouw/simply-cpp/wiki/Reference-sc-web), plus [getting started](https://github.com/roelofrossouw/simply-cpp/wiki/Getting-Started) for the whole suite.

## Usage

```cmake
find_package(sc-web CONFIG REQUIRED)

target_link_libraries(myapp PRIVATE sc::sc-web-shared)
```

```cpp
#include <xml.h>

const auto doc = sc::web::document::load("drawing.svg");   // or parse(text); sc::web::error says where it's wrong
const auto svg = doc.root();
svg.name();                                     // "svg", without a namespace prefix
// Attributes by key, as a map; children and text through calls:
svg["width"];                                   // an attribute by key: "200", or empty when it has none
svg.attribute("width");                         // std::optional<std::string_view>, to tell missing from empty
svg.attribute("height", "100");                 // or with a fallback
svg.contains("viewBox");
for (const auto &[name, value]: svg.attributes()) { /* in document order */ }
for (const auto child: svg.children()) { /* elements only: no text or comments */ }
for (const auto rect: svg.children("rect")) { /* by name */ }
const auto children = svg.children();           // a std::ranges::forward_range
std::ranges::find_if(children, [](const auto &e) { return e.attribute("id") == "logo"; });
svg.child("title")->text();                     // all the text inside, entities decoded
use["xlink:href"];                              // a prefixed attribute, by its name as written
```

An element is a cheap handle that keeps its document alive, so the names,
values and text it gives (`std::string_view`s into the document) stay valid
while it, or any other handle or iterator of that document, lives. Keep one
around rather than chaining off a temporary: `load(file).root().name()` would
outlive its document.

**Safe by default:** parsing never fetches anything, over the network or from
a file (no external DTD or entity), and libxml2 limits how far a document's
own entities may expand. Those entities, the predefined ones and character
references are decoded in text and values; text in another encoding comes as
UTF-8.

## Demo

`sc-web-xml` reads an XML file, or without one a small SVG of its own, and
shows its root, attributes, children and text. It is installed with the
runtime package (`simply-cpp-web`):

<!-- sc-example: examples/sc-web-xml.cpp -->
```cpp
sc::console::heading("Parsing");
const auto doc = file ? sc::web::document::load(file->get<std::string>())
                      : sc::web::document::parse(R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="200" height="100">)"
                                                 R"(<title>A badge &amp; a dot</title>)"
                                                 R"(<rect width="200" height="100" rx="12" fill="steelblue"/>)"
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

sc::console::heading("A mistake");
try {
    (void) sc::web::document::parse("<svg><rect></svg>");
} catch (const sc::web::error &mistake) {
    sc::console::note(mistake.what());
}
```
<!-- /sc-example -->

## Building locally

Run `bash scripts/install.sh` to configure, build, test, package and install.
It needs libxml2: `libxml2-dev` on Ubuntu (installed if missing); the macOS
SDK has it, so nothing comes from Homebrew.

```bash
sudo curl -fsSL https://apt.roelof.co.za/setup.sh | bash
sudo apt -y install simply-cpp-web-dev
```
