#include <cthtml.hpp>

void empty_symbol_17() { }

// the HTML5 feature matrix: every convenience and every refusal, one
// by one (tests/document.cpp exercises them together on a real page)
#if CTLL_CNTTP_COMPILER_CHECK

#include <string_view>
#include <type_traits>
using namespace std::literals;

// --- void elements
static_assert(cthtml::is_valid<"<br>">);
static_assert(cthtml::is_valid<"<br/>">); // XML habit, same element
static_assert(std::is_same_v<decltype(cthtml::parse<"<br>">()), decltype(cthtml::parse<"<br/>">())>);
static_assert(cthtml::is_valid<"<meta charset=utf-8><link rel=x><hr><wbr><embed src=x>">);
static_assert(!cthtml::is_valid<"</br>">);  // voids take no close tag
static_assert(!cthtml::is_valid<"</img>">);
static_assert(cthtml::parse<"<img src=x>">().get<"body">().get<"img">().empty());

// --- self-closing syntax is only for voids
static_assert(!cthtml::is_valid<"<div/>">);
static_assert(!cthtml::is_valid<"<span/>">);
static_assert(!cthtml::is_valid<"<body/>">);

// --- optional end tags: the auto-close table
static_assert(cthtml::parse<"<p>a<p>b<p>c">().get<"body">().count<"p">() == 3);
static_assert(cthtml::parse<"<p>a<div>b</div>">().get<"body">().count<"p">() == 1);
static_assert(cthtml::parse<"<ul><li>a<li>b<li>c</ul>">()
                  .get<"body">().get<"ul">().count<"li">() == 3);
static_assert(cthtml::parse<"<ul><li>a<ul><li>b</ul></ul>">()          // nested list
                  .get<"body">().get<"ul">().get<"li">().count<"ul">() == 1);
static_assert(cthtml::parse<"<dl><dt>t<dd>d<dt>t2<dd>d2</dl>">()
                  .get<"body">().get<"dl">().child_count() == 4);
static_assert(cthtml::parse<"<select><option>a<option>b</select>">()
                  .get<"body">().get<"select">().count<"option">() == 2);
static_assert(cthtml::parse<"<select><optgroup><option>a<optgroup><option>b</select>">()
                  .get<"body">().get<"select">().count<"optgroup">() == 2);
static_assert(cthtml::parse<"<table><thead><tr><th>h<tbody><tr><td>1<tr><td>2</table>">()
                  .get<"body">().get<"table">().get<"tbody">().count<"tr">() == 2);
static_assert(cthtml::parse<"<ruby>base<rt>above<rp>(</rp></ruby>">()
                  .get<"body">().get<"ruby">().count<"rt">() == 1);
// a block-level open tag closes <p>
static_assert(cthtml::parse<"<p>a<table><tr><td>x</table>">().get<"body">().count<"p">() == 1);
// EOF closes whatever is still open
static_assert(cthtml::is_valid<"<div><ul><li>deep">);

// --- implied html/head/body
constexpr auto frag = cthtml::parse<"<p>hi">();
static_assert(frag.name() == "html");
static_assert(frag.child<0>().name() == "head");
static_assert(frag.child<1>().name() == "body");
static_assert(frag.get<"head">().empty());
static_assert(frag.get<"body">().get<"p">().text() == "hi"sv);
static_assert(cthtml::parse<"">().child_count() == 2); // even empty input
// metadata before any content collects into head; once body content
// starts, everything (metadata included) stays in body
constexpr auto split = cthtml::parse<"<meta charset=x><title>t</title><p>a</p>">();
static_assert(split.get<"head">().contains<"meta">());
static_assert(split.get<"head">().contains<"title">());
static_assert(split.get<"body">().contains<"p">());
static_assert(cthtml::parse<"<p>a</p><style>x{}</style>">()
                  .get<"body">().contains<"style">());
// explicit structure tags merge their attributes, at any position
static_assert(cthtml::parse<"<html lang=en><body class=c><p>x">().attribute<"lang">() == "en"sv);
static_assert(cthtml::parse<"<body class=c><p>x">().get<"body">().attribute<"class">() == "c"sv);
// text after </body> still lands in body (spec recovery)
static_assert(cthtml::parse<"<body><p>x</p></body>tail">().get<"body">().text() == "tail"sv);
// </body>/</html> close open omissible elements, but not real ones
static_assert(cthtml::is_valid<"<p>x</body></html>">);
static_assert(!cthtml::is_valid<"<div>x</body>">);
static_assert(!cthtml::is_valid<"<p>x</head>">); // head is long closed

// --- case-insensitivity
static_assert(cthtml::is_valid<"<DIV>x</div>">);
static_assert(cthtml::is_valid<"<Ul><LI>a<li>b</uL>">);
static_assert(cthtml::parse<"<DIV>x</div>">().get<"body">().get<"div">().name() == "div"sv);
static_assert(cthtml::parse<"<p ID=x>t</p>">().get<"body">().get<"p">().attribute<"id">() == "x"sv);
static_assert(cthtml::parse<"<p id=x>t</p>">().get<"body">().get<"P">().attribute<"ID">() == "x"sv);

