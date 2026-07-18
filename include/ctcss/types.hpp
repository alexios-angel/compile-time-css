#ifndef CTCSS__TYPES__HPP
#define CTCSS__TYPES__HPP

#include "../ctll/fixed_string.hpp"
#ifndef CTCSS_IN_A_MODULE
#include <cstddef>
#include <string_view>
#include <type_traits>
#endif

// The stylesheet types a parse produces. The whole sheet is a TYPE -
// every selector, property and value is encoded in template
// parameters - so the values here are empty structs whose accessors
// are all constexpr and static.
//
// A selector is a chain of sel_step<compound, rel>: rel says how the
// step attaches to the one before it (descendant or child; the first
// step's rel is ignored). A compound carries its type selector, #id
// and .classes as text types (empty text = absent, "*" = universal).

namespace ctcss {

// --- text, used for names, values and selector parts alike

template <auto... Chars> struct text {
	// null-terminated so c_str()/data() work as C strings; size() excludes it
	static constexpr char storage[sizeof...(Chars) + 1]{static_cast<char>(Chars)..., '\0'};

	static constexpr const char * c_str() noexcept { return storage; }
	static constexpr size_t size() noexcept { return sizeof...(Chars); }
	static constexpr bool empty() noexcept { return sizeof...(Chars) == 0; }
	static constexpr std::string_view view() noexcept {
		return std::string_view{storage, sizeof...(Chars)};
	}
	constexpr operator std::string_view() const noexcept { return view(); }
	template <auto... Rhs> constexpr bool operator==(text<Rhs...>) const noexcept {
		return view() == text<Rhs...>::view();
	}
	friend constexpr bool operator==(text, std::string_view rhs) noexcept {
		return view() == rhs;
	}
	friend constexpr bool operator==(std::string_view lhs, text) noexcept {
		return lhs == view();
	}
};

namespace detail {

constexpr char ascii_lower(char c) noexcept {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr bool ascii_iequals(std::string_view a, std::string_view b) noexcept {
	if (a.size() != b.size()) { return false; }
	for (size_t i = 0; i < a.size(); ++i) {
		if (ascii_lower(a[i]) != ascii_lower(b[i])) { return false; }
	}
	return true;
}

template <size_t Index, typename Head, typename... Tail> constexpr auto nth() noexcept {
	if constexpr (Index == 0) {
		return Head{};
	} else {
		return nth<Index - 1, Tail...>();
	}
}

} // namespace detail

// --- selectors

enum class rel : unsigned char {
	self,       // the first step of a selector
	descendant, // whitespace combinator
	child       // >
};

// one compound: tag (empty = any, "*" = any), #id, .classes
template <typename Tag, typename Id, typename Classes> struct compound;
template <typename Tag, typename Id, typename... Classes>
struct compound<Tag, Id, ctll::list<Classes...>> {
	using tag_type = Tag;
	using id_type = Id;

	static constexpr std::string_view tag() noexcept { return Tag::view(); }
	static constexpr std::string_view id() noexcept { return Id::view(); }
	static constexpr size_t class_count() noexcept { return sizeof...(Classes); }
	template <size_t I> static constexpr auto class_at() noexcept {
		static_assert(I < sizeof...(Classes), "ctcss: class index out of range");
		return detail::nth<I, Classes...>();
	}
	// specificity contribution: (ids, classes, types)
	static constexpr int spec_a() noexcept { return Id::empty() ? 0 : 1; }
	static constexpr int spec_b() noexcept { return static_cast<int>(sizeof...(Classes)); }
	static constexpr int spec_c() noexcept {
		return (Tag::empty() || Tag::view() == std::string_view{"*"}) ? 0 : 1;
	}
};

template <typename Compound, rel Rel> struct sel_step {
	using compound_type = Compound;
	static constexpr rel relation() noexcept { return Rel; }
};

template <typename... Steps> struct selector {
	static constexpr size_t step_count() noexcept { return sizeof...(Steps); }
	template <size_t I> static constexpr auto step() noexcept {
		static_assert(I < sizeof...(Steps), "ctcss: step index out of range");
		return detail::nth<I, Steps...>();
	}
	// packed specificity: ids*10000 + classes*100 + types (enough head-
	// room for real stylesheets; ties break on source order anyway)
	static constexpr int specificity() noexcept {
		return ((Steps::compound_type::spec_a() * 10000 +
		         Steps::compound_type::spec_b() * 100 +
		         Steps::compound_type::spec_c()) +
		        ... + 0);
	}
};

// --- declarations and rules

template <typename Prop, typename Value, bool Important> struct declaration {
	using property_type = Prop;
	using value_type = Value;
	static constexpr std::string_view property() noexcept { return Prop::view(); }
	static constexpr std::string_view value() noexcept { return Value::view(); }
	static constexpr bool important() noexcept { return Important; }
};

template <typename Selectors, typename Decls> struct rule;
template <typename... Selectors, typename... Decls>
struct rule<ctll::list<Selectors...>, ctll::list<Decls...>> {
	static constexpr size_t selector_count() noexcept { return sizeof...(Selectors); }
	template <size_t I> static constexpr auto selector_at() noexcept {
		static_assert(I < sizeof...(Selectors), "ctcss: selector index out of range");
		return detail::nth<I, Selectors...>();
	}
	static constexpr size_t decl_count() noexcept { return sizeof...(Decls); }
	template <size_t I> static constexpr auto decl() noexcept {
		static_assert(I < sizeof...(Decls), "ctcss: declaration index out of range");
		return detail::nth<I, Decls...>();
	}

	// the raw value of a property within this rule ("" when absent);
	// property names are case-insensitive and stored lowercase
#if CTLL_CNTTP_COMPILER_CHECK
	template <ctll::fixed_string Key> static constexpr std::string_view property() noexcept {
#else
	template <const auto & Key> static constexpr std::string_view property() noexcept {
#endif
		std::string_view out{};
		(void)((key_matches<Decls>(Key) ? (out = Decls::value(), true) : false) || ...);
		return out;
	}

private:
	template <typename D, typename K> static constexpr bool key_matches(const K & key) noexcept {
		constexpr auto view = D::property_type::view();
		if (key.size() != view.size()) { return false; }
		for (size_t i = 0; i < view.size(); ++i) {
			const char32_t k = key[i];
			const char32_t kf = (k >= U'A' && k <= U'Z') ? k - U'A' + U'a' : k;
			if (static_cast<char32_t>(static_cast<unsigned char>(view[i])) != kf) {
				return false;
			}
		}
		return true;
	}
};

template <typename... Rules> struct stylesheet {
	static constexpr size_t rule_count() noexcept { return sizeof...(Rules); }
	template <size_t I> static constexpr auto rule_at() noexcept {
		static_assert(I < sizeof...(Rules), "ctcss: rule index out of range");
		return detail::nth<I, Rules...>();
	}
};

// compile-time iteration
CTLL_EXPORT template <typename F, typename... Rules>
constexpr void for_each_rule(stylesheet<Rules...>, F && f) {
	(f(Rules{}), ...);
}

} // namespace ctcss

#endif
