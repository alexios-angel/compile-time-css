#include <ctcss.hpp>
#include <string_view>

// C++17: inputs and keys are fixed_string variables with linkage
static constexpr auto sheet_text =
    ctll::fixed_string{"p { color: black; } div.note p { color: red; }"};
static constexpr auto bad_text = ctll::fixed_string{"p { color red }"};
static constexpr ctll::fixed_string color_key = "color";

static_assert(ctcss::is_valid<sheet_text>);
static_assert(!ctcss::is_valid<bad_text>);

constexpr auto sheet = ctcss::parse<sheet_text>();
static_assert(sheet.rule_count() == 2);
static_assert(sheet.rule_at<0>().template property<color_key>() ==
              std::string_view{"black"});

// matching and the cascade are plain value calls - no C++20 needed
constexpr ctcss::element_ref chain[] = {{"div", "", "note"}, {"p"}};
static_assert(ctcss::matches(sheet.rule_at<1>().selector_at<0>(), chain));
static_assert(ctcss::query(sheet, chain, "color") == std::string_view{"red"});

// value helpers likewise
static_assert(ctcss::parse_length("2em").value == 2);
static_assert(ctcss::parse_color("#fff").r == 255);

// diagnostics through the variable-form API
static_assert(ctcss::error_info<sheet_text>().ok());
constexpr auto broken_info = ctcss::error_info<bad_text>();
static_assert(broken_info.kind != ctlark::error_kind::none);
static_assert(!ctcss::error_message<bad_text>().empty());
constexpr auto traced = ctcss::debug::traced_parse<sheet_text>();
static_assert(traced.ok && traced.log.events > 0);
static_assert(!ctcss::debug::dump_tokens<sheet_text>().empty());

void empty_symbol() { }
