#ifndef CTCSS__MATCH__HPP
#define CTCSS__MATCH__HPP

#include <cstdint>

#include "types.hpp"
#ifndef CTCSS_IN_A_MODULE
#include <array>
#include <cstddef>
#include <string_view>
#endif

// Selector matching and the cascade. Selectors are TYPES, but matching
// is a VALUE computation: each selector/stylesheet materializes a flat
// static view (step tables, one decl_entry per selector x declaration),
// and the match/query functions are plain constexpr loops over those
// views and a chain of element_refs. That makes them equally usable in
// a static_assert (style resolution at compile time, for a compile-time
// DOM) and at runtime (a browser restyling after a script mutation).
//
// An element_ref is deliberately tiny - tag, id, and the space-
// separated class attribute, exactly what an HTML element knows:
//
//   ctcss::element_ref{"div", "top", "note wide"}
//
// A match chain is root-first, self-last:
//
//   constexpr ctcss::element_ref chain[] = {{"body"}, {"div", "", "note"}, {"p"}};
//   static_assert(ctcss::matches(sheet.rule_at<0>().selector_at<0>(), chain));
//   static_assert(ctcss::query(sheet, chain, "color") == "red");
//
// query resolves the cascade the way CSS does: !important wins, then
// specificity (ids, classes, types), then source order.

namespace ctcss {

// what a selector is matched against: one element's identity
struct element_ref {
	std::string_view tag{};
	std::string_view id{};
	std::string_view classes{}; // space-separated, like the class attribute
};

namespace detail {

// is `name` one of the space-separated words in `classes`?
constexpr bool has_class(std::string_view classes, std::string_view name) noexcept {
	std::size_t i = 0;
	while (i < classes.size()) {
		while (i < classes.size() && is_css_blank(classes[i])) { ++i; }
		std::size_t j = i;
		while (j < classes.size() && !is_css_blank(classes[j])) { ++j; }
		if (j > i && classes.substr(i, j - i) == name) { return true; }
		i = j;
	}
	return false;
}

} // namespace detail

} // namespace ctcss

#endif
