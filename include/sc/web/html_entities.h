#pragma once
// HTML character references, encoded and decoded the way Perl's HTML::HTML5::Entities 0.004 does it
// (its encode_entities and decode_entities with their defaults), so code that used it gets the same
// text from these. UTF-8 in and out; bytes that aren't UTF-8 pass through unchanged.
//
//     sc::web::encode_entities("Café <b> & “quotes”")   // Café &lt;b&gt; &amp; &ldquo;quotes&rdquo;
//     sc::web::decode_entities("&lt;p&gt;caf&eacute;&#x2014;&#169")   // <p>café—©
//
// encode_entities escapes what isn't a word character (Unicode letters, digits, marks and _ stay as
// they are), white space or plain ASCII punctuation: so " & ' < >, control characters and symbols
// such as € or emoji. HTML 4's named entity is used when there is one (&euro;, &nbsp;, &quot;),
// else a hex one (&#x27; for ', &#x1f600;).
//
// decode_entities replaces every named reference of the WHATWG HTML Standard (2125 names, a few to
// two code points), decimal &#169; and hex &#xa9;, each with or without its closing ';'. A name runs
// over all the word characters after '&', and one that isn't known is left as it is (&bogus; stays).
// A number that isn't a Unicode scalar value (0, a surrogate, above U+10FFFF) gives U+FFFD, where
// Perl would make text PostgreSQL refuses; numbers are taken as they are, without HTML's Windows-1252
// remapping of &#128; to &#159; (as Perl did).

#include <string>
#include <string_view>

namespace sc::web {
    [[nodiscard]] std::string encode_entities(std::string_view text);
    [[nodiscard]] std::string decode_entities(std::string_view text);
}

// sc::html was this module's second namespace until 2.1.0. Its two functions stay, defined in the
// library (programs built against 2.0 call them), until the next major.
namespace sc::html {
    [[deprecated("use sc::web::encode_entities")]] [[nodiscard]] std::string encode_entities(std::string_view text);
    [[deprecated("use sc::web::decode_entities")]] [[nodiscard]] std::string decode_entities(std::string_view text);
}
