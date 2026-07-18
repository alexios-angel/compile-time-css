#ifndef CTCSS__MATCH__HPP
#define CTCSS__MATCH__HPP

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
	size_t i = 0;
	while (i < classes.size()) {
		while (i < classes.size() && is_css_blank(classes[i])) { ++i; }
		size_t j = i;
		while (j < classes.size() && !is_css_blank(classes[j])) { ++j; }
		if (j > i && classes.substr(i, j - i) == name) { return true; }
		i = j;
	}
	return false;
}

} // namespace detail

// one selector step, as plain data
struct step_view {
	std::string_view tag{};
	std::string_view id{};
	const std::string_view * classes = nullptr;
	size_t class_count = 0;
	rel relation = rel::self;
};

struct selector_view {
	const step_view * steps = nullptr;
	size_t count = 0;
	int spec = 0;
};

namespace detail {

template <typename Compound> struct compound_classes;
template <typename Tag, typename Id, typename... Cs>
struct compound_classes<compound<Tag, Id, ctll::list<Cs...>>> {
	static constexpr std::array<std::string_view, sizeof...(Cs) + 1> data{Cs::view()...};
};

template <typename Step> constexpr step_view step_view_of() noexcept {
	using C = typename Step::compound_type;
	return step_view{C::tag(), C::id(), compound_classes<C>::data.data(),
	                 C::class_count(), Step::relation()};
}

template <typename Selector> struct selector_data;
template <typename... Steps> struct selector_data<selector<Steps...>> {
	static constexpr std::array<step_view, sizeof...(Steps)> steps{step_view_of<Steps>()...};
	static constexpr selector_view view{steps.data(), sizeof...(Steps),
	                                    selector<Steps...>::specificity()};
};

constexpr bool matches_step(const step_view & s, const element_ref & e) noexcept {
	if (!s.tag.empty() && s.tag != std::string_view{"*"} &&
	    !ascii_iequals(s.tag, e.tag)) {
		return false;
	}
	if (!s.id.empty() && s.id != e.id) { return false; }
	for (size_t i = 0; i < s.class_count; ++i) {
		if (!has_class(e.classes, s.classes[i])) { return false; }
	}
	return true;
}

// does steps[0..si] match with steps[si] anchored at chain[ci]?
// (a step's relation says how it attaches to the step on its LEFT)
constexpr bool match_at(const selector_view & sel, size_t si, const element_ref * chain,
                        size_t ci) noexcept {
	if (!matches_step(sel.steps[si], chain[ci])) { return false; }
	if (si == 0) { return true; }
	if (sel.steps[si].relation == rel::child) {
		return ci > 0 && match_at(sel, si - 1, chain, ci - 1);
	}
	// descendant: anchor the previous step at ANY strict ancestor
	for (size_t a = ci; a > 0; --a) {
		if (match_at(sel, si - 1, chain, a - 1)) { return true; }
	}
	return false;
}

constexpr bool matches_view(const selector_view & sel, const element_ref * chain,
                            size_t n) noexcept {
	return n > 0 && sel.count > 0 && match_at(sel, sel.count - 1, chain, n - 1);
}

} // namespace detail

// --- the public matching surface (selector TYPES against value chains)

CTLL_EXPORT template <typename... Steps>
constexpr selector_view view_of(selector<Steps...>) noexcept {
	return detail::selector_data<selector<Steps...>>::view;
}

CTLL_EXPORT template <typename... Steps, size_t N>
constexpr bool matches(selector<Steps...> s, const element_ref (&chain)[N]) noexcept {
	return detail::matches_view(view_of(s), chain, N);
}
CTLL_EXPORT template <typename... Steps>
constexpr bool matches(selector<Steps...> s, const element_ref * chain, size_t n) noexcept {
	return detail::matches_view(view_of(s), chain, n);
}

// --- the flattened sheet and the cascade

struct decl_entry {
	selector_view sel{};
	int order = 0;
	std::string_view property{};
	std::string_view value{};
	bool important = false;
};

namespace detail {

template <typename Rule> struct rule_shape;
template <typename... Sels, typename... Decls>
struct rule_shape<rule<ctll::list<Sels...>, ctll::list<Decls...>>> {
	static constexpr size_t entries = sizeof...(Sels) * sizeof...(Decls);

	template <typename Out>
	static constexpr void fill(Out & out, size_t & at, int & order) noexcept {
		(fill_selector<Sels>(out, at, order), ...);
		order += static_cast<int>(sizeof...(Decls));
	}
	template <typename Sel, typename Out>
	static constexpr void fill_selector(Out & out, size_t & at, int order) noexcept {
		((out[at++] = decl_entry{selector_data<Sel>::view, order++,
		                         Decls::property_type::view(), Decls::value_type::view(),
		                         Decls::important()}),
		 ...);
	}
};

template <typename Sheet> struct sheet_data;
template <typename... Rules> struct sheet_data<stylesheet<Rules...>> {
	static constexpr size_t count = (rule_shape<Rules>::entries + ... + 0);
	static constexpr auto build() noexcept {
		std::array<decl_entry, count == 0 ? 1 : count> out{};
		size_t at = 0;
		int order = 0;
		(rule_shape<Rules>::fill(out, at, order), ...);
		return out;
	}
	static constexpr std::array<decl_entry, count == 0 ? 1 : count> entries = build();
};

} // namespace detail

// every (selector, declaration) pair of the sheet, in document order
CTLL_EXPORT template <typename... Rules>
constexpr const decl_entry * entries(stylesheet<Rules...>) noexcept {
	return detail::sheet_data<stylesheet<Rules...>>::entries.data();
}
CTLL_EXPORT template <typename... Rules>
constexpr size_t entry_count(stylesheet<Rules...>) noexcept {
	return detail::sheet_data<stylesheet<Rules...>>::count;
}

// resolve one property for one element (chain root-first, self-last)
// through the cascade: !important, then specificity, then source order.
// "" when nothing matches.
CTLL_EXPORT template <typename... Rules>
constexpr std::string_view query(stylesheet<Rules...> sheet, const element_ref * chain,
                                 size_t n, std::string_view property) noexcept {
	std::string_view best{};
	int best_spec = -1;
	int best_order = -1;
	bool best_important = false;
	const decl_entry * es = entries(sheet);
	for (size_t i = 0; i < entry_count(sheet); ++i) {
		const decl_entry & e = es[i];
		if (!detail::ascii_iequals(e.property, property)) { continue; }
		if (!detail::matches_view(e.sel, chain, n)) { continue; }
		const bool wins = (e.important != best_important)
		                      ? e.important
		                      : (e.sel.spec != best_spec ? e.sel.spec > best_spec
		                                                 : e.order >= best_order);
		if (wins) {
			best = e.value;
			best_spec = e.sel.spec;
			best_order = e.order;
			best_important = e.important;
		}
	}
	return best;
}

CTLL_EXPORT template <typename... Rules, size_t N>
constexpr std::string_view query(stylesheet<Rules...> sheet, const element_ref (&chain)[N],
                                 std::string_view property) noexcept {
	return query(sheet, chain, N, property);
}

} // namespace ctcss

#endif
