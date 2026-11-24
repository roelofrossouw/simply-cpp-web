#include <sc/web/markdown.h>
#include <sc/web/html_entities.h>

#include <md4c.h>

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>

namespace sc::web {
    namespace {
        void escape_into(std::string &out, const std::string_view text) {
            for (const char c: text) {
                switch (c) {
                    case '&': out += "&amp;"; break;
                    case '<': out += "&lt;"; break;
                    case '>': out += "&gt;"; break;
                    case '"': out += "&quot;"; break;
                    default: out += c;
                }
            }
        }

        // An attribute md4c hands over (a link's href, an image's src, a title): its text with
        // entities decoded and NUL characters made U+FFFD.
        std::string attribute_of(const MD_ATTRIBUTE &attribute) {
            std::string out;
            if (!attribute.text) return out;
            for (int i = 0; attribute.substr_offsets[i] < attribute.size; ++i) {
                const std::string_view part{attribute.text + attribute.substr_offsets[i], attribute.substr_offsets[i + 1] - attribute.substr_offsets[i]};
                switch (attribute.substr_types[i]) {
                    case MD_TEXT_ENTITY: out += decode_entities(part); break;
                    case MD_TEXT_NULLCHAR: out += "\xEF\xBF\xBD"; break;
                    default: out += part;
                }
            }
            return out;
        }

        const char *alignment(const MD_ALIGN align) {
            switch (align) {
                case MD_ALIGN_LEFT: return " style=\"text-align: left\"";
                case MD_ALIGN_CENTER: return " style=\"text-align: center\"";
                case MD_ALIGN_RIGHT: return " style=\"text-align: right\"";
                default: return "";
            }
        }

        struct renderer {
            markdown_options settings;
            std::string html;
            std::vector<markdown_heading> headings;
            std::map<std::string, int> ids; // each heading id given, and how often

            // While in a heading: its HTML and text, kept until its id is known.
            bool in_heading = false;
            int heading_level = 1;
            std::string heading_html, heading_text;

            // While in an image: its alt text (what's inside its brackets, without markup).
            int image_depth = 0;
            std::string alt;

            std::string &out() { return in_heading ? heading_html : html; }

            void text(const std::string_view plain) {
                if (image_depth) {
                    alt += plain;
                    return;
                }
                escape_into(out(), plain);
                if (in_heading) heading_text += plain;
            }

            std::string unique_id(const std::string &text) {
                std::string id = markdown::anchor(text);
                const int seen = ids[id]++;
                if (seen) id += "-" + std::to_string(seen);
                return id;
            }

            int enter_block(const MD_BLOCKTYPE type, void *detail) {
                switch (type) {
                    case MD_BLOCK_DOC: break;
                    case MD_BLOCK_QUOTE: html += "<blockquote>\n"; break;
                    case MD_BLOCK_UL: html += "<ul>\n"; break;
                    case MD_BLOCK_OL: {
                        const auto *ol = static_cast<const MD_BLOCK_OL_DETAIL *>(detail);
                        html += ol->start == 1 ? std::string{"<ol>\n"} : "<ol start=\"" + std::to_string(ol->start) + "\">\n";
                        break;
                    }
                    case MD_BLOCK_LI: {
                        const auto *li = static_cast<const MD_BLOCK_LI_DETAIL *>(detail);
                        if (li->is_task) html += li->task_mark == ' ' ? "<li class=\"task\">[ ] " : "<li class=\"task done\">[x] ";
                        else html += "<li>";
                        break;
                    }
                    case MD_BLOCK_HR: html += "<hr>\n"; break;
                    case MD_BLOCK_H:
                        in_heading = true;
                        heading_level = static_cast<int>(static_cast<const MD_BLOCK_H_DETAIL *>(detail)->level);
                        heading_html.clear();
                        heading_text.clear();
                        break;
                    case MD_BLOCK_CODE: {
                        const auto *code = static_cast<const MD_BLOCK_CODE_DETAIL *>(detail);
                        const std::string language = attribute_of(code->lang);
                        html += "<pre><code";
                        if (!language.empty()) {
                            html += " class=\"language-";
                            escape_into(html, language);
                            html += '"';
                        }
                        html += '>';
                        break;
                    }
                    case MD_BLOCK_HTML: break;
                    case MD_BLOCK_P: html += "<p>"; break;
                    case MD_BLOCK_TABLE: html += "<table>\n"; break;
                    case MD_BLOCK_THEAD: html += "<thead>\n"; break;
                    case MD_BLOCK_TBODY: html += "<tbody>\n"; break;
                    case MD_BLOCK_TR: html += "<tr>"; break;
                    case MD_BLOCK_TH:
                        html += std::string{"<th"} + alignment(static_cast<const MD_BLOCK_TD_DETAIL *>(detail)->align) + ">";
                        break;
                    case MD_BLOCK_TD:
                        html += std::string{"<td"} + alignment(static_cast<const MD_BLOCK_TD_DETAIL *>(detail)->align) + ">";
                        break;
                    default: break;
                }
                return 0;
            }

