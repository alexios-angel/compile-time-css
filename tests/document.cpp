#include <ctcss.hpp>

void empty_symbol_17() { }

// the string-literal API needs C++20 class-type template parameters;
// tests/cxx17.cpp covers the C++17 variable form
#if CTLL_CNTTP_COMPILER_CHECK

#include <string_view>
using namespace std::literals;

// --- a real stylesheet
constexpr auto sheet = ctcss::parse<R"(
	/* the demo theme */
	* { box-sizing: border-box; }
	p { color: black; margin: 8px; }
	div.note p { color: red !important; }
	#main.wide > ul { width: 640px; padding: 0 4px }
	h1, .title { font-weight: bold; font-family: 'Fira Sans', sans-serif; }
	ul li { -webkit-thing: none; }
)">();

static_assert(sheet.rule_count() == 6);

// rules, declarations, values
static_assert(sheet.rule_at<1>().decl_count() == 2);
static_assert(sheet.rule_at<1>().decl<0>().property() == "color"sv);
static_assert(sheet.rule_at<1>().decl<0>().value() == "black"sv);
static_assert(!sheet.rule_at<1>().decl<0>().important());
static_assert(sheet.rule_at<2>().decl<0>().important());
static_assert(sheet.rule_at<2>().decl<0>().value() == "red"sv); // "!important" peeled
static_assert(sheet.rule_at<1>().property<"margin">() == "8px"sv);
static_assert(sheet.rule_at<1>().property<"MARGIN">() == "8px"sv); // names fold case
static_assert(sheet.rule_at<1>().property<"missing">() == ""sv);
static_assert(sheet.rule_at<4>().selector_count() == 2);
static_assert(sheet.rule_at<4>().property<"font-family">() == "'Fira Sans', sans-serif"sv);
static_assert(sheet.rule_at<5>().decl<0>().property() == "-webkit-thing"sv);

// selector structure and specificity
constexpr auto s_note = sheet.rule_at<2>().selector_at<0>(); // div.note p
static_assert(s_note.step_count() == 2);
static_assert(s_note.specificity() == 102); // one class, two types
constexpr auto s_main = sheet.rule_at<3>().selector_at<0>(); // #main.wide > ul
static_assert(s_main.specificity() == 10101);
static_assert(s_main.step<1>().relation() == ctcss::rel::child);
static_assert(decltype(s_main.step<0>())::compound_type::id() == "main"sv);
static_assert(decltype(s_main.step<0>())::compound_type::class_count() == 1);
static_assert(sheet.rule_at<0>().selector_at<0>().specificity() == 0); // *

// --- matching against element chains (root-first, self-last)
constexpr ctcss::element_ref chain_note_p[] = {{"body"}, {"div", "", "note wide"}, {"p"}};
constexpr ctcss::element_ref chain_plain_p[] = {{"body"}, {"p"}};
static_assert(ctcss::matches(s_note, chain_note_p));
static_assert(!ctcss::matches(s_note, chain_plain_p));

constexpr ctcss::element_ref chain_ul[] = {{"body"}, {"div", "main", "wide"}, {"ul"}};
constexpr ctcss::element_ref chain_ul_deep[] = {
    {"body"}, {"div", "main", "wide"}, {"section"}, {"ul"}};
static_assert(ctcss::matches(s_main, chain_ul));
static_assert(!ctcss::matches(s_main, chain_ul_deep)); // > demands a direct child

// descendant skips generations; tags match case-insensitively
constexpr ctcss::element_ref chain_li[] = {
    {"BODY"}, {"UL", "", ""}, {"DIV"}, {"LI"}};
static_assert(ctcss::matches(sheet.rule_at<5>().selector_at<0>(), chain_li));

// --- the cascade: !important beats specificity beats source order
static_assert(ctcss::query(sheet, chain_note_p, "color") == "red"sv);
static_assert(ctcss::query(sheet, chain_plain_p, "color") == "black"sv);
static_assert(ctcss::query(sheet, chain_note_p, "margin") == "8px"sv);
static_assert(ctcss::query(sheet, chain_note_p, "box-sizing") == "border-box"sv);
static_assert(ctcss::query(sheet, chain_note_p, "width") == ""sv);
constexpr ctcss::element_ref chain_title[] = {{"body"}, {"span", "", "title"}};
static_assert(ctcss::query(sheet, chain_title, "font-weight") == "bold"sv);
static_assert(ctcss::query(sheet, chain_title, "FONT-WEIGHT") == "bold"sv);

// source order breaks ties; higher specificity beats later order
constexpr auto order_sheet =
    ctcss::parse<"p { color: red; } p { color: blue; } p.x { color: green; }">();
constexpr ctcss::element_ref just_p[] = {{"p"}};
constexpr ctcss::element_ref p_x[] = {{"p", "", "x"}};
static_assert(ctcss::query(order_sheet, just_p, "color") == "blue"sv);
static_assert(ctcss::query(order_sheet, p_x, "color") == "green"sv);

// !important resists later, more specific rules
constexpr auto imp_sheet =
    ctcss::parse<"p { color: red !important; } p.x { color: blue; }">();
static_assert(ctcss::query(imp_sheet, p_x, "color") == "red"sv);

// --- iteration
static_assert([] {
	size_t decls = 0;
	ctcss::for_each_rule(sheet, [&](auto r) { decls += r.decl_count(); });
	return decls;
}() == 9);

// --- serialization: minified round-trip
static_assert(ctcss::serialize(ctcss::parse<"p ,  .a  { color : red ; }">()) ==
              "p,.a{color:red}"sv);
static_assert(ctcss::serialize(ctcss::parse<"div.note p { color: red !important; }">()) ==
              "div.note p{color:red!important}"sv);
static_assert(ctcss::serialize(ctcss::parse<"#m.w > ul { x: 1 }">()) == "#m.w>ul{x:1}"sv);
constexpr auto rt = ctcss::parse<"a.x{b:c}">();
static_assert(std::is_same_v<decltype(ctcss::parse<ctll::fixed_string{"a.x{b:c}"}>()),
                             std::remove_const_t<decltype(rt)>>);

// --- diagnostics
static_assert(ctcss::error_info<"p { a: b; }">().ok());
static_assert(ctcss::error_message<"p { a: b; }">() == ""sv);
constexpr auto broken = ctcss::error_info<"p { color: red;">();
static_assert(broken.kind != ctlark::error_kind::none);
static_assert(!ctcss::error_message<"p { color: red;">().empty());
static_assert(!ctcss::debug::dump_tokens<"p { a: b; }">().empty());
static_assert(ctcss::debug::dump_grammar().find("terminal COMPOUND") != std::string_view::npos);

void empty_symbol() { }

#endif
