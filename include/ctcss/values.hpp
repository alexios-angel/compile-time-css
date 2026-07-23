#ifndef CTCSS__VALUES__HPP
#define CTCSS__VALUES__HPP

#include <cstdint>

#include "types.hpp"
#ifndef CTCSS_IN_A_MODULE
#include <cstddef>
#include <string_view>
#endif

// Typed views over declaration values, parsed on demand (values are
// stored as raw trimmed text). Everything is constexpr, so a layout
// engine can consume lengths and colors at compile time:
//
//   constexpr auto len = ctcss::parse_length("12px");
//   static_assert(len.ok && len.value == 12 && len.u == ctcss::unit::px);
//   constexpr auto c = ctcss::parse_color("#ff8800");
//   static_assert(c.ok && c.r == 255 && c.g == 136 && c.b == 0);

namespace ctcss {

enum class unit : unsigned char { none, px, em, rem, pct, vw, vh };

struct length {
	bool ok = false;
	double value = 0;
	unit u = unit::none;
};

struct color {
	bool ok = false;
	unsigned char r = 0, g = 0, b = 0, a = 255;
};

namespace detail {

constexpr bool is_digit(char c) noexcept {
	return c >= '0' && c <= '9';
}
constexpr std::int32_t hexval(char c) noexcept {
	if (c >= '0' && c <= '9') { return c - '0'; }
	if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
	if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
	return -1;
}

// parse a (possibly signed, possibly fractional) decimal number;
// returns chars consumed (0 = no number)
constexpr std::size_t take_number(std::string_view s, double & out) noexcept {
	std::size_t i = 0;
	bool neg = false;
	if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
		neg = s[i] == '-';
		++i;
	}
	double v = 0;
	std::size_t digits = 0;
	while (i < s.size() && is_digit(s[i])) {
		v = v * 10 + (s[i] - '0');
		++i;
		++digits;
	}
	if (i < s.size() && s[i] == '.') {
		++i;
		double scale = 0.1;
		while (i < s.size() && is_digit(s[i])) {
			v += (s[i] - '0') * scale;
			scale /= 10;
			++i;
			++digits;
		}
	}
	if (digits == 0) { return 0; }
	out = neg ? -v : v;
	return i;
}

} // namespace detail

// "12px", "1.5em", "-2rem", "50%", "0" (unitless zero), "100vh"
CTLL_EXPORT constexpr length parse_length(std::string_view s) noexcept {
	double v = 0;
	const std::size_t used = detail::take_number(s, v);
	if (used == 0) { return {}; }
	const std::string_view rest = s.substr(used);
	if (rest.empty()) { return {true, v, unit::none}; }
	if (rest == "px") { return {true, v, unit::px}; }
	if (rest == "em") { return {true, v, unit::em}; }
	if (rest == "rem") { return {true, v, unit::rem}; }
	if (rest == "%") { return {true, v, unit::pct}; }
	if (rest == "vw") { return {true, v, unit::vw}; }
	if (rest == "vh") { return {true, v, unit::vh}; }
	return {};
}

