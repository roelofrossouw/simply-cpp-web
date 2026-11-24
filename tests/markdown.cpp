// sc::web::markdown: blocks, spans, tables with alignment, code, images, escaping, autolinks,
// task lists and GitHub's heading ids.
#include <sc/web/markdown.h>

#include "sc_test.h"

#include <string>

using sc::web::markdown;

static bool has(const std::string &html, const std::string &part) { return html.find(part) != std::string::npos; }

int main() {
    SECTION("Headings, paragraphs and spans");
    {
        const markdown page{"# Report\n\nSome *em*, **strong**, `code` and ~~gone~~ text.\n\n## 1.2 In scope\n"};
        CHECK(has(page.html(), "<h1 id=\"report\">Report</h1>"));
        CHECK(has(page.html(), "<p>Some <em>em</em>, <strong>strong</strong>, <code>code</code> and <del>gone</del> text.</p>"));
        CHECK(has(page.html(), "<h2 id=\"12-in-scope\">1.2 In scope</h2>"));
        CHECK_EQ(page.headings().size(), std::size_t{2});
        CHECK_EQ(page.headings()[1], (sc::web::markdown_heading{2, "1.2 In scope", "12-in-scope"}));
    }

    SECTION("GitHub's ids: punctuation out, repeats numbered, markup's text kept");
    {
        CHECK_EQ(markdown::anchor("Appendix A: Containers by host (23 November 2026)"), std::string{"appendix-a-containers-by-host-23-november-2026"});
        CHECK_EQ(markdown::anchor("6.4 Backup layers and off-site design (planned)"), std::string{"64-backup-layers-and-off-site-design-planned"});
        CHECK_EQ(markdown::anchor("Tier 0 on hv3: hv3 fetches, the data centre cannot delete"),
                 std::string{"tier-0-on-hv3-hv3-fetches-the-data-centre-cannot-delete"});
        const markdown page{"## Notes\n\n## Notes\n\n### The `vault` *folder*\n"};
        CHECK_EQ(page.headings()[0].id, std::string{"notes"});
        CHECK_EQ(page.headings()[1].id, std::string{"notes-1"});
        CHECK_EQ(page.headings()[2].text, std::string{"The vault folder"});
        CHECK_EQ(page.headings()[2].id, std::string{"the-vault-folder"});
        const markdown plain{"## Notes\n", {.heading_ids = false}};
        CHECK(has(plain.html(), "<h2>Notes</h2>"));
    }

    SECTION("Tables keep their columns' alignment");
    {
        const markdown page{"| Item | Qty | Amount |\n|:---|:-:|--:|\n| Tea | 2 | 12.50 |\n"};
        CHECK(has(page.html(), "<table>"));
        CHECK(has(page.html(), "<thead>"));
        CHECK(has(page.html(), "<th style=\"text-align: left\">Item</th>"));
        CHECK(has(page.html(), "<th style=\"text-align: center\">Qty</th>"));
        CHECK(has(page.html(), "<td style=\"text-align: right\">12.50</td>"));
        const markdown untabled{"| A |\n|---|\n| b |\n", {.tables = false}};
        CHECK(!has(untabled.html(), "<table>"));
    }

    SECTION("Lists, quotes, rules and task lists");
    {
        const markdown page{"- one\n- two\n  1. nested\n\n3. three\n4. four\n\n> quoted\n\n---\n\n- [ ] todo\n- [x] done\n"};
        CHECK(has(page.html(), "<ul>\n<li>one</li>"));
        CHECK(has(page.html(), "<ol>\n<li>nested</li>"));
        CHECK(has(page.html(), "<ol start=\"3\">"));
        CHECK(has(page.html(), "<blockquote>\n<p>quoted</p>"));
        CHECK(has(page.html(), "<hr>"));
        CHECK(has(page.html(), "<li class=\"task\">[ ] todo</li>"));
        CHECK(has(page.html(), "<li class=\"task done\">[x] done</li>"));
    }

    SECTION("Code is escaped, its language a class");
    {
        const markdown page{"```sql\nselect * from t where a < 3 & b > 1;\n```\n"};
        CHECK(has(page.html(), "<pre><code class=\"language-sql\">select * from t where a &lt; 3 &amp; b &gt; 1;\n</code></pre>"));
    }

    SECTION("Links, autolinks and images");
    {
        const markdown page{"See [the plan](#1-purpose \"Plan\"), www.example.com, https://example.com/a?b=1&c=2 and info@example.com.\n\n"
                            "![The **logo**](images/logo.svg)\n"};
        CHECK(has(page.html(), "<a href=\"#1-purpose\" title=\"Plan\">the plan</a>"));
        CHECK(has(page.html(), "<a href=\"http://www.example.com\">www.example.com</a>"));
        CHECK(has(page.html(), "<a href=\"https://example.com/a?b=1&amp;c=2\">"));
        CHECK(has(page.html(), "<a href=\"mailto:info@example.com\">info@example.com</a>"));
        CHECK(has(page.html(), "<img src=\"images/logo.svg\" alt=\"The logo\">"));
    }

    SECTION("HTML is text unless raw_html, entities decoded and escaped again");
    {
        const markdown page{"A <b>tag</b> &amp; &copy; \"quotes\"\n"};
        CHECK(has(page.html(), "A &lt;b&gt;tag&lt;/b&gt; &amp; \xC2\xA9 &quot;quotes&quot;"));
        CHECK(has(markdown{"A <b>tag</b>\n", {.raw_html = true}}.html(), "A <b>tag</b>"));
    }

    TEST_SUMMARY();
}