            int leave_block(const MD_BLOCKTYPE type, void *) {
                switch (type) {
                    case MD_BLOCK_QUOTE: html += "</blockquote>\n"; break;
                    case MD_BLOCK_UL: html += "</ul>\n"; break;
                    case MD_BLOCK_OL: html += "</ol>\n"; break;
                    case MD_BLOCK_LI: html += "</li>\n"; break;
                    case MD_BLOCK_H: {
                        in_heading = false;
                        markdown_heading heading{heading_level, heading_text, {}};
                        const std::string tag = "h" + std::to_string(heading_level);
                        html += "<" + tag;
                        if (settings.heading_ids) {
                            heading.id = unique_id(heading_text);
                            html += " id=\"";
                            escape_into(html, heading.id);
                            html += '"';
                        }
                        html += ">" + heading_html + "</" + tag + ">\n";
                        headings.push_back(std::move(heading));
                        break;
                    }
                    case MD_BLOCK_CODE: html += "</code></pre>\n"; break;
                    case MD_BLOCK_P: html += "</p>\n"; break;
                    case MD_BLOCK_TABLE: html += "</table>\n"; break;
                    case MD_BLOCK_THEAD: html += "</thead>\n"; break;
                    case MD_BLOCK_TBODY: html += "</tbody>\n"; break;
                    case MD_BLOCK_TR: html += "</tr>\n"; break;
                    case MD_BLOCK_TH: html += "</th>"; break;
                    case MD_BLOCK_TD: html += "</td>"; break;
                    default: break;
                }
                return 0;
            }

            int enter_span(const MD_SPANTYPE type, void *detail) {
                if (image_depth) {
                    if (type == MD_SPAN_IMG) ++image_depth;
                    return 0;
                }
                switch (type) {
                    case MD_SPAN_EM: out() += "<em>"; break;
                    case MD_SPAN_STRONG: out() += "<strong>"; break;
                    case MD_SPAN_CODE: out() += "<code>"; break;
                    case MD_SPAN_DEL: out() += "<del>"; break;
                    case MD_SPAN_U: out() += "<u>"; break;
                    case MD_SPAN_A: {
                        const auto *a = static_cast<const MD_SPAN_A_DETAIL *>(detail);
                        std::string href = attribute_of(a->href);
                        // Bare addresses and www. links (autolinks) get their scheme.
                        if (href.find(':') == std::string::npos) {
                            if (href.find('@') != std::string::npos && href.find('/') == std::string::npos) href = "mailto:" + href;
                            else if (href.starts_with("www.")) href = "http://" + href;
                        }
                        out() += "<a href=\"";
                        escape_into(out(), href);
                        out() += '"';
                        if (const auto title = attribute_of(a->title); !title.empty()) {
                            out() += " title=\"";
                            escape_into(out(), title);
                            out() += '"';
                        }
                        out() += '>';
                        break;
                    }
                    case MD_SPAN_IMG: {
                        const auto *img = static_cast<const MD_SPAN_IMG_DETAIL *>(detail);
                        out() += "<img src=\"";
                        escape_into(out(), attribute_of(img->src));
                        out() += '"';
                        if (const auto title = attribute_of(img->title); !title.empty()) {
                            out() += " title=\"";
                            escape_into(out(), title);
                            out() += '"';
                        }
                        ++image_depth;
                        alt.clear();
                        break;
                    }
                    default: break;
                }
                return 0;
            }