// --- attribute flavours
constexpr auto attrs = cthtml::parse<R"(<p a=1 b="two" c='three' d e-f=x_y></p>)">()
                           .get<"body">().get<"p">();
static_assert(attrs.attribute_count() == 5);
static_assert(attrs.attribute<"a">() == "1"sv);
static_assert(attrs.attribute<"b">() == "two"sv);
static_assert(attrs.attribute<"c">() == "three"sv);
static_assert(attrs.attribute<"d">() == ""sv); // boolean
static_assert(attrs.has_attribute<"d">());
static_assert(attrs.attribute<"e-f">() == "x_y"sv);
static_assert(!cthtml::is_valid<"<p x=1 X=2></p>">); // duplicates fold case too

// --- DOCTYPE: accepted and skipped, any case, with legacy strings
static_assert(cthtml::is_valid<"<!DOCTYPE html><p>x">);
static_assert(cthtml::is_valid<"<!doctype HTML><p>x">);
static_assert(cthtml::is_valid<R"(<!DOCTYPE html SYSTEM "about:legacy-compat"><p>x)">);
static_assert(cthtml::parse<"<!DOCTYPE html><p>x">().get<"body">().child_count() == 1);

// --- raw text (<script>, <style>) and RCDATA (<title>, <textarea>)
static_assert(cthtml::parse<R"(<script>if(a<b&&c>d){s="</p>"}</script>)">()
                  .get<"head">().get<"script">().text() == R"(if(a<b&&c>d){s="</p>"})"sv);
static_assert(cthtml::parse<"<SCRIPT>x</SCRIPT>">().get<"head">().get<"script">().text() == "x"sv);
static_assert(cthtml::parse<"<script>x</script >">().get<"head">().get<"script">().text() == "x"sv);
static_assert(cthtml::parse<"<style>a>b{x:1}</style>">()
                  .get<"head">().get<"style">().text() == "a>b{x:1}"sv);
// <scripty> is an ordinary element, longest match wins
static_assert(cthtml::parse<"<scripty>a</scripty>">()
                  .get<"body">().get<"scripty">().text() == "a"sv);
// raw text is NOT decoded; RCDATA is
static_assert(cthtml::parse<"<script>&amp;</script>">()
                  .get<"head">().get<"script">().text() == "&amp;"sv);
static_assert(cthtml::parse<"<title>&amp;</title>">()
                  .get<"head">().get<"title">().text() == "&"sv);
// textarea preserves whitespace, minus one leading newline
static_assert(cthtml::parse<"<textarea>  a  </textarea>">()
                  .get<"body">().get<"textarea">().text() == "  a  "sv);
static_assert(cthtml::parse<"<textarea>\n  a\n</textarea>">()
                  .get<"body">().get<"textarea">().text() == "  a\n"sv);
// a raw-text element must reach its close tag
static_assert(!cthtml::is_valid<"<script>alert(1)">);
static_assert(!cthtml::is_valid<"<style>p{}">);

// --- <pre>: whitespace preserved, one leading newline dropped
static_assert(cthtml::parse<"<pre>\ntext\n</pre>">()
                  .get<"body">().get<"pre">().text() == "text\n"sv);
static_assert(cthtml::parse<"<pre>  <b>k</b>  </pre>">()
                  .get<"body">().get<"pre">().child_count() == 3);
// blank text elsewhere is dropped
static_assert(cthtml::parse<"<div>  <b>k</b>  </div>">()
                  .get<"body">().get<"div">().child_count() == 1);

// --- comments and CDATA
static_assert(cthtml::is_valid<"<!-- -- nested dashes -- --><p>x">);
static_assert(cthtml::is_valid<"<p><!-- inside -->x</p>">);
static_assert(cthtml::is_valid<"<p><![CDATA[dropped]]></p>">);
static_assert(cthtml::parse<"<p>a<!-- c -->b</p>">()
                  .get<"body">().get<"p">().text() == "ab"sv); // merged across

// --- the refusals, with their reasons
static_assert(cthtml::bind_error<"<b><i>x</b></i>">().reason ==
              cthtml::bind_reason::mismatched_tag);
static_assert(cthtml::bind_error<"<ul>x</div></ul>">().reason ==
              cthtml::bind_reason::stray_end_tag);
static_assert(cthtml::bind_error<"<p a=1 a=2></p>">().reason ==
              cthtml::bind_reason::duplicate_attribute);
static_assert(cthtml::bind_error<"<article/>">().reason ==
              cthtml::bind_reason::self_closing_non_void);
// a close tag may skip omissible end tags, not real ones
static_assert(cthtml::is_valid<"<ul><li><p>x</ul>">);   // </ul> closes p, li
static_assert(!cthtml::is_valid<"<div><b>x</div>">);    // </div> cannot close b

void empty_symbol() { }

#endif
