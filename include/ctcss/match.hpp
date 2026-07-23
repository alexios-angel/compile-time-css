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
// An element_ref is deliberately tiny - tag, id, the space-separated
// class attribute, and a UI-state bitmask (hover/active/focus/checked/
// disabled - what pseudo-class selectors match against). The browser
// engine fills `states` from live interaction; static chains leave it 0:
//
//   ctcss::element_ref{"div", "top", "note wide"}
//   ctcss::element_ref{"a", "", "", ctcss::ps_hover}   // a hovered link
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

// the interaction states a pseudo-class selector can require; unscoped
// so the bits compose without casts (element_ref::states is `unsigned`)
enum pseudo_state : unsigned {
	ps_hover = 1u,    // :hover  - pointer over the element (or a descendant)
	ps_active = 2u,   // :active - pressed (mouse held)
	ps_focus = 4u,    // :focus  - the focused control
	ps_checked = 8u,  // :checked - toggled checkbox/radio, selected option
	ps_disabled = 16u // :disabled - a disabled form control
};

// what a selector is matched against: one element's identity + UI state
struct element_ref {
	std::string_view tag{};
	std::string_view id{};
	std::string_view classes{}; // space-separated, like the class attribute
	unsigned states{};          // pseudo_state bits; 0 = no interaction state
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
