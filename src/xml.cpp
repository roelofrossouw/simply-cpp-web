#include "xml.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <deque>
#include <fstream>
#include <iterator>
#include <mutex>

namespace sc::web {
    namespace detail {
        struct document_data {
            xmlDocPtr document;
            // Strings the tree doesn't hold in one piece ("prefix:name", an attribute value split
            // around an entity reference), kept for the document's lifetime so views of them stay
            // valid. A deque doesn't move what it holds.
            std::deque<std::string> kept;
            std::mutex keeping;

            explicit document_data(const xmlDocPtr doc) : document(doc) {
            }

            std::string_view keep(std::string text) {
                const std::lock_guard lock{keeping};
                return kept.emplace_back(std::move(text));
            }

            ~document_data() { xmlFreeDoc(document); }

            document_data(const document_data &) = delete;
            document_data &operator=(const document_data &) = delete;
        };
    }

    namespace {
        const xmlNode *as_node(const void *node) { return static_cast<const xmlNode *>(node); }

        const xmlAttr *as_attribute(const void *at) { return static_cast<const xmlAttr *>(at); }

        std::string_view view(const xmlChar *text) {
            return text ? std::string_view{reinterpret_cast<const char *>(text)} : std::string_view{};
        }

        // "prefix:name", or "name" without a namespace prefix.
        std::string qualified(const xmlNs *ns, const xmlChar *name) {
            std::string result;
            if (ns && ns->prefix) result.append(view(ns->prefix)).push_back(':');
            return result.append(view(name));
        }

        // libxml2 wants initialising once, before threads parse at the same time.
        void initialise() {
            static std::once_flag once;
            std::call_once(once, [] { xmlInitParser(); });
        }

        // An attribute's name as written, and its value: in place when it's one text node (the usual
        // case), else joined and kept.
        std::string_view name_of(detail::document_data &data, const xmlAttr *at) {
            if (!at->ns || !at->ns->prefix) return view(at->name);
            return data.keep(qualified(at->ns, at->name));
        }

        std::string_view value_of(detail::document_data &data, const xmlAttr *at) {
            const xmlNode *value = at->children;
            if (!value) return {};
            if (!value->next && value->type == XML_TEXT_NODE) return view(value->content);
            xmlChar *joined = xmlNodeListGetString(at->doc, value, 1);
            std::string text{view(joined)};
            xmlFree(joined);
            return data.keep(std::move(text));
        }

        // The first element node from node on, among its siblings, with name (any, if empty).
        const void *next_element(const xmlNode *node, const std::string *name) {
            for (; node; node = node->next) {
                if (node->type == XML_ELEMENT_NODE && (!name || name->empty() || view(node->name) == *name)) return node;
            }
            return nullptr;
        }
    }

    // attributes

    attribute attributes::iterator::operator*() const {
        const auto *at = as_attribute(at_);
        return {name_of(*document_, at), value_of(*document_, at)};
    }

    attributes::iterator &attributes::iterator::operator++() {
        at_ = as_attribute(at_)->next;
        return *this;
    }

    attributes::iterator attributes::iterator::operator++(int) {
        auto before = *this;
        ++*this;
        return before;
    }

    // elements

    elements::iterator::iterator(std::shared_ptr<detail::document_data> document, std::shared_ptr<const std::string> name, const void *at)
        : document_(std::move(document)), name_(std::move(name)), at_(next_element(as_node(at), name_.get())) {
    }

    element elements::iterator::operator*() const { return {document_, at_}; }

    elements::iterator &elements::iterator::operator++() {
        at_ = next_element(as_node(at_)->next, name_.get());
        return *this;
    }

    elements::iterator elements::iterator::operator++(int) {
        auto before = *this;
        ++*this;
        return before;
    }

    // element

    std::string_view element::name() const & { return view(as_node(node_)->name); }

    std::string element::qualified_name() const { return qualified(as_node(node_)->ns, as_node(node_)->name); }

    std::optional<std::string_view> element::attribute(const std::string_view name) const & {
        for (const xmlAttr *at = as_node(node_)->properties; at; at = at->next) {
            const bool prefixed = at->ns && at->ns->prefix;
            if (prefixed ? qualified(at->ns, at->name) == name : view(at->name) == name) return value_of(*document_, at);
        }
        return std::nullopt;
    }

    std::string_view element::attribute(const std::string_view name, const std::string_view fallback) const & {
        return attribute(name).value_or(fallback);
    }

    web::attributes element::attributes() const { return {document_, as_node(node_)->properties}; }

    elements element::children() const { return {document_, as_node(node_)->children, {}}; }

    elements element::children(const std::string_view name) const { return {document_, as_node(node_)->children, std::string{name}}; }

    std::optional<element> element::child(const std::string_view name) const {
        const std::string wanted{name};
        if (const void *found = next_element(as_node(node_)->children, &wanted)) return element{document_, found};
        return std::nullopt;
    }

    std::optional<element> element::parent() const {
        const xmlNode *up = as_node(node_)->parent;
        if (!up || up->type != XML_ELEMENT_NODE) return std::nullopt;
        return element{document_, up};
    }

    std::string element::text() const {
        xmlChar *all = xmlNodeGetContent(as_node(node_));
        std::string result{view(all)};
        xmlFree(all);
        return result;
    }

    long element::line() const { return xmlGetLineNo(as_node(node_)); }

    // document

    document document::parse(const std::string_view xml) {
        initialise();
        xmlParserCtxtPtr context = xmlNewParserCtxt();
        if (!context) throw error{"sc::web: out of memory"};
        // No network, and no external DTD or entity loaded (no XML_PARSE_DTDLOAD or NOENT); a
        // DOCTYPE's own entities stay references, expanded when read, within libxml2's limits on
        // amplification. Errors come back here rather than printed.
        constexpr int options = XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING;
        xmlDocPtr parsed = xmlCtxtReadMemory(context, xml.data(), static_cast<int>(xml.size()), nullptr, nullptr, options);
        if (!parsed || !xmlDocGetRootElement(parsed)) {
            std::string message = "not XML";
            if (const xmlError *problem = xmlCtxtGetLastError(context); problem && problem->message) {
                message = std::string{problem->message};
                while (!message.empty() && (message.back() == '\n' || message.back() == ' ')) message.pop_back();
                message += " (line " + std::to_string(problem->line) + ")";
            }
            if (parsed) xmlFreeDoc(parsed);
            xmlFreeParserCtxt(context);
            throw error{"sc::web: " + message};
        }
        xmlFreeParserCtxt(context);
        return document{std::make_shared<detail::document_data>(parsed)};
    }

    document document::load(const std::filesystem::path &file) {
        std::ifstream in{file, std::ios::binary};
        if (!in) throw error{"sc::web: can't read " + file.string()};
        const std::string text{std::istreambuf_iterator<char>{in}, {}};
        return parse(text);
    }

    element document::root() const { return {data_, xmlDocGetRootElement(data_->document)}; }
}
