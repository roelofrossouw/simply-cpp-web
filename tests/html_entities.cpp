// sc::html's entities against what 1web's database functions gave on db3 (Perl's
// HTML::HTML5::Entities 0.004: html_encode and html_decode_work), recorded 2026-11-07.
#include <sc/web/html_entities.h>

#include "sc_test.h"

#include <string>

namespace {
    std::string s(const char8_t *text) { return reinterpret_cast<const char *>(text); }
}

int main() {
    using sc::html::decode_entities;
    using sc::html::encode_entities;

    SECTION("encode_entities as Perl's");
    {
        CHECK_EQ(encode_entities(s(u8"")), s(u8""));
        CHECK_EQ(encode_entities(s(u8"plain ascii text 123")), s(u8"plain ascii text 123"));
        CHECK_EQ(encode_entities(s(u8"a<b>&c\"d'e")), s(u8"a&lt;b&gt;&amp;c&quot;d&#x27;e"));
        CHECK_EQ(encode_entities(s(u8"Café é ü ß ñ Ñ")), s(u8"Café é ü ß ñ Ñ"));
        CHECK_EQ(encode_entities(s(u8"€ £ ¥ © ® ™ — – … • ·")), s(u8"&euro; &pound; &yen; &copy; &reg; &trade; &mdash; &ndash; &hellip; &bull; &middot;"));
        CHECK_EQ(encode_entities(s(u8"中文 日本語 한국어")), s(u8"中文 日本語 한국어"));
        CHECK_EQ(encode_entities(s(u8"😀 🚀 👍🏽")), s(u8"&#x1f600; &#x1f680; &#x1f44d;&#x1f3fd;"));
        CHECK_EQ(encode_entities(s(u8"\x09""\x0a""\x0d"" tab")), s(u8"\x09""\x0a""\x0d"" tab"));
        CHECK_EQ(encode_entities(s(u8"\x01""\x02""\x1f""\x7f""")), s(u8"&#x1;&#x2;&#x1f;&#x7f;"));
        CHECK_EQ(encode_entities(s(u8" nbsp­")), s(u8"&nbsp;nbsp&shy;"));
        CHECK_EQ(encode_entities(s(u8"math ∑ ∫ √ ∞ ≠ ≤")), s(u8"math &sum; &int; &radic; &infin; &ne; &le;"));
        CHECK_EQ(encode_entities(s(u8"arrows ← → ↑")), s(u8"arrows &larr; &rarr; &uarr;"));
        CHECK_EQ(encode_entities(s(u8"greek αβγ ΩΣ")), s(u8"greek αβγ ΩΣ"));
        CHECK_EQ(encode_entities(s(u8"ª º µ ¹ ² ³ ¼")), s(u8"ª º µ &sup1; &sup2; &sup3; &frac14;"));
        CHECK_EQ(encode_entities(s(u8"₂ ⁰ ℃ ℉")), s(u8"&#x2082; &#x2070; &#x2103; &#x2109;"));
        CHECK_EQ(encode_entities(s(u8"! # $ % ( ) * + , - . / : ; = ? @ [ \\ ] ^ _ ` { | } ~")), s(u8"! # $ % ( ) * + , - . / : ; = ? @ [ \\ ] ^ _ ` { | } ~"));
        CHECK_EQ(encode_entities(s(u8"‍‌ zwj")), s(u8"‍‌ zwj"));
        CHECK_EQ(encode_entities(s(u8"Ǆ ǅ")), s(u8"Ǆ ǅ"));
        CHECK_EQ(encode_entities(s(u8"á combining")), s(u8"á combining"));
        CHECK_EQ(encode_entities(s(u8"٠١ arabic digits")), s(u8"٠١ arabic digits"));
        CHECK_EQ(encode_entities(s(u8"fullwidth Ａ１")), s(u8"fullwidth Ａ１"));
        CHECK_EQ(encode_entities(s(u8"  line sep")), s(u8"&#x2028; line sep"));
        CHECK_EQ(encode_entities(s(u8"﻿ bom")), s(u8"&#xfeff; bom"));
        CHECK_EQ(encode_entities(s(u8"<script>alert('x')</script>")), s(u8"&lt;script&gt;alert(&#x27;x&#x27;)&lt;/script&gt;"));
    }

    SECTION("decode_entities as Perl's");
    {
        CHECK_EQ(decode_entities(s(u8"")), s(u8""));
        CHECK_EQ(decode_entities(s(u8"no refs")), s(u8"no refs"));
        CHECK_EQ(decode_entities(s(u8"&amp;&lt;&gt;&quot;&apos;")), s(u8"&<>\"'"));
        CHECK_EQ(decode_entities(s(u8"&amp&lt&gt")), s(u8"&<>"));
        CHECK_EQ(decode_entities(s(u8"&eacute;&Eacute;&szlig;")), s(u8"éÉß"));
        CHECK_EQ(decode_entities(s(u8"&#65;&#x42;&#X43;&#0065;&#x00e9;")), s(u8"ABCAé"));
        CHECK_EQ(decode_entities(s(u8"&#169&#xa9")), s(u8"©©"));
        CHECK_EQ(decode_entities(s(u8"&notin;&notit;&notin")), s(u8"∉&notit;∉"));
        CHECK_EQ(decode_entities(s(u8"&NotEqualTilde;&nGt;")), s(u8"≂̸≫⃒"));
        CHECK_EQ(decode_entities(s(u8"&bogus; &bogus &")), s(u8"&bogus; &bogus &"));
        CHECK_EQ(decode_entities(s(u8"& alone && &;")), s(u8"& alone && &;"));
        CHECK_EQ(decode_entities(s(u8"&#; &#x; &#xg; &#")), s(u8"&#; &#x; &#xg; &#"));
        CHECK_EQ(decode_entities(s(u8"&#128;&#150;&#159;")), s(u8""));
        CHECK_EQ(decode_entities(s(u8"&#x1F600;&#128512;")), s(u8"😀😀"));
        CHECK_EQ(decode_entities(s(u8"&AElig&AElig;")), s(u8"ÆÆ"));
        CHECK_EQ(decode_entities(s(u8"&lt;x&gtx")), s(u8"<x&gtx"));
        CHECK_EQ(decode_entities(s(u8"&ltx;")), s(u8"&ltx;"));
        CHECK_EQ(decode_entities(s(u8"&nbsp;&shy;")), s(u8" ­"));
        CHECK_EQ(decode_entities(s(u8"caf&eacuteé;")), s(u8"caf&eacuteé;"));
        CHECK_EQ(decode_entities(s(u8"&amp;amp;")), s(u8"&amp;"));
        CHECK_EQ(decode_entities(s(u8"&Afr;&fjlig;&ThickSpace;")), s(u8"𝔄fj  "));
        CHECK_EQ(decode_entities(s(u8"text &copy 2024")), s(u8"text © 2024"));
        CHECK_EQ(decode_entities(s(u8"&eacute_")), s(u8"&eacute_"));
        CHECK_EQ(decode_entities(s(u8"&amp1;")), s(u8"&amp1;"));
        CHECK_EQ(decode_entities(s(u8"&amp;lt;")), s(u8"&lt;"));
    }

    SECTION("Numbers that aren't characters give U+FFFD (Perl gave text PostgreSQL refused)");
    {
        CHECK_EQ(decode_entities("&#xD800; &#1114112; &#99999999999999; &#0;"), s(u8"\ufffd \ufffd \ufffd \ufffd"));
    }

    SECTION("Bytes that aren't UTF-8 pass through");
    {
        CHECK_EQ(encode_entities("a\xff<"), std::string{"a\xff&lt;"});
        CHECK_EQ(decode_entities("\xff&amp;"), std::string{"\xff&"});
    }

    SECTION("Round trip");
    {
        const auto text = s(u8"<a href=\"x?a=1&b=2\">Caf\u00e9 \u20ac5 \U0001f600</a>");
        CHECK_EQ(decode_entities(encode_entities(text)), text);
    }
    TEST_SUMMARY();
}
