#ifndef CTCSS__TYPES__HPP
#define CTCSS__TYPES__HPP

#include <cstdint>

// module-export seam: ctcss.cppm defines CTCSS_IN_A_MODULE and the
// public names get `export`; plain includes get nothing
#ifdef CTCSS_IN_A_MODULE
#define CTCSS_EXPORT export
#else
#define CTCSS_EXPORT
#endif
#ifndef CTCSS_IN_A_MODULE
#include <cstddef>
#include <string_view>
#include <type_traits>
#endif

// Shared vocabulary for the value parser: the case/blank helpers and
// the rel enum. A selector step's rel says how it attaches to the one
// before it (descendant or child; the first
// step's rel is ignored). A compound carries its type selector, #id
// and .classes as text types (empty text = absent, "*" = universal).

namespace ctcss {

namespace detail {

constexpr char ascii_lower(char c) noexcept {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// CSS whitespace. Lives here (not in the grammar-binding header) so the
// value path - matching + the runtime parser - needs no grammar include.
constexpr bool is_css_blank(char c) noexcept {
	return c == ' ' || c == '\x09' || c == '\x0A' || c == '\x0D';
}

constexpr bool ascii_iequals(std::string_view a, std::string_view b) noexcept {
	if (a.size() != b.size()) { return false; }
	for (std::size_t i = 0; i < a.size(); ++i) {
		if (ascii_lower(a[i]) != ascii_lower(b[i])) { return false; }
	}
	return true;
}

} // namespace detail

// how one selector step attaches to the previous one
enum class rel : unsigned char {
	self,       // the first step of a selector
	descendant, // whitespace combinator
	child       // >
};

} // namespace ctcss

#endif
