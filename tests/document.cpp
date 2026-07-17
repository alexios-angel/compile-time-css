#include <cthtml.hpp>

void empty_symbol_17() { }

// the string-literal API needs C++20 class-type template parameters;
// tests/cxx17.cpp covers the C++17 variable form
#if CTLL_CNTTP_COMPILER_CHECK

#include <string_view>
using namespace std::literals;

// --- basics: HTML5 conveniences parse silently
static_assert(cthtml::is_valid<"<p>fragments are fine">);
static_assert(cthtml::is_valid<"<br>">);
static_assert(cthtml::is_valid<"<div><p>EOF closes everything">);
static_assert(cthtml::is_valid<"  \n  ">);
static_assert(cthtml::is_valid<"">);
static_assert(cthtml::is_valid<"<!DOCTYPE html><title>t</title>">);

// --- THE feature: author mistakes are a compile-time property
static_assert(!cthtml::is_valid<"<b><i>x</b></i>">);      // crossing close tag
static_assert(!cthtml::is_valid<"<p>x</p></p>">);         // stray close tag
static_assert(!cthtml::is_valid<"<a x='1' x='2'></a>">);  // duplicate attribute
static_assert(!cthtml::is_valid<"<div/>">);               // self-closed non-void
static_assert(!cthtml::is_valid<"</br>">);                // closing a void
static_assert(!cthtml::is_valid<"a < b">);                // raw < in text
static_assert(!cthtml::is_valid<"<script>unclosed">);     // raw text needs its close

// --- a real document
constexpr auto doc = cthtml::parse<R"(
<!DOCTYPE html>
<!-- landing page -->
<html lang="en">
<head>
	<meta charset=utf-8>
	<title>Demo &amp; Docs</title>
	<link rel=stylesheet href=/main.css>
	<style>p em{color:#333}</style>
	<script type=module>if(a<b){go("</div>")}</script>
</head>
<body class=main data-x='1'>
	<h1 id=top>Hello</h1>
	<p>one<p>two
	<ul><li>alpha<li>beta</ul>
	<table><tr><td>a<td>b<tr><td>c<td>d</table>
	<img src=logo.png alt="the logo">
	<input type=checkbox checked>
	<pre>
  keep   this</pre>
	<textarea name=msg>
draft</textarea>
	<form action=/send method=POST><button disabled>Go</button></form>
</body>
</html>
)">();

// every parse yields html > (head, body), like a browser DOM
static_assert(doc.name() == "html");
static_assert(doc.attribute<"lang">() == "en"sv);
static_assert(doc.child_count() == 2);
constexpr auto head = doc.get<"head">();
constexpr auto body = doc.get<"body">();

// metadata collects into head
static_assert(head.child_count() == 5);
static_assert(head.get<"meta">().attribute<"charset">() == "utf-8"sv);
static_assert(head.get<"title">().text() == "Demo & Docs"sv);
static_assert(head.get<"link">().attribute<"href">() == "/main.css"sv);
static_assert(head.get<"style">().text() == "p em{color:#333}"sv);
// raw text: markup, quotes and stray < survive verbatim
static_assert(head.get<"script">().text() == R"(if(a<b){go("</div>")})"sv);
static_assert(head.get<"script">().attribute<"type">() == "module"sv);

// explicit <body> contributed its attributes to the synthesized frame
static_assert(body.attribute<"class">() == "main"sv);
static_assert(body.attribute<"data-x">() == "1"sv);
static_assert(body.child_count() == 10);

// optional end tags: <p>, <li>, <td>, <tr> auto-close
static_assert(body.count<"p">() == 2);
static_assert(body.get<"ul">().count<"li">() == 2);
static_assert(body.get<"ul">().child<0>().text() == "alpha"sv);
static_assert(body.get<"table">().count<"tr">() == 2);
static_assert(body.get<"table">().child<0>().count<"td">() == 2);
static_assert(body.get<"table">().child<1>().child<1>().text() == "d"sv);

// void elements are childless, no close tag needed
static_assert(body.get<"img">().empty());
static_assert(body.get<"img">().attribute<"alt">() == "the logo"sv);

// boolean attributes report the empty string, like the DOM
static_assert(body.get<"input">().has_attribute<"checked">());
static_assert(body.get<"input">().attribute<"checked">() == ""sv);
static_assert(!body.get<"input">().has_attribute<"missing">());

// whitespace: dropped between elements, preserved inside <pre> and
// <textarea> (minus the single newline right after the open tag)
static_assert(body.get<"pre">().text() == "  keep   this"sv);
static_assert(body.get<"textarea">().text() == "draft"sv);

// attribute flavours: unquoted values keep their case
static_assert(body.get<"form">().attribute<"action">() == "/send"sv);
static_assert(body.get<"form">().attribute<"method">() == "POST"sv);
static_assert(body.get<"form">().get<"button">().text() == "Go"sv);

// positional attribute access
static_assert(body.get<"img">().attribute_count() == 2);
static_assert(body.get<"img">().attribute_name<0>() == "src"sv);
static_assert(body.get<"img">().attribute_value<1>() == "the logo"sv);

