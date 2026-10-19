#pragma once

#include "style.h"

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace sc::web {
    /// A document that can't be read: bad XML, or a file that can't be opened. what() says where.
    class error : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    namespace detail {
        struct document_data; // libxml2's tree, kept out of this header
    }

    class element;

    /// One attribute of an element: its name as written ("width", "xlink:href") and its value,
    /// entities decoded. Valid while the element's document is (any element of it keeps it).
    struct attribute {
        std::string_view name;
        std::string_view value;
    };

    /// An element's attributes, in document order: a forward range of sc::web::attribute.
    ///
    ///     for (const auto &[name, value]: rect.attributes()) { ... }
    class attributes {
    public:
        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = attribute;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = attribute;

            iterator() = default;

            attribute operator*() const;
            iterator &operator++();
            iterator operator++(int);
            bool operator==(const iterator &other) const { return at_ == other.at_; }

        private:
            friend class attributes;
            iterator(std::shared_ptr<detail::document_data> document, const void *at) : document_(std::move(document)), at_(at) {
            }

            std::shared_ptr<detail::document_data> document_; // the values stay valid while it lives
            const void *at_ = nullptr;
        };

        [[nodiscard]] iterator begin() const { return {document_, first_}; }
        [[nodiscard]] iterator end() const { return {}; }
        [[nodiscard]] bool empty() const { return first_ == nullptr; }

    private:
        friend class element;
        attributes(std::shared_ptr<detail::document_data> document, const void *first) : document_(std::move(document)), first_(first) {
        }

        std::shared_ptr<detail::document_data> document_;
        const void *first_ = nullptr;
    };

    /// An element's child elements in document order, or only those with one name: a forward range
    /// of sc::web::element (text, comments and the like are left out). Its iterators keep the
    /// document alive themselves, so they can outlive the range.
    ///
    ///     for (const auto row: table.children("tr")) { ... }
    ///     std::ranges::find_if(svg.children(), [](const auto &e) { return e.name() == "defs"; });
    class elements {
    public:
        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = element;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = element;

            iterator() = default;

            element operator*() const;
            iterator &operator++();
            iterator operator++(int);
            bool operator==(const iterator &other) const { return at_ == other.at_; }

        private:
            friend class elements;
            iterator(std::shared_ptr<detail::document_data> document, std::shared_ptr<const std::string> name, const void *at);

            std::shared_ptr<detail::document_data> document_;
            std::shared_ptr<const std::string> name_; // empty: every element
            const void *at_ = nullptr;
        };

        [[nodiscard]] iterator begin() const { return {document_, name_, first_}; }
        [[nodiscard]] iterator end() const { return {}; }
        [[nodiscard]] bool empty() const { return begin() == end(); }

    private:
        friend class element;
        elements(std::shared_ptr<detail::document_data> document, const void *first, std::string name)
            : document_(std::move(document)), name_(std::make_shared<const std::string>(std::move(name))), first_(first) {
        }

        std::shared_ptr<detail::document_data> document_;
        std::shared_ptr<const std::string> name_;
        const void *first_ = nullptr;
    };

    /// An element of a document: a cheap handle, copied freely, that keeps its document alive, so the
    /// names, values and text it gives stay valid while it (or any other handle) lives.
    class element {
    public:
        /// Its local name, without a namespace prefix: "rect" for <svg:rect>.
        [[nodiscard]] std::string_view name() const;

        /// Its name as written: "svg:rect".
        [[nodiscard]] std::string qualified_name() const;

        /// Whether its local name is this: if (!root.is("svg")) throw ...
        [[nodiscard]] bool is(const std::string_view local_name) const { return name() == local_name; }

        /// An attribute's value by its name as written ("width", "xlink:href"); nothing when it
        /// has none.
        [[nodiscard]] std::optional<std::string_view> attribute(std::string_view name) const;

        /// The value, or fallback when it has no such attribute.
        [[nodiscard]] std::string_view attribute(std::string_view name, std::string_view fallback) const;

        /// An attribute's value, empty when it has none, as std::map and nlohmann::json read by
        /// key: svg["width"]. attribute() or contains() tell an empty value from a missing one.
        [[nodiscard]] std::string_view operator[](const std::string_view name) const { return attribute(name, {}); }

        [[nodiscard]] bool contains(const std::string_view name) const { return attribute(name).has_value(); }

        [[nodiscard]] web::attributes attributes() const;

        /// Its style="..." attribute's declarations, parsed (empty when it has none):
        /// el.style()["color"], el.style().contains("font"), for (const auto &[name, value]: el.style()).
        [[nodiscard]] web::style style() const { return web::style{attribute("style", {})}; }

        [[nodiscard]] elements children() const;

        /// The child elements with this local name.
        [[nodiscard]] elements children(std::string_view name) const;

        /// The first child element with this local name, if any.
        [[nodiscard]] std::optional<element> child(std::string_view name) const;

        /// The element it's in; nothing for the root.
        [[nodiscard]] std::optional<element> parent() const;

        /// All the text inside it, its descendants' included, in document order, entities decoded,
        /// white space as written.
        [[nodiscard]] std::string text() const;

        /// The line it starts on in the source, for messages.
        [[nodiscard]] long line() const;

        bool operator==(const element &other) const { return node_ == other.node_; }

    private:
        friend class document;
        friend class elements;
        element(std::shared_ptr<detail::document_data> document, const void *node) : document_(std::move(document)), node_(node) {
        }

        std::shared_ptr<detail::document_data> document_;
        const void *node_;
    };

    /// A parsed XML document. Entities are decoded in text and attribute values: the five predefined
    /// ones, character references, and those the document's own DOCTYPE declares (as Adobe
    /// Illustrator's SVGs use them), within libxml2's limits on how far they may expand. Nothing is
    /// ever fetched, over the network or from a file (no external DTD or entity). Text in another
    /// encoding is read as UTF-8.
    ///
    ///     const auto doc = sc::web::document::parse(R"(<svg width="20"><rect/></svg>)");
    ///     doc.root().attribute("width");   // "20"
    class document {
    public:
        /// Throws sc::web::error for text that isn't well-formed XML, saying what and on which line.
        [[nodiscard]] static document parse(std::string_view xml);

        /// Throws sc::web::error when the file can't be read or isn't XML.
        [[nodiscard]] static document load(const std::filesystem::path &file);

        /// Its root element.
        [[nodiscard]] element root() const;

    private:
        explicit document(std::shared_ptr<detail::document_data> data) : data_(std::move(data)) {
        }

        std::shared_ptr<detail::document_data> data_;
    };
}