// #rgb, #rgba, #rrggbb, #rrggbbaa, rgb(r, g, b), rgba(r, g, b, a),
// and the CSS Level 1 named colors + a few staples + transparent
CTLL_EXPORT constexpr color parse_color(std::string_view s) noexcept {
	const auto named = [&](std::string_view n, unsigned char r, unsigned char g,
	                       unsigned char b, unsigned char a = 255) {
		return detail::ascii_iequals(s, n) ? color{true, r, g, b, a} : color{};
	};
	if (!s.empty() && s[0] == '#') {
		const std::string_view h = s.substr(1);
		const auto nib = [&](std::size_t i) { return detail::hexval(h[i]); };
		if (h.size() == 3 || h.size() == 4) {
			std::int32_t parts[4] = {0, 0, 0, 15};
			for (std::size_t i = 0; i < h.size(); ++i) {
				const std::int32_t v = nib(i);
				if (v < 0) { return {}; }
				parts[i] = v;
			}
			return {true, static_cast<unsigned char>(parts[0] * 17),
			        static_cast<unsigned char>(parts[1] * 17),
			        static_cast<unsigned char>(parts[2] * 17),
			        static_cast<unsigned char>(parts[3] * 17)};
		}
		if (h.size() == 6 || h.size() == 8) {
			std::int32_t parts[4] = {0, 0, 0, 255};
			for (std::size_t i = 0; i < h.size(); i += 2) {
				const std::int32_t hi = nib(i);
				const std::int32_t lo = nib(i + 1);
				if (hi < 0 || lo < 0) { return {}; }
				parts[i / 2] = hi * 16 + lo;
			}
			return {true, static_cast<unsigned char>(parts[0]),
			        static_cast<unsigned char>(parts[1]),
			        static_cast<unsigned char>(parts[2]),
			        static_cast<unsigned char>(parts[3])};
		}
		return {};
	}
	if (s.size() > 4 && (s.substr(0, 4) == "rgb(" || s.substr(0, 5) == "rgba(") &&
	    s.back() == ')') {
		std::string_view body = s.substr(s[3] == '(' ? 4 : 5);
		body = body.substr(0, body.size() - 1);
		double parts[4] = {0, 0, 0, 255};
		std::size_t part = 0;
		std::size_t i = 0;
		while (part < 4) {
			while (i < body.size() && detail::is_css_blank(body[i])) { ++i; }
			double v = 0;
			const std::size_t used = detail::take_number(body.substr(i), v);
			if (used == 0) { return {}; }
			i += used;
			parts[part] = part == 3 ? v * 255 : v;
			++part;
			while (i < body.size() && detail::is_css_blank(body[i])) { ++i; }
			if (i == body.size()) { break; }
			if (body[i] != ',') { return {}; }
			++i;
		}
		if (i != body.size()) { return {}; }
		const auto clamp = [](double v) {
			return static_cast<unsigned char>(v < 0 ? 0 : v > 255 ? 255 : v);
		};
		return {true, clamp(parts[0]), clamp(parts[1]), clamp(parts[2]), clamp(parts[3])};
	}
	if (const color c = named("black", 0, 0, 0); c.ok) { return c; }
	if (const color c = named("white", 255, 255, 255); c.ok) { return c; }
	if (const color c = named("red", 255, 0, 0); c.ok) { return c; }
	if (const color c = named("green", 0, 128, 0); c.ok) { return c; }
	if (const color c = named("blue", 0, 0, 255); c.ok) { return c; }
	if (const color c = named("yellow", 255, 255, 0); c.ok) { return c; }
	if (const color c = named("cyan", 0, 255, 255); c.ok) { return c; }
	if (const color c = named("aqua", 0, 255, 255); c.ok) { return c; }
	if (const color c = named("magenta", 255, 0, 255); c.ok) { return c; }
	if (const color c = named("fuchsia", 255, 0, 255); c.ok) { return c; }
	if (const color c = named("gray", 128, 128, 128); c.ok) { return c; }
	if (const color c = named("grey", 128, 128, 128); c.ok) { return c; }
	if (const color c = named("silver", 192, 192, 192); c.ok) { return c; }
	if (const color c = named("maroon", 128, 0, 0); c.ok) { return c; }
	if (const color c = named("olive", 128, 128, 0); c.ok) { return c; }
	if (const color c = named("lime", 0, 255, 0); c.ok) { return c; }
	if (const color c = named("navy", 0, 0, 128); c.ok) { return c; }
	if (const color c = named("teal", 0, 128, 128); c.ok) { return c; }
	if (const color c = named("purple", 128, 0, 128); c.ok) { return c; }
	if (const color c = named("orange", 255, 165, 0); c.ok) { return c; }
	if (const color c = named("transparent", 0, 0, 0, 0); c.ok) { return c; }
	return {};
}

} // namespace ctcss

#endif