            int leave_span(const MD_SPANTYPE type, void *) {
                if (image_depth) {
                    if (type == MD_SPAN_IMG && --image_depth == 0) {
                        out() += " alt=\"";
                        escape_into(out(), alt);
                        out() += "\">";
                        if (in_heading) heading_text += alt;
                    }
                    return 0;
                }
                switch (type) {
                    case MD_SPAN_EM: out() += "</em>"; break;
                    case MD_SPAN_STRONG: out() += "</strong>"; break;
                    case MD_SPAN_CODE: out() += "</code>"; break;
                    case MD_SPAN_DEL: out() += "</del>"; break;
                    case MD_SPAN_U: out() += "</u>"; break;
                    case MD_SPAN_A: out() += "</a>"; break;
                    default: break;
                }
                return 0;
            }

            int on_text(const MD_TEXTTYPE type, const std::string_view content) {
                switch (type) {
                    case MD_TEXT_NULLCHAR: text("\xEF\xBF\xBD"); break;
                    case MD_TEXT_BR:
                        if (image_depth) alt += ' ';
                        else out() += "<br>\n";
                        break;
                    case MD_TEXT_SOFTBR:
                        if (image_depth) alt += ' ';
                        else out() += '\n';
                        if (in_heading) heading_text += ' ';
                        break;
                    case MD_TEXT_ENTITY: text(decode_entities(content)); break;
                    case MD_TEXT_HTML:
                        if (settings.raw_html && !image_depth) out() += content;
                        else text(content);
                        break;
                    default: text(content);
                }
                return 0;
            }
        };
    }

    markdown::markdown(const std::string_view text, markdown_options settings) {
        renderer state{settings};
        unsigned flags = 0;
        if (settings.tables) flags |= MD_FLAG_TABLES;
        if (settings.strikethrough) flags |= MD_FLAG_STRIKETHROUGH;
        if (settings.task_lists) flags |= MD_FLAG_TASKLISTS;
        if (settings.autolinks) flags |= MD_FLAG_PERMISSIVEURLAUTOLINKS | MD_FLAG_PERMISSIVEWWWAUTOLINKS | MD_FLAG_PERMISSIVEEMAILAUTOLINKS;
        if (!settings.raw_html) flags |= MD_FLAG_NOHTML;
        MD_PARSER parser{};
        parser.abi_version = 0;
        parser.flags = flags;
        parser.enter_block = [](MD_BLOCKTYPE type, void *detail, void *user) { return static_cast<renderer *>(user)->enter_block(type, detail); };
        parser.leave_block = [](MD_BLOCKTYPE type, void *detail, void *user) { return static_cast<renderer *>(user)->leave_block(type, detail); };
        parser.enter_span = [](MD_SPANTYPE type, void *detail, void *user) { return static_cast<renderer *>(user)->enter_span(type, detail); };
        parser.leave_span = [](MD_SPANTYPE type, void *detail, void *user) { return static_cast<renderer *>(user)->leave_span(type, detail); };
        parser.text = [](MD_TEXTTYPE type, const MD_CHAR *content, MD_SIZE size, void *user) {
            return static_cast<renderer *>(user)->on_text(type, std::string_view{content, size});
        };
        if (md_parse(text.data(), static_cast<MD_SIZE>(text.size()), &parser, &state) != 0)
            throw std::runtime_error("sc::web::markdown: md4c couldn't parse the text (out of memory)");
        html_ = std::move(state.html);
        headings_ = std::move(state.headings);
    }

    std::string markdown::anchor(const std::string_view text) {
        std::string out;
        for (const char c: text) {
            const auto byte = static_cast<unsigned char>(c);
            if (byte >= 0x80) out += c; // part of a letter outside ASCII: kept, as GitHub keeps letters
            else if (std::isalnum(byte) || c == '-' || c == '_') out += static_cast<char>(std::tolower(byte));
            else if (std::isspace(byte)) out += '-';
        }
        return out;
    }
}