// --- names are case-insensitive, stored lowercase
constexpr auto cased = cthtml::parse<"<DIV Id=\"x\"><SPAN>y</SpAn></div>">();
static_assert(cased.get<"body">().get<"div">().name() == "div");
static_assert(cased.get<"body">().get<"DIV">().attribute<"ID">() == "x"sv);
static_assert(cased.get<"body">().get<"Div">().get<"span">().text() == "y"sv);

// --- character references: named (the full WHATWG table), numeric,
// two-code-point, windows-1252 remap; unknown ones stay literal
static_assert(cthtml::parse<"<p>&Uuml;ber &copy; &#233; &#x1F600;</p>">()
                  .get<"body">().get<"p">().text() ==
              "\xc3\x9c" "ber \xc2\xa9 \xc3\xa9 \xf0\x9f\x98\x80"sv);
static_assert(cthtml::parse<"<p>&nGt;</p>">().get<"body">().get<"p">().text() ==
              "\xe2\x89\xab\xe2\x83\x92"sv); // U+226B U+20D2
static_assert(cthtml::parse<"<p>&#128;</p>">().get<"body">().get<"p">().text() ==
              "\xe2\x82\xac"sv); // 0x80 remaps to the euro sign
static_assert(cthtml::parse<"<p>&#x0;</p>">().get<"body">().get<"p">().text() ==
              "\xef\xbf\xbd"sv); // NUL becomes U+FFFD
static_assert(cthtml::parse<"<p>&notarealname; &amp x &</p>">()
                  .get<"body">().get<"p">().text() == "&notarealname; &amp x &"sv);
static_assert(cthtml::parse<R"(<a href="/x?a=1&amp;b=2&c=3"></a>)">()
                  .get<"body">().get<"a">().attribute<"href">() == "/x?a=1&b=2&c=3"sv);

// RCDATA elements decode references, raw-text elements do not
static_assert(cthtml::parse<"<title>a &amp; b</title>">()
                  .get<"head">().get<"title">().text() == "a & b"sv);
static_assert(cthtml::parse<"<style>a &amp; b</style>">()
                  .get<"head">().get<"style">().text() == "a &amp; b"sv);

// comments (HTML rules: -- inside is fine) and CDATA sections vanish;
// adjacent text merges across them
static_assert(cthtml::parse<"<p>x<!-- a -- b -->y</p>">()
                  .get<"body">().get<"p">().text() == "xy"sv);
static_assert(cthtml::parse<"<p><![CDATA[gone]]></p>">()
                  .get<"body">().get<"p">().text() == ""sv);

// mixed content: text() concatenates the direct text children
constexpr auto mixed = cthtml::parse<"<p>one<b>bold</b>two</p>">().get<"body">().get<"p">();
static_assert(mixed.text() == "onetwo"sv);
static_assert(mixed.child_count() == 3);
static_assert(mixed.child<1>().name() == "b"sv);
static_assert(mixed.child<0>().type == cthtml::kind::text);

// --- serialization: minified HTML with the void/boolean/raw rules
static_assert(cthtml::serialize(cthtml::parse<"<!DOCTYPE html><p>hi">()) ==
              "<html><head></head><body><p>hi</p></body></html>"sv);
static_assert(cthtml::serialize(cthtml::parse<"<input disabled><br>">().get<"body">()) ==
              "<body><input disabled><br></body>"sv);
static_assert(cthtml::serialize(cthtml::parse<"<script>a<b</script>">().get<"head">()) ==
              "<head><script>a<b</script></head>"sv);
static_assert(cthtml::serialize(cthtml::parse<"<p>a &amp; b</p>">().get<"body">().get<"p">()) ==
              "<p>a &amp; b</p>"sv);
static_assert(cthtml::serialize(cthtml::parse<R"(<a q="say &quot;hi&quot;"></a>)">()
                  .get<"body">().get<"a">()) == R"(<a q="say &quot;hi&quot;"></a>)"sv);
// round-trip: parse(serialize(x)) is the same type as x
constexpr auto rt = cthtml::parse<"<ul id=nav><li>Docs</ul>">();
static_assert(cthtml::serialize(rt) ==
              R"(<html><head></head><body><ul id="nav"><li>Docs</li></ul></body></html>)"sv);
static_assert(std::is_same_v<
    decltype(cthtml::parse<ctll::fixed_string{
        R"(<html><head></head><body><ul id="nav"><li>Docs</li></ul></body></html>)"}>()),
    std::remove_const_t<decltype(rt)>>);

// --- iteration
static_assert([] {
	size_t elements = 0, texts = 0;
	cthtml::for_each_child(body, [&](auto child) {
		if constexpr (decltype(child)::type == cthtml::kind::element) {
			++elements;
		} else {
			++texts;
		}
	});
	return elements * 10 + texts;
}() == 100);
static_assert([] {
	size_t total = 0;
	cthtml::for_each_attribute(body, [&](auto name, auto value) {
		total += name.size() + value.size();
	});
	return total;
}() == (5 + 4) + (6 + 1));

