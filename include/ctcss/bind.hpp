#ifndef CTCSS__BIND__HPP
#define CTCSS__BIND__HPP

#include "grammar.hpp"
#include "types.hpp"
#ifndef CTCSS_IN_A_MODULE
#include <cstddef>
#include <string_view>
#include <utility>
#endif

// Lowering the ctlark parse tree into stylesheet types. The work here
// is textual: a COMPOUND token ("div.note#top") splits into its tag,
// #id and .classes (type selectors fold to lowercase - they match
// case-insensitively against HTML - ids and classes stay exact);
// declaration values trim their whitespace and peel a trailing
// "!important"; property names fold to lowercase. Selector trees
// (left-nested descendant/child nodes) flatten into sel_step chains.

namespace ctcss::detail {

constexpr bool is_css_blank(char c) noexcept {
	return c == ' ' || c == '\x09' || c == '\x0A' || c == '\x0D';
}

// --- span lifts

template <typename Text, size_t From, size_t Len, bool Lower> struct lift_span {
	template <size_t... I> static constexpr auto go(std::index_sequence<I...>) noexcept {
		if constexpr (Lower) {
			return ctcss::text<ascii_lower(Text::view()[From + I])...>{};
		} else {
			return ctcss::text<Text::view()[From + I]...>{};
		}
	}
	using type = decltype(go(std::make_index_sequence<Len>{}));
};

// --- COMPOUND splitting: "div.a#b.c" -> tag "div", id "b", classes {a, c}

template <typename Text> struct compound_parse {
	static constexpr std::string_view s() noexcept { return Text::view(); }

	static constexpr size_t tag_len() noexcept {
		size_t i = 0;
		while (i < s().size() && s()[i] != '.' && s()[i] != '#') { ++i; }
		return i;
	}
	static constexpr size_t unit_count() noexcept {
		size_t n = 0;
		for (const char c : s()) {
			if (c == '.' || c == '#') { ++n; }
		}
		return n;
	}
	// start offset of the K-th [.#] delimiter
	static constexpr size_t unit_start(size_t k) noexcept {
		size_t seen = 0;
		for (size_t i = 0; i < s().size(); ++i) {
			if (s()[i] == '.' || s()[i] == '#') {
				if (seen == k) { return i; }
				++seen;
			}
		}
		return s().size();
	}
	static constexpr size_t unit_end(size_t k) noexcept {
		for (size_t i = unit_start(k) + 1; i < s().size(); ++i) {
			if (s()[i] == '.' || s()[i] == '#') { return i; }
		}
		return s().size();
	}

	using tag_type = typename lift_span<Text, 0, tag_len(), true>::type;
	template <size_t K> using unit_type =
	    typename lift_span<Text, unit_start(K) + 1, unit_end(K) - unit_start(K) - 1,
	                       false>::type;

	// fold the units into (id, classes) - a later #id wins
	template <size_t K, typename Id, typename Classes> static constexpr auto fold() noexcept {
		if constexpr (K == unit_count()) {
			return compound<tag_type, Id, Classes>{};
		} else if constexpr (s()[unit_start(K)] == '#') {
			return fold<K + 1, unit_type<K>, Classes>();
		} else {
			return fold_class<K, Id>(Classes{});
		}
	}
	template <size_t K, typename Id, typename... Cs>
	static constexpr auto fold_class(ctll::list<Cs...>) noexcept {
		return fold<K + 1, Id, ctll::list<Cs..., unit_type<K>>>();
	}

	using type = decltype(fold<0, ctcss::text<>, ctll::list<>>());
};

template <typename Token> using bind_compound =
    typename compound_parse<typename Token::value_type>::type;

// --- selector trees -> sel_step chains

template <typename Sel, typename Step> struct selector_append;
template <typename... Steps, typename Step>
struct selector_append<selector<Steps...>, Step> {
	using type = selector<Steps..., Step>;
};

template <typename Node> struct bind_selector;
// a lone COMPOUND token (the ?selector single-child case)
template <typename TName, typename TVal>
struct bind_selector<ctlark::token<TName, TVal>> {
	using type = selector<sel_step<typename compound_parse<TVal>::type, rel::self>>;
};
template <typename TName, typename Left, typename Comp>
struct bind_selector<ctlark::tree<TName, Left, Comp>> {
	static constexpr rel relation() noexcept {
		return TName::view() == std::string_view{"child"} ? rel::child : rel::descendant;
	}
	using type = typename selector_append<
	    typename bind_selector<Left>::type,
	    sel_step<bind_compound<Comp>, relation()>>::type;
};

// --- declarations: property lowercased, value trimmed, !important peeled

template <typename Text> struct cook_value {
	static constexpr std::string_view s() noexcept { return Text::view(); }

	static constexpr size_t from() noexcept {
		size_t i = 0;
		while (i < s().size() && is_css_blank(s()[i])) { ++i; }
		return i;
	}
	static constexpr size_t trimmed_end(size_t end) noexcept {
		while (end > from() && is_css_blank(s()[end - 1])) { --end; }
		return end;
	}
	// does [from, end) end with '!' ws* "important" (case-insensitive)?
	static constexpr size_t important_bang(size_t end) noexcept {
		constexpr std::string_view word = "important";
		if (end - from() < word.size() + 1) { return s().size() + 1; } // sentinel: no
		size_t w = end - word.size();
		for (size_t i = 0; i < word.size(); ++i) {
			if (ascii_lower(s()[w + i]) != word[i]) { return s().size() + 1; }
		}
		size_t b = w;
		while (b > from() && is_css_blank(s()[b - 1])) { --b; }
		if (b == from() || s()[b - 1] != '!') { return s().size() + 1; }
		return b - 1;
	}

	static constexpr size_t end0() noexcept { return trimmed_end(s().size()); }
	static constexpr bool important() noexcept {
		return important_bang(end0()) <= s().size();
	}
	static constexpr size_t value_end() noexcept {
		return important() ? trimmed_end(important_bang(end0())) : end0();
	}

	using type = typename lift_span<Text, from(), value_end() - from(), false>::type;
};

template <typename Node> struct bind_decl;
template <typename TName, typename Prop, typename Val>
struct bind_decl<ctlark::tree<TName, Prop, Val>> {
	using prop_type =
	    typename lift_span<typename Prop::value_type, 0, Prop::value_type::size(), true>::type;
	using cooked = cook_value<typename Val::value_type>;
	using type = declaration<prop_type, typename cooked::type, cooked::important()>;
};

// --- rules and the sheet

template <typename Node> struct bind_selectors;
template <typename TName, typename... Sels>
struct bind_selectors<ctlark::tree<TName, Sels...>> {
	using type = ctll::list<typename bind_selector<Sels>::type...>;
};

template <typename Node> struct bind_body;
template <typename TName, typename... Decls>
struct bind_body<ctlark::tree<TName, Decls...>> {
	using type = ctll::list<typename bind_decl<Decls>::type...>;
};

template <typename Node> struct bind_rule;
template <typename TName, typename Sels, typename Body>
struct bind_rule<ctlark::tree<TName, Sels, Body>> {
	using type = rule<typename bind_selectors<Sels>::type, typename bind_body<Body>::type>;
};

template <typename Tree> struct bind_sheet;
template <typename TName, typename... Rules>
struct bind_sheet<ctlark::tree<TName, Rules...>> {
	using type = stylesheet<typename bind_rule<Rules>::type...>;
};

} // namespace ctcss::detail

#endif
