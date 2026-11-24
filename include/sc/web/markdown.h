#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace sc::web {
    /// What sc::web::markdown accepts beyond CommonMark.
    struct markdown_options {
        /// GitHub's tables, ~~strikethrough~~, - [x] task lists and bare www./http:// links and
        /// addresses.
        bool tables = true;
        bool strikethrough = true;
        bool task_lists = true;
        bool autolinks = true;
        /// Every heading gets an id as GitHub makes them ("## 1.2 In scope" is "12-in-scope"; a
        /// second heading of the same text "-1" after it), so [links](#12-in-scope) reach it.
        bool heading_ids = true;
        /// HTML written in the Markdown goes through as it is; by default it's text, escaped.
        bool raw_html = false;
    };

    /// A heading of the document: its level (1 for #), its text without markup, and its id (empty
    /// without heading_ids).
    struct markdown_heading {
        int level = 1;
        std::string text;
        std::string id;

        bool operator==(const markdown_heading &) const = default;
    };

    /// Markdown (CommonMark, with GitHub's tables, strikethrough, task lists and autolinks) as HTML,
    /// parsed by md4c. Text is escaped; images are <img> with their alt, tables keep their columns'
    /// alignment (style="text-align: ..."), fenced code is <pre><code class="language-x">.
    ///
    ///     const sc::web::markdown page{"# Report\n\n| Item | Amount |\n|---|--:|\n| Tea | 12.50 |\n"};
    ///     page.html();               // <h1 id="report">Report</h1><table>...
    ///     page.headings()[0].text;   // "Report"
    class markdown {
    public:
        explicit markdown(std::string_view text, markdown_options settings = {});

        [[nodiscard]] const std::string &html() const { return html_; }

        /// Every heading, in order.
        [[nodiscard]] const std::vector<markdown_heading> &headings() const { return headings_; }

        /// The id GitHub gives a heading's text: lower case, spaces to hyphens, and anything but
        /// letters, digits, "-" and "_" left out ("6.4 Backup layers" is "64-backup-layers").
        [[nodiscard]] static std::string anchor(std::string_view text);

    private:
        std::string html_;
        std::vector<markdown_heading> headings_;
    };
}
