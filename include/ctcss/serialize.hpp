#ifndef CTCSS__SERIALIZE__HPP
#define CTCSS__SERIALIZE__HPP

#include "types.hpp"
#ifndef CTCSS_IN_A_MODULE
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>
#endif

// Compile-time serialization: ctcss::serialize(sheet) renders a
// stylesheet back to minified CSS in static storage and returns a
// std::string_view of it - nothing happens at runtime.
//
//   constexpr auto sheet = ctcss::parse<"p ,  .a  { color : red ; }">();
//   static_assert(ctcss::serialize(sheet) == "p,.a{color:red}");

namespace ctcss {

namespace detail {

// --- size pass

template <typename Compound, size_t... I>
constexpr size_t classes_size(std::index_sequence<I...>) noexcept {
	return ((1 + decltype(Compound::template class_at<I>())::size()) + ... + 0);
}

template <typename Compound> constexpr size_t compound_size(Compound) noexcept {
	size_t total = Compound::tag().size();
	if (!Compound::id().empty()) { total += 1 + Compound::id().size(); }
	total += classes_size<Compound>(std::make_index_sequence<Compound::class_count()>{});
	return total;
}

template <typename... Steps> constexpr size_t selector_size(selector<Steps...>) noexcept {
	size_t total = 0;
	((total += (Steps::relation() == rel::self ? 0 : 1) +
	           compound_size(typename Steps::compound_type{})),
	 ...);
	return total;
}

template <typename... Sels, typename... Decls>
constexpr size_t rule_size(rule<ctll::list<Sels...>, ctll::list<Decls...>>) noexcept {
	size_t total = (selector_size(Sels{}) + ... + 0);
	total += sizeof...(Sels) > 1 ? sizeof...(Sels) - 1 : 0; // commas
	total += 2;                                             // { }
	((total += Decls::property().size() + 1 + Decls::value().size() +
	           (Decls::important() ? 10 : 0)),
	 ...);
	total += sizeof...(Decls) > 1 ? sizeof...(Decls) - 1 : 0; // semicolons
	return total;
}

// --- write pass

constexpr char * write_view(char * out, std::string_view piece) noexcept {
	for (const char c : piece) { *out++ = c; }
	return out;
}

template <typename Compound, size_t... I>
constexpr char * write_classes(char * out, std::index_sequence<I...>) noexcept {
	((*out++ = '.', out = write_view(out, decltype(Compound::template class_at<I>())::view())),
	 ...);
	return out;
}

template <typename Compound> constexpr char * write_compound(char * out, Compound) noexcept {
	out = write_view(out, Compound::tag());
	if (!Compound::id().empty()) {
		*out++ = '#';
		out = write_view(out, Compound::id());
	}
	return write_classes<Compound>(out, std::make_index_sequence<Compound::class_count()>{});
}

template <typename... Steps> constexpr char * write_selector(char * out, selector<Steps...>) noexcept {
	((out = Steps::relation() == rel::self
	            ? out
	            : write_view(out, Steps::relation() == rel::child ? ">" : " "),
	  out = write_compound(out, typename Steps::compound_type{})),
	 ...);
	return out;
}

template <typename... Sels, typename... Decls>
constexpr char * write_rule(char * out, rule<ctll::list<Sels...>, ctll::list<Decls...>>) noexcept {
	bool first = true;
	((out = first ? out : write_view(out, ","), first = false,
	  out = write_selector(out, Sels{})),
	 ...);
	*out++ = '{';
	first = true;
	((out = first ? out : write_view(out, ";"), first = false,
	  out = write_view(out, Decls::property()), *out++ = ':',
	  out = write_view(out, Decls::value()),
	  out = Decls::important() ? write_view(out, "!important") : out),
	 ...);
	*out++ = '}';
	return out;
}

template <typename Sheet> struct serialized_storage;
template <typename... Rules> struct serialized_storage<stylesheet<Rules...>> {
	static constexpr size_t length = (rule_size(Rules{}) + ... + 0);
	static constexpr std::array<char, length + 1> compute() noexcept {
		std::array<char, length + 1> out{};
		char * at = out.data();
		((at = write_rule(at, Rules{})), ...);
		return out;
	}
	static constexpr std::array<char, length + 1> content = compute();
};

} // namespace detail

// minified CSS for a whole stylesheet, in static storage
CTLL_EXPORT template <typename... Rules>
constexpr std::string_view serialize(stylesheet<Rules...> = {}) noexcept {
	using storage = detail::serialized_storage<stylesheet<Rules...>>;
	return std::string_view{storage::content.data(), storage::length};
}

} // namespace ctcss

#endif