void empty_symbol() { }

#endif

// --- operator[] and iterators (see include/cthtml/views.hpp)

#if CTLL_CNTTP_COMPILER_CHECK

namespace bracket_tests {

constexpr auto page = cthtml::parse<
    R"(<title>demo</title><section id=a><h2>One</h2></section><section id=b></section><p>tail)">();
constexpr auto pbody = page.get<"body">();

// [] is get (first child element with the tag) or child (by position),
// case-insensitively, with misses yielding empty views
static_assert(page["head"]["title"].text() == "demo"sv);
static_assert(pbody["section"].attribute("id") == "a"sv);
static_assert(pbody["SECTION"]["H2"].text() == "One"sv);
static_assert(pbody[1].attribute("Id") == "b"sv);
static_assert(pbody[2].text() == "tail"sv);
static_assert(pbody.count("section") == 2);
static_assert(pbody.contains("p") && !pbody.contains("missing"));
static_assert(pbody["missing"][0]["still-missing"].name().empty());

// name TYPES work as [] arguments in any standard
static_assert([] {
	size_t found = 0;
	cthtml::for_each_child(pbody, [&](auto child) {
		if constexpr (decltype(child)::type == cthtml::kind::element) {
			if (pbody[child.name()].name() == child.name()) {
				++found;
			}
		}
	});
	return found;
}() == 3);

// begin/end yield uniform views from static storage: range-for works,
// in constexpr evaluation included
static_assert([] {
	size_t elements = 0;
	size_t texts = 0;
	for (const auto & n : pbody) {
		if (n.type == cthtml::kind::element) {
			++elements;
		} else {
			++texts;
		}
	}
	return elements * 10 + texts;
}() == 30);

// (gcc 10 wants these loops in named functions, not constexpr lambdas)
constexpr size_t section_attr_chars() {
	size_t chars = 0;
	for (const auto & a : cthtml::attributes(pbody["section"])) {
		chars += a.name.size() + a.value.size();
	}
	return chars;
}
static_assert(section_attr_chars() == 2 + 1);
static_assert(cthtml::attributes(page).empty());

// childless elements iterate zero times
static_assert(cthtml::begin(cthtml::parse<"<br>">().get<"body">().get<"br">()) ==
              cthtml::end(cthtml::parse<"<br>">().get<"body">().get<"br">()));

} // namespace bracket_tests

// --- diagnostics: error_info, error_message, bind_error, debug tools

// valid documents report nothing
static_assert(cthtml::error_info<"<p>hi">().ok());
static_assert(cthtml::error_message<"<p>hi">() == ""sv);
static_assert(cthtml::bind_error<"<p>hi">().ok());

// a tag left open at EOF: kind, offset and the expected terminals
constexpr auto unclosed = cthtml::error_info<"<p class=x">();
static_assert(unclosed.kind != ctlark::error_kind::none);
static_assert(unclosed.position == 10);
static_assert(!cthtml::error_message<"<p class=x">().empty());

// tree-construction failures name the rule and the offending token
constexpr auto crossing = cthtml::bind_error<"<b><i>x</b></i>">();
static_assert(crossing.reason == cthtml::bind_reason::mismatched_tag);
static_assert(crossing.where == "</b>"sv);
constexpr auto stray = cthtml::bind_error<"<p>x</p></p>">();
static_assert(stray.reason == cthtml::bind_reason::stray_end_tag);
static_assert(stray.where == "</p>"sv);
constexpr auto dup_attr = cthtml::bind_error<R"(<a x="1" x="2"></a>)">();
static_assert(dup_attr.reason == cthtml::bind_reason::duplicate_attribute);
static_assert(dup_attr.where == "x"sv);
constexpr auto selfclosed = cthtml::bind_error<"<div/>">();
static_assert(selfclosed.reason == cthtml::bind_reason::self_closing_non_void);
static_assert(selfclosed.where == "<div"sv);
static_assert(cthtml::bind_error<"</br>">().reason == cthtml::bind_reason::stray_end_tag);

// the ctlark debugging toolbox with the HTML grammar baked in
static_assert(cthtml::debug::dump_tokens<"<a x=1>hi</a>">() ==
              "OPEN '<a' @0..2\n"
              "NAME 'x' @3..4\n"
              "EQUAL '=' @4..5\n"
              "UNQVAL '1' @5..6\n"
              "MORETHAN '>' @6..7\n"
              "TEXT 'hi' @7..9\n"
              "CLOSE '</a>' @9..13\n"sv);
static_assert(cthtml::debug::dump_tokens<"<script>a<b</script>">() ==
              "SCRIPT_OPEN '<script' @0..7\n"
              "MORETHAN '>' @7..8\n"
              "SCRIPT_BODY 'a<b</script>' @8..20\n"sv);
constexpr auto traced = cthtml::debug::traced_parse<"<b><i>x</b></i>">();
static_assert(traced.ok); // the SYNTAX parses; tree construction rejects it
static_assert(traced.log.events > 0);
static_assert(cthtml::debug::dump_grammar().find("terminal OPEN") != std::string_view::npos);

#endif
