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

	// --- @font-face is CAPTURED (not skipped), and rules after it still parse
	{
		const auto s = ctcss::parse_value(
		    "@font-face { font-family: 'Press Start 2P'; src: url(/f/ps.ttf) format('truetype') }"
		    " p { color: red }");
		if (s.font_faces.size() != 1) { std::printf("FAIL font_faces %zu\n", s.font_faces.size()); ++failures; }
		else {
			eq("font-face family", s.font_faces[0].get("font-family"), "'Press Start 2P'");
			eq("font-face src", s.font_faces[0].get("src"), "url(/f/ps.ttf) format('truetype')");
		}
		element_ref chain[] = {{"p"}};
		eq("rule-after-font-face", ctcss::query(s, chain, 1, "color"), "red");
	}
	// --- @keyframes is CAPTURED with its stops (from/to/% + comma lists)
	{
		const auto s = ctcss::parse_value(
		    "@keyframes spin { from { opacity: 0; transform: rotate(0deg) }"
		    " 50%, 75% { opacity: 0.5 } to { opacity: 1; transform: rotate(360deg) } }");
		const auto * k = s.animation("spin");
		if (k == nullptr) { std::printf("FAIL keyframes not found\n"); ++failures; }
		else if (k->frames.size() != 4) { std::printf("FAIL keyframes stops %zu\n", k->frames.size()); ++failures; }
		else {
			// from -> 0.0, to -> 1.0
			if (k->frames.front().at != 0.0 || k->frames.back().at != 1.0) {
				std::printf("FAIL keyframe positions\n"); ++failures;
			}
			eq("keyframe decl", k->frames.back().decls.empty() ? "" : k->frames.back().decls[0].property, "opacity");
		}
		// -webkit- prefix is stripped
		const auto s2 = ctcss::parse_value("@-webkit-keyframes fade { to { opacity: 1 } }");
		if (s2.animation("fade") == nullptr) { std::printf("FAIL vendor keyframes\n"); ++failures; }
	}

	// --- pseudo-classes: :hover/:active/:focus/:checked/:disabled match the
	// element_ref `states` bitmask; anything unknown makes the rule
	// IMPOSSIBLE (never matches - real browser behavior for e.g. :visited)
	{
		element_ref plain[] = {{"a"}};
		element_ref hov[] = {{"a", "", "", ctcss::ps_hover}};
		eq("hover-on", q("a:hover { color: red }", hov, 1, "color"), "red");
		eq("hover-off", q("a:hover { color: red }", plain, 1, "color"), "");
		eq("hover-case", q("a:HOVER { color: red }", hov, 1, "color"), "red");

		element_ref act[] = {{"button", "", "", ctcss::ps_active}};
		element_ref foc[] = {{"input", "", "", ctcss::ps_focus}};
		element_ref chk[] = {{"input", "", "", ctcss::ps_checked}};
		element_ref dis[] = {{"button", "", "", ctcss::ps_disabled}};
		eq("active-on", q("button:active { color: red }", act, 1, "color"), "red");
		eq("focus-on", q("input:focus { color: red }", foc, 1, "color"), "red");
		eq("checked-on", q("input:checked { color: red }", chk, 1, "color"), "red");
		eq("disabled-on", q("button:disabled { color: gray }", dis, 1, "color"), "gray");
		element_ref btn[] = {{"button"}};
		eq("active-off", q("button:active { color: red }", btn, 1, "color"), "");

		// class AND state required, in either spelling order
		element_ref both[] = {{"a", "", "btn", ctcss::ps_hover}};
		element_ref clsonly[] = {{"a", "", "btn"}};
		eq("class+state", q("a.btn:hover { color: red }", both, 1, "color"), "red");
		eq("state-order", q("a:hover.btn { color: red }", both, 1, "color"), "red");
		eq("class-no-state", q("a.btn:hover { color: red }", clsonly, 1, "color"), "");

		// bare :hover matches any hovered element
		element_ref anyhov[] = {{"div", "", "", ctcss::ps_hover}};
		eq("bare-hover", q(":hover { color: red }", anyhov, 1, "color"), "red");

		// ancestor hover through the chain (div:hover p)
		element_ref deep_on[] = {{"div", "", "", ctcss::ps_hover}, {"p"}};
		element_ref deep_off[] = {{"div"}, {"p"}};
		eq("ancestor-hover", q("div:hover p { color: red }", deep_on, 2, "color"), "red");
		eq("ancestor-hover-off", q("div:hover p { color: red }", deep_off, 2, "color"), "");

		// multiple required states
		element_ref cd[] = {{"input", "", "", ctcss::ps_checked | ctcss::ps_disabled}};
		element_ref conly[] = {{"input", "", "", ctcss::ps_checked}};
		eq("multi-state", q("input:checked:disabled { color: gray }", cd, 1, "color"), "gray");
		eq("multi-state-partial", q("input:checked:disabled { color: gray }", conly, 1, "color"), "");
	}
	// --- pseudo specificity: a pseudo-class weighs like a class (100)
	{
		element_ref hov[] = {{"a", "", "x", ctcss::ps_hover}};
		eq("pseudo-beats-type", q("a:hover { color: blue } a { color: red }", hov, 1, "color"), "blue");
		// tie with a class -> source order
		eq("pseudo-class-tie", q("a:hover { color: blue } a.x { color: green }", hov, 1, "color"), "green");
		eq("pseudo-class-tie2", q("a.x { color: green } a:hover { color: blue }", hov, 1, "color"), "blue");
		// two classes beat one pseudo
		element_ref xy[] = {{"a", "", "x y", ctcss::ps_hover}};
		eq("classes-beat-pseudo", q("a:hover { color: blue } a.x.y { color: green }", xy, 1, "color"), "green");
	}
	// --- unknown pseudos: the rule exists but NEVER matches (no fallback
	// to the base selector), and other alternatives in a list still work
	{
		element_ref plain[] = {{"a"}};
		element_ref hov[] = {{"a", "", "", ctcss::ps_hover}};
		eq("visited-never", q("a:visited { color: purple }", plain, 1, "color"), "");
		eq("visited-never-hov", q("a:visited { color: purple }", hov, 1, "color"), "");
		eq("visited-no-shadow", q("a:visited { color: purple } a { color: blue }", plain, 1, "color"), "blue");
		eq("pseudo-element-never", q("a::before { color: red }", plain, 1, "color"), "");
		eq("functional-never", q("li:nth-child(2) { color: red }", plain, 1, "color"), "");
		element_ref b[] = {{"b"}};
		eq("list-isolation", q("a:visited, b { color: red }", b, 1, "color"), "red");
	}
	// the critical pseudo facts also fold in a constant expression
	static_assert([] {
		auto s = ctcss::parse_value("a:hover { color: red } a:visited { color: purple }");
		const element_ref hov[] = {{"a", "", "", ctcss::ps_hover}};
		const element_ref plain[] = {{"a"}};
		return ctcss::query(s, hov, 1, "color") == "red"sv &&
		       ctcss::query(s, plain, 1, "color") == ""sv;
	}(), "pseudo-classes match states and unknown pseudos never match");

	if (failures == 0) { std::printf("ctcss value suite: all checks passed\n"); }
	return failures ? 1 : 0;
}
