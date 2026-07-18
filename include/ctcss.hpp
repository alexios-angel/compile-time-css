#ifndef CTCSS__HPP
#define CTCSS__HPP

#include "ctlark.hpp"
#include "ctcss/grammar.hpp"
#include "ctcss/types.hpp"
#include "ctcss/bind.hpp"
#include "ctcss/values.hpp"
#include "ctcss/match.hpp"
#include "ctcss/serialize.hpp"

// ctcss: compile-time CSS.
//
//   constexpr auto sheet = ctcss::parse<R"(
//       p        { color: black; margin: 8px; }
//       div.note p { color: red !important; }
//       #main    { width: 640px; }
//   )">();
//
//   constexpr ctcss::element_ref chain[] = {
//       {"body"}, {"div", "", "note wide"}, {"p"}};
//   static_assert(ctcss::query(sheet, chain, "color") == "red");
//   static_assert(ctcss::query(sheet, chain, "margin") == "8px");
//   static_assert(ctcss::parse_length("8px").value == 8);
//   static_assert(!ctcss::is_valid<"p { color red }">);
//
// The stylesheet is parsed while your code compiles - malformed CSS is
// a compile error, or `false` from is_valid - and the sheet is a TYPE
// whose every accessor is constexpr. Selector matching and the cascade
// (!important, specificity, source order) are VALUE computations over
// flattened static views, so they run in static_asserts against a
// compile-time DOM chain and equally well at runtime, when a script
// has just flipped a class and styles need resolving again.
//
// The grammar layer is ctlark (compile-time Lark): compound selectors
// are single tokens (whitespace adjacency IS the descendant
// combinator - see grammar.hpp), the binder (bind.hpp) splits them
// into typed tag/#id/.classes parts and cooks declaration values, and
// match.hpp supplies element_ref chains, matches() and query().

namespace ctcss {

#if CTLL_CNTTP_COMPILER_CHECK
#define CTCSS_STRING_INPUT ctll::fixed_string
#else
// C++17: pass a constexpr ctll::fixed_string variable with linkage
#define CTCSS_STRING_INPUT const auto &
#endif

// does the input parse as CSS (within the supported subset)?
CTLL_EXPORT template <CTCSS_STRING_INPUT input> constexpr bool is_valid =
	ctlark::is_valid<detail::css_grammar, input, detail::css_start>;

// what failed and where, when it does not: kind, byte offset, line,
// column and the expected terminals (kind none = the syntax is fine)
CTLL_EXPORT template <CTCSS_STRING_INPUT input> constexpr ctlark::error_info_t error_info() noexcept {
	return ctlark::error_info<detail::css_grammar, input, detail::css_start>();
}

// the rendered diagnostic - location, snippet with a caret, expected
// terminals - as a static string ("" when the syntax is fine)
CTLL_EXPORT template <CTCSS_STRING_INPUT input> constexpr std::string_view error_message() noexcept {
	return ctlark::error_message<detail::css_grammar, input, detail::css_start>();
}

// parse the input into its stylesheet type; invalid CSS fails the build
CTLL_EXPORT template <CTCSS_STRING_INPUT input> constexpr auto parse() noexcept {
#ifdef CTLARK_VERBOSE_ERRORS
	(void)ctlark::verbose_report<detail::css_grammar, input, detail::css_start>();
#endif
	static_assert(ctlark::is_valid<detail::css_grammar, input, detail::css_start>,
	              "ctcss: the input is not valid CSS (within the supported subset) - print "
	              "ctcss::error_message<input>() for the location and the expected tokens");
	if constexpr (is_valid<input>) {
		using bound = detail::bind_sheet<
		    decltype(ctlark::parse<detail::css_grammar, input, detail::css_start>())>;
		return typename bound::type{};
	} else {
		return stylesheet<>{};
	}
}

// the ctlark debugging toolbox with the CSS grammar baked in
namespace debug {

CTLL_EXPORT template <CTCSS_STRING_INPUT input, size_t Cap = 4096> constexpr auto traced_parse() noexcept {
	return ctlark::debug::traced_parse<detail::css_grammar, input, detail::css_start, Cap>();
}

CTLL_EXPORT template <CTCSS_STRING_INPUT input> constexpr std::string_view dump_tokens() noexcept {
	return ctlark::debug::dump_tokens<detail::css_grammar, input, detail::css_start>();
}

CTLL_EXPORT constexpr std::string_view dump_grammar() noexcept {
	return ctlark::debug::dump_grammar<detail::css_grammar>();
}

CTLL_EXPORT template <size_t MaxTokens = 1024>
ctlark::debug::runtime_result parse_runtime(std::string_view in) {
	return ctlark::debug::parse_runtime<detail::css_grammar, MaxTokens>(in, "start");
}

} // namespace debug

} // namespace ctcss

#endif
