// The BY-VALUE stylesheet path (ctcss/value.hpp): parse a runtime string into
// an owning value_sheet with a LINEAR parser, then resolve the cascade with
// the same rules as the compile-time TYPE path. These checks mirror the type
// path's semantics AND exercise the leniency the type path lacks (skips
// @font-face/@keyframes, recurses @media) so real-world CSS parses.
#include <ctcss/value.hpp> // value path only - no Earley grammar pulled in
#include <cstdio>
#include <string_view>

using namespace std::literals;
using ctcss::element_ref;

static int failures = 0;

static void eq(const char * label, std::string_view got, std::string_view want) {
	if (got != want) {
		std::printf("FAIL %s: got[%.*s] want[%.*s]\n", label, (int)got.size(), got.data(),
		            (int)want.size(), want.data());
		++failures;
	}
}

// query helper over a freshly parsed sheet
static std::string_view q(std::string_view css, const element_ref * chain, size_t n,
                          std::string_view prop) {
	static ctcss::value_sheet keep; // keep storage alive for the returned view
	keep = ctcss::parse_value(css);
	return ctcss::query(keep, chain, n, prop);
}

int main() {
	// --- basics
	{
		element_ref chain[] = {{"p"}};
		eq("tag", q("p { color: red }", chain, 1, "color"), "red");
		eq("final-semi-optional", q("p { color: green; }", chain, 1, "color"), "green");
		eq("absent-prop", q("p { color: red }", chain, 1, "margin"), "");
	}
	// --- specificity: id (10000) > class (100) > type (1)
	{
		element_ref chain[] = {{"div", "x", "a"}};
		eq("id-beats-class",
		   q(".a { color: blue } #x { color: green } div { color: red }", chain, 1, "color"),
		   "green");
		eq("class-beats-type", q("div { color: red } .a { color: blue }", chain, 1, "color"),
		   "blue");
	}
	// --- source order breaks ties
	{
		element_ref chain[] = {{"p"}};
		eq("later-wins", q("p { color: red } p { color: blue }", chain, 1, "color"), "blue");
	}
	// --- !important beats specificity
	{
		element_ref chain[] = {{"div", "x", ""}};
		eq("important-wins", q("#x { color: green } div { color: red !important }", chain, 1, "color"),
		   "red");
		eq("important-spaced-bang",
		   q("#x { color: green } div { color: red ! important }", chain, 1, "color"), "red");
	}
	// --- descendant combinator (whitespace)
	{
		element_ref d_p[] = {{"div"}, {"p"}};
		element_ref p_only[] = {{"p"}};
		eq("descendant-matches", q("div p { color: red }", d_p, 2, "color"), "red");
		eq("descendant-needs-ancestor", q("div p { color: red }", p_only, 1, "color"), "");
	}
	// --- child combinator (>)
	{
		element_ref div_p[] = {{"div"}, {"p"}};
		element_ref div_span_p[] = {{"div"}, {"span"}, {"p"}};
		eq("child-direct", q("div > p { color: red }", div_p, 2, "color"), "red");
		eq("child-not-grandchild", q("div > p { color: red }", div_span_p, 3, "color"), "");
	}
	// --- multiple classes on one compound
	{
		element_ref both[] = {{"i", "", "a b"}};
		element_ref one[] = {{"i", "", "a"}};
		eq("two-classes-match", q(".a.b { color: red }", both, 1, "color"), "red");
		eq("two-classes-need-both", q(".a.b { color: red }", one, 1, "color"), "");
	}
	// --- selector list
	{
		element_ref h2[] = {{"h2"}};
		eq("selector-list", q("h1, h2, h3 { color: red }", h2, 1, "color"), "red");
	}
	// --- leniency: @media recurses (condition ignored), @font-face/@keyframes skipped
	{
		element_ref chain[] = {{"p"}};
		eq("at-media-recurses",
		   q("@media screen and (min-width: 100px) { p { color: red } }", chain, 1, "color"), "red");
		eq("font-face-skipped-then-rule",
		   q("@font-face { font-family: 'X'; src: url(x.woff) } p { color: blue }", chain, 1, "color"),
		   "blue");
		eq("keyframes-skipped-then-rule",
		   q("@keyframes spin { 0% { opacity: 0 } 100% { opacity: 1 } } p { color: green }", chain, 1,
		     "color"),
		   "green");
		eq("import-skipped", q("@import url(x.css); p { color: red }", chain, 1, "color"), "red");
	}
	// --- comments ignored
	{
		element_ref chain[] = {{"p"}};
		eq("comment", q("/* c */ p { /* k */ color: red /* v */ }", chain, 1, "color"), "red");
	}
	// --- typed value helpers still work off the resolved string
	{
		element_ref chain[] = {{"p"}};
		const ctcss::length l = ctcss::parse_length(q("p { margin: 8px }", chain, 1, "margin"));
		if (!(l.ok && l.value == 8)) { std::printf("FAIL length\n"); ++failures; }
	}
	// --- parsed+queried at run time in the engine, BUT in C++23 std::string
	// and std::vector are constexpr, so the whole thing also folds in a
	// constant expression (transiently) - like the rest of the ct* stack.
	// (A value_sheet owns heap, so it can't be a *persisted* constexpr
	// variable - non-transient allocation - only used within one evaluation.)
	{
		const auto s = ctcss::parse_value("a.x { color: red } #y { color: blue }");
		const element_ref c[] = {{"a", "y", "x"}};
		eq("id-wins-runtime", ctcss::query(s, c, 1, "color"), "blue");
		if (s.rule_count() != 2) { std::printf("FAIL rule_count %zu\n", s.rule_count()); ++failures; }
	}
	static_assert([] {
		auto s = ctcss::parse_value("a.x { color: red } #y { color: blue } @media x { p { z: 1 } }");
		const element_ref c[] = {{"a", "y", "x"}};
		return ctcss::query(s, c, 1, "color") == "blue"sv && s.rule_count() == 3;
	}(), "value CSS parses+queries at compile time (transient constexpr)");

	if (failures == 0) { std::printf("ctcss value suite: all checks passed\n"); }
	return failures ? 1 : 0;
}
