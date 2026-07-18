#include <ctcss.hpp>

void empty_symbol_17() { }

#if CTLL_CNTTP_COMPILER_CHECK

#include <string_view>
using namespace std::literals;

// --- what parses
static_assert(ctcss::is_valid<"">);
static_assert(ctcss::is_valid<"p { }">);
static_assert(ctcss::is_valid<"p { color: red }">); // final ; optional
static_assert(ctcss::is_valid<"* { box-sizing: border-box; }">);
static_assert(ctcss::is_valid<".a.b.c { x: y; }">);
static_assert(ctcss::is_valid<"#only { x: y; }">);
static_assert(ctcss::is_valid<"div.note#top.wide { x: y; }">);
static_assert(ctcss::is_valid<"a b c d { x: y; }">);
static_assert(ctcss::is_valid<"a > b > c { x: y; }">);
static_assert(ctcss::is_valid<"h1, h2, .t, #i { x: y; }">);
static_assert(ctcss::is_valid<"p { font: 12px/1.4 'Fira Sans', sans-serif; }">);
static_assert(ctcss::is_valid<"p { background: url(img.png) no-repeat; }">);
static_assert(ctcss::is_valid<"/* only a comment */">);
static_assert(ctcss::is_valid<"p { -moz-x: 1; -webkit-y: 2; }">);
static_assert(ctcss::is_valid<"p { c: red ! important ; }">); // spaced bang

// --- what does not
static_assert(!ctcss::is_valid<"p { color red }">);   // missing colon
static_assert(!ctcss::is_valid<"p { color: red;">);   // missing brace
static_assert(!ctcss::is_valid<"color: red;">);       // bare declaration
static_assert(!ctcss::is_valid<"p color: red; }">);   // missing open brace
static_assert(!ctcss::is_valid<"@media (x) { }">);    // at-rules not in v0.1
static_assert(!ctcss::is_valid<"p:hover { c: v; }">); // pseudo-classes not in v0.1

// --- compound splitting details
constexpr auto mixed = ctcss::parse<".lead#hero.big { x: y; }">();
constexpr auto comp = mixed.rule_at<0>().selector_at<0>().step<0>();
static_assert(decltype(comp)::compound_type::tag() == ""sv);
static_assert(decltype(comp)::compound_type::id() == "hero"sv);
static_assert(decltype(comp)::compound_type::class_count() == 2);
static_assert(decltype(comp)::compound_type::class_at<0>() == "lead"sv);
static_assert(decltype(comp)::compound_type::class_at<1>() == "big"sv);

// tags fold to lowercase at bind time
constexpr auto upper = ctcss::parse<"DIV { x: y; }">();
static_assert(decltype(upper.rule_at<0>().selector_at<0>().step<0>())::compound_type::tag() ==
              "div"sv);

// --- specificity table
static_assert(ctcss::parse<"* { x: y; }">().rule_at<0>().selector_at<0>().specificity() == 0);
static_assert(ctcss::parse<"p { x: y; }">().rule_at<0>().selector_at<0>().specificity() == 1);
static_assert(ctcss::parse<".c { x: y; }">().rule_at<0>().selector_at<0>().specificity() == 100);
static_assert(ctcss::parse<"#i { x: y; }">().rule_at<0>().selector_at<0>().specificity() == 10000);
static_assert(ctcss::parse<"p.c#i a.b { x: y; }">().rule_at<0>().selector_at<0>().specificity() ==
              10202); // 1 id, 2 classes, 2 types

// --- matching edges
constexpr auto star = ctcss::parse<"* { x: y; }">();
constexpr ctcss::element_ref anything[] = {{"whatever", "id", "cls"}};
static_assert(ctcss::query(star, anything, "x") == "y"sv);

// class words split on whitespace, exact matches only
constexpr auto cls = ctcss::parse<".note { x: y; }">();
constexpr ctcss::element_ref has[] = {{"p", "", "big note wide"}};
constexpr ctcss::element_ref hasnt[] = {{"p", "", "notes bignote"}};
static_assert(ctcss::query(cls, has, "x") == "y"sv);
static_assert(ctcss::query(cls, hasnt, "x") == ""sv);

// descendant may match ANY ancestor pairing (backtracking)
constexpr auto deep = ctcss::parse<".a .b .c { x: y; }">();
constexpr ctcss::element_ref zigzag[] = {
    {"d1", "", "a"}, {"d2", "", "b"}, {"d3", "", "a b"}, {"d4", "", "c"}};
static_assert(ctcss::query(deep, zigzag, "x") == "y"sv);

// the same element cannot satisfy two chain steps
constexpr auto two = ctcss::parse<".a .a { x: y; }">();
constexpr ctcss::element_ref single_a[] = {{"p", "", "a"}};
constexpr ctcss::element_ref nested_a[] = {{"p", "", "a"}, {"q", "", "a"}};
static_assert(ctcss::query(two, single_a, "x") == ""sv);
static_assert(ctcss::query(two, nested_a, "x") == "y"sv);

// --- entries view (the flattened cascade input)
constexpr auto flat = ctcss::parse<"a, b { x: 1; y: 2; } c { z: 3; }">();
static_assert(ctcss::entry_count(flat) == 5); // 2 selectors x 2 decls + 1
static_assert(ctcss::entries(flat)[0].property == "x"sv);
static_assert(ctcss::entries(flat)[4].value == "3"sv);

// --- value helpers
static_assert(ctcss::parse_length("12px").ok);
static_assert(ctcss::parse_length("12px").value == 12);
static_assert(ctcss::parse_length("1.5em").value == 1.5);
static_assert(ctcss::parse_length("-2rem").value == -2);
static_assert(ctcss::parse_length("50%").u == ctcss::unit::pct);
static_assert(ctcss::parse_length("100vh").u == ctcss::unit::vh);
static_assert(ctcss::parse_length("0").u == ctcss::unit::none);
static_assert(!ctcss::parse_length("auto").ok);
static_assert(!ctcss::parse_length("px").ok);

static_assert(ctcss::parse_color("#ff8800").r == 255);
static_assert(ctcss::parse_color("#ff8800").g == 136);
static_assert(ctcss::parse_color("#f80").g == 136); // #rgb expands
static_assert(ctcss::parse_color("#ff880080").a == 128);
static_assert(ctcss::parse_color("rgb(0, 128, 255)").b == 255);
static_assert(ctcss::parse_color("rgba(1, 2, 3, 0.5)").a == 127);
static_assert(ctcss::parse_color("rgb(300, -5, 0)").r == 255); // clamped
static_assert(ctcss::parse_color("ORANGE").r == 255);          // names fold case
static_assert(ctcss::parse_color("transparent").a == 0);
static_assert(!ctcss::parse_color("notacolor").ok);
static_assert(!ctcss::parse_color("#12345").ok);

// values keep their inner spacing; !important variants peel
constexpr auto spaced = ctcss::parse<"p { margin: 0   4px ; c: v !IMPORTANT; }">();
static_assert(spaced.rule_at<0>().decl<0>().value() == "0   4px"sv);
static_assert(spaced.rule_at<0>().decl<1>().important());
static_assert(spaced.rule_at<0>().decl<1>().value() == "v"sv);

void empty_symbol() { }

#endif
