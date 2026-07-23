#ifndef CTCSS__VALUE__HPP
#define CTCSS__VALUE__HPP

#include <cstddef>

#include <cstdint>

#include "match.hpp"
#include "values.hpp"
#ifndef CTCSS_IN_A_MODULE
#include <string>
#include <string_view>
#include <vector>
#endif

// A stylesheet parsed BY VALUE from a runtime std::string_view - the runtime
// twin of parse<>()'s compile-time TYPE. ctlark's Earley parse is superlinear
// (the compile-time wall for big pages, just like ctjs/cthtml); this is a
// hand-written LINEAR recursive-descent parser producing an OWNING value
// stylesheet, walked by the same cascade match.hpp uses on the type path.
//
// Two deliberate differences from the type path:
//   * O(n), no template instantiation - a large stylesheet costs the page's
//     translation unit nothing (it is parsed at runtime, from a string).
//   * LENIENT - unsupported constructs (@media/@font-face/@keyframes,
//     malformed declarations) are skipped, not fatal. @media/@supports blocks
//     recurse (their condition is ignored, inner rules apply); declaration
//     at-rules are dropped. So real-world CSS parses instead of failing.
//
// Supported subset mirrors the grammar: comma selector lists; compound
// tag/#id/.class with descendant (whitespace) and child (>) combinators;
// `prop: value [!important]` declarations; C-style comments. The cascade is
// identical to the type path: !important, then specificity, then order.

namespace ctcss {

struct value_sheet {
	struct compound {
		std::string tag;
		std::string id;
		std::vector<std::string> classes;
	};
	struct step {
		compound comp;
		rel relation = rel::self; // how this step attaches to the one on its LEFT
	};
	struct selector {
		std::vector<step> steps;
		std::int32_t spec = 0;
	};
	struct entry {
		std::int32_t selector = -1; // index into `selectors`
		std::int32_t order = 0;
		std::string property;
		std::string value;
		bool important = false;
	};

	// a plain declaration (used by @font-face and @keyframes bodies)
	struct declaration {
		std::string property;
		std::string value;
	};
	// @font-face { font-family: X; src: url(...); ... }
	struct font_face {
		std::vector<declaration> decls;
		// the value of `prop` in this @font-face ("" if absent), unquoted
		constexpr std::string_view get(std::string_view prop) const noexcept {
			for (const declaration & d : decls) {
				if (d.property.size() == prop.size()) {
					bool eq = true;
					for (std::size_t i = 0; i < prop.size(); ++i) {
						char a = d.property[i], b = prop[i];
						if (a >= 'A' && a <= 'Z') { a = static_cast<char>(a - 'A' + 'a'); }
						if (b >= 'A' && b <= 'Z') { b = static_cast<char>(b - 'A' + 'a'); }
						if (a != b) { eq = false; break; }
					}
					if (eq) { return d.value; }
				}
			}
			return {};
		}
	};
	// one keyframe stop: its position (0..1) and the declarations that apply there
	struct keyframe {
		double at = 0; // 0..1
		std::vector<declaration> decls;
	};
	// @keyframes NAME { 0% {...} 100% {...} }
	struct keyframes_rule {
		std::string name;
		std::vector<keyframe> frames;
	};

	std::vector<selector> selectors;
	std::vector<entry> entries;
	std::vector<font_face> font_faces;
	std::vector<keyframes_rule> keyframes;
	std::size_t rules = 0;

	constexpr std::size_t rule_count() const noexcept { return rules; }
	constexpr std::size_t entry_count() const noexcept { return entries.size(); }

	// find a @keyframes animation by name (nullptr if none)
	constexpr const keyframes_rule * animation(std::string_view name) const noexcept {
		for (const keyframes_rule & k : keyframes) {
			if (std::string_view{k.name} == name) { return &k; }
		}
		return nullptr;
	}
};

namespace detail {

// replace every /* ... */ comment with a single space, so comments may sit
// anywhere (inside selectors, inside declaration bodies) - the grammar path
// gets this from %ignore; the linear parser gets it from this pre-pass.
constexpr std::string strip_css_comments(std::string_view s) {
	std::string out;
	std::size_t i = 0;
	while (i < s.size()) {
		if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
			i += 2;
			while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) { ++i; }
			i = (i + 1 < s.size()) ? i + 2 : s.size();
			out += ' ';
		} else {
			out += s[i++];
		}
	}
	return out;
}

// std::string from a view, CONSTEXPR-SAFE on the std::embed clang: its
// libstdc++ ships _M_construct<const char*> as an extern template (declared,
// not constexpr-defined), so `std::string{sv}` / `std::string(p,n)` fail in a
// constant expression - but append()/push_back() fold fine. The whole ct*
// stack constructs constexpr strings this way.
constexpr std::string v_str(std::string_view v) {
	std::string s;
	s.append(v.data(), v.size());
	return s;
}

constexpr std::string_view v_trim(std::string_view s) noexcept {
	std::size_t a = 0, b = s.size();
	while (a < b && is_css_blank(s[a])) { ++a; }
	while (b > a && is_css_blank(s[b - 1])) { --b; }
	return s.substr(a, b - a);
}

constexpr std::int32_t v_spec_of(const value_sheet::selector & s) noexcept {
	std::int32_t out = 0;
	for (const auto & st : s.steps) {
		out += (st.comp.id.empty() ? 0 : 10000) +
		       static_cast<std::int32_t>(st.comp.classes.size()) * 100 +
		       ((st.comp.tag.empty() || st.comp.tag == std::string_view{"*"}) ? 0 : 1);
	}
	return out;
}

// split "div.note#top" into tag + #id + .classes (empty tag = any)
constexpr value_sheet::compound v_split_compound(std::string_view tok) {
	value_sheet::compound c;
	std::size_t t = 0;
	while (t < tok.size() && tok[t] != '.' && tok[t] != '#') { ++t; }
	c.tag = v_str(tok.substr(0, t));
	std::size_t p = t;
	while (p < tok.size()) {
		const char kind = tok[p++];
		std::size_t q = p;
		while (q < tok.size() && tok[q] != '.' && tok[q] != '#') { ++q; }
		std::string name = v_str(tok.substr(p, q - p));
		if (!name.empty()) {
			if (kind == '#') {
				c.id = std::move(name);
			} else {
				c.classes.push_back(std::move(name));
			}
		}
		p = q;
	}
	return c;
}

// parse one selector ("div.note > p") into steps
constexpr value_sheet::selector v_parse_selector(std::string_view sel) {
	value_sheet::selector out;
	std::size_t k = 0;
	bool first = true, pending_child = false;
	while (k < sel.size()) {
		while (k < sel.size() && is_css_blank(sel[k])) { ++k; }
		if (k >= sel.size()) { break; }
		if (sel[k] == '>') {
			pending_child = true;
			++k;
			continue;
		}
		const std::size_t start = k;
		while (k < sel.size() && !is_css_blank(sel[k]) && sel[k] != '>') { ++k; }
		const std::string_view tok = sel.substr(start, k - start);
		if (tok.empty()) { continue; }
		value_sheet::step st;
		st.comp = v_split_compound(tok);
		st.relation = first ? rel::self : (pending_child ? rel::child : rel::descendant);
		out.steps.push_back(std::move(st));
		first = false;
		pending_child = false;
	}
	out.spec = v_spec_of(out);
	return out;
}

// strip one layer of matching surrounding quotes (font-family "X", url('...'))
constexpr std::string_view unquote(std::string_view s) noexcept {
	if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
		return s.substr(1, s.size() - 2);
	}
	return s;
}

// parse a leading (optionally signed, optionally fractional) number; trailing
// units (%, px, ...) are ignored. 0 if none.
constexpr double v_num(std::string_view s) noexcept {
	std::size_t i = 0;
	bool neg = false;
	if (i < s.size() && (s[i] == '+' || s[i] == '-')) { neg = s[i] == '-'; ++i; }
	double v = 0;
	while (i < s.size() && s[i] >= '0' && s[i] <= '9') { v = v * 10 + (s[i] - '0'); ++i; }
	if (i < s.size() && s[i] == '.') {
		++i;
		double f = 0.1;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9') { v += (s[i] - '0') * f; f *= 0.1; ++i; }
	}
	return neg ? -v : v;
}

// split a declaration body ("a: 1; b: 2") into declarations (property/value),
// trimming and peeling a trailing !important. Shared by rules, @font-face and
// @keyframes stops.
constexpr std::vector<value_sheet::declaration> v_parse_decls(std::string_view body) {
	std::vector<value_sheet::declaration> out;
	std::size_t ds = 0;
	for (std::size_t k = 0; k <= body.size(); ++k) {
		if (k == body.size() || body[k] == ';') {
			const std::string_view dt = v_trim(body.substr(ds, k - ds));
			ds = k + 1;
			if (dt.empty()) { continue; }
			const std::size_t colon = dt.find(':');
			if (colon == std::string_view::npos) { continue; }
			const std::string_view prop = v_trim(dt.substr(0, colon));
			std::string_view val = v_trim(dt.substr(colon + 1));
			const std::size_t bang = val.rfind('!');
			if (bang != std::string_view::npos &&
			    ascii_iequals(v_trim(val.substr(bang + 1)), std::string_view{"important"})) {
				val = v_trim(val.substr(0, bang));
			}
			if (prop.empty() || val.empty()) { continue; }
			out.push_back({v_str(prop), v_str(val)});
		}
	}
	return out;
}

struct css_parser {
	std::string_view s;
	std::size_t i = 0;
	value_sheet & out;
	std::int32_t order = 0;

	constexpr void skip_ws() {
		for (;;) {
			while (i < s.size() && is_css_blank(s[i])) { ++i; }
			if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
				i += 2;
				while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) { ++i; }
				i = (i + 1 < s.size()) ? i + 2 : s.size();
				continue;
			}
			break;
		}
	}
	constexpr void run() { parse_rules(false); }
	constexpr void parse_rules(bool nested) {
		for (;;) {
			skip_ws();
			if (i >= s.size()) { return; }
			if (s[i] == '}') {
				++i;
				if (nested) { return; }
				continue; // stray close at top level
			}
			if (s[i] == '@') {
				at_rule();
				continue;
			}
			parse_rule();
		}
	}
	// skip a balanced { ... } block; enter at the char AFTER the '{'
	constexpr void skip_block() {
		std::int32_t depth = 1;
		while (i < s.size() && depth > 0) {
			if (s[i] == '{') { ++depth; }
			else if (s[i] == '}') { --depth; }
			++i;
		}
	}
	// @font-face + @keyframes are CAPTURED; @media/@supports/@document/@layer
	// recurse (condition ignored - fine for screen); @import/@charset/@namespace
	// end at ';'; anything else (@page, ...) is a skipped balanced block.
	constexpr void at_rule() {
		++i; // past '@'
		const std::size_t ns = i;
		while (i < s.size() && (s[i] == '-' || (s[i] >= 'a' && s[i] <= 'z') ||
		                        (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9'))) { ++i; }
		std::string_view name = s.substr(ns, i - ns);
		// strip a vendor prefix (-webkit-keyframes -> keyframes)
		for (const std::string_view p : {std::string_view{"-webkit-"}, std::string_view{"-moz-"},
		                                 std::string_view{"-o-"}, std::string_view{"-ms-"}}) {
			if (name.size() > p.size() && name.substr(0, p.size()) == p) {
				name = name.substr(p.size());
				break;
			}
		}
		const std::size_t ps = i;
		while (i < s.size() && s[i] != '{' && s[i] != ';' && s[i] != '}') { ++i; }
		const std::string_view prelude = v_trim(s.substr(ps, i - ps));
		if (i >= s.size() || s[i] == '}') { return; }
		if (s[i] == ';') { ++i; return; } // @import / @charset / @namespace
		++i;                              // past '{'
		if (ascii_iequals(name, std::string_view{"font-face"})) {
			const std::size_t bstart = i;
			while (i < s.size() && s[i] != '}') { ++i; }
			value_sheet::font_face ff;
			ff.decls = v_parse_decls(s.substr(bstart, i - bstart));
			if (i < s.size()) { ++i; }
			if (!ff.decls.empty()) { out.font_faces.push_back(std::move(ff)); }
		} else if (ascii_iequals(name, std::string_view{"keyframes"})) {
			parse_keyframes(unquote(prelude));
		} else if (ascii_iequals(name, std::string_view{"media"})) {
			// we render a single landscape, screen-sized viewport: apply the block
			// unless it is gated on portrait or print (which we are not). Full
			// viewport-dependent media-query evaluation is not modeled.
			const bool skip = prelude.find("portrait") != std::string_view::npos ||
			                  prelude.find("print") != std::string_view::npos;
			if (skip) { skip_block(); } else { parse_rules(true); }
		} else if (ascii_iequals(name, std::string_view{"supports"}) ||
		           ascii_iequals(name, std::string_view{"document"}) ||
		           ascii_iequals(name, std::string_view{"layer"})) {
			parse_rules(true);
		} else {
			skip_block(); // @page / @counter-style / ... - not modeled
		}
	}
	// parse a @keyframes body: each `<stop-list> { decls }` where a stop is a
	// percentage or from/to; one keyframe per stop (all sharing the decls).
	constexpr void parse_keyframes(std::string_view name) {
		value_sheet::keyframes_rule kf;
		kf.name = v_str(name);
		for (;;) {
			skip_ws();
			if (i >= s.size()) { break; }
			if (s[i] == '}') { ++i; break; } // end of the @keyframes block
			const std::size_t ss = i;
			while (i < s.size() && s[i] != '{' && s[i] != '}') { ++i; }
			if (i >= s.size() || s[i] == '}') {
				if (i < s.size()) { ++i; }
				break;
			}
			const std::string_view stops = s.substr(ss, i - ss);
			++i; // past '{'
			const std::size_t bstart = i;
			while (i < s.size() && s[i] != '}') { ++i; }
			const std::vector<value_sheet::declaration> decls = v_parse_decls(s.substr(bstart, i - bstart));
			if (i < s.size()) { ++i; }
			std::size_t start = 0;
			for (std::size_t k = 0; k <= stops.size(); ++k) {
				if (k == stops.size() || stops[k] == ',') {
					const std::string_view one = v_trim(stops.substr(start, k - start));
					start = k + 1;
					if (one.empty()) { continue; }
					double at = 0;
					if (ascii_iequals(one, std::string_view{"from"})) { at = 0.0; }
					else if (ascii_iequals(one, std::string_view{"to"})) { at = 1.0; }
					else { at = v_num(one) / 100.0; }
					kf.frames.push_back(value_sheet::keyframe{at, decls});
				}
			}
		}
		if (!kf.name.empty() && !kf.frames.empty()) { out.keyframes.push_back(std::move(kf)); }
	}
	constexpr void parse_rule() {
		const std::size_t start = i;
		while (i < s.size() && s[i] != '{' && s[i] != '}') { ++i; }
		if (i >= s.size() || s[i] == '}') {
			// no rule body before end/close: junk (e.g. @font-face
			// declarations reached via recursion). Leave the '}' for the
			// caller loop; if it was a top-level fragment, drop it.
			return;
		}
		const std::string_view sel_text = s.substr(start, i - start);
		++i; // past '{'
		const std::size_t bstart = i;
		while (i < s.size() && s[i] != '}') { ++i; }
		const std::string_view body = s.substr(bstart, i - bstart);
		if (i < s.size()) { ++i; } // past '}'
		add_rule(sel_text, body);
	}
	constexpr void add_rule(std::string_view sel_text, std::string_view body) {
		std::vector<std::int32_t> sel_indices;
		std::size_t start = 0;
		for (std::size_t k = 0; k <= sel_text.size(); ++k) {
			if (k == sel_text.size() || sel_text[k] == ',') {
				const std::string_view piece = v_trim(sel_text.substr(start, k - start));
				if (!piece.empty()) {
					value_sheet::selector sel = v_parse_selector(piece);
					if (!sel.steps.empty()) {
						out.selectors.push_back(std::move(sel));
						sel_indices.push_back(static_cast<std::int32_t>(out.selectors.size() - 1));
					}
				}
				start = k + 1;
			}
		}
		if (sel_indices.empty()) { return; }

		struct pv {
			std::string prop;
			std::string val;
			bool imp;
		};
		std::vector<pv> decls;
		std::size_t ds = 0;
		for (std::size_t k = 0; k <= body.size(); ++k) {
			if (k == body.size() || body[k] == ';') {
				const std::string_view dt = v_trim(body.substr(ds, k - ds));
				ds = k + 1;
				if (dt.empty()) { continue; }
				const std::size_t colon = dt.find(':');
				if (colon == std::string_view::npos) { continue; }
				const std::string_view prop = v_trim(dt.substr(0, colon));
				std::string_view val = v_trim(dt.substr(colon + 1));
				if (prop.empty() || val.empty()) { continue; }
				bool imp = false;
				const std::size_t bang = val.rfind('!');
				if (bang != std::string_view::npos &&
				    ascii_iequals(v_trim(val.substr(bang + 1)), std::string_view{"important"})) {
					imp = true;
					val = v_trim(val.substr(0, bang));
				}
				if (val.empty()) { continue; }
				decls.push_back({v_str(prop), v_str(val), imp});
			}
		}
		for (const std::int32_t si : sel_indices) {
			for (const pv & d : decls) {
				out.entries.push_back(value_sheet::entry{si, order++, d.prop, d.val, d.imp});
			}
		}
		out.rules += 1;
	}
};

// --- the value matcher (the twin of match.hpp's, over owned data)

constexpr bool v_matches_step(const value_sheet::step & s, const element_ref & e) noexcept {
	if (!s.comp.tag.empty() && s.comp.tag != std::string_view{"*"} &&
	    !ascii_iequals(s.comp.tag, e.tag)) {
		return false;
	}
	if (!s.comp.id.empty() && std::string_view{s.comp.id} != e.id) { return false; }
	for (const auto & c : s.comp.classes) {
		if (!has_class(e.classes, c)) { return false; }
	}
	return true;
}
constexpr bool v_match_at(const value_sheet::selector & sel, std::size_t si,
                          const element_ref * chain, std::size_t ci) noexcept {
	if (!v_matches_step(sel.steps[si], chain[ci])) { return false; }
	if (si == 0) { return true; }
	if (sel.steps[si].relation == rel::child) {
		return ci > 0 && v_match_at(sel, si - 1, chain, ci - 1);
	}
	for (std::size_t a = ci; a > 0; --a) {
		if (v_match_at(sel, si - 1, chain, a - 1)) { return true; }
	}
	return false;
}
constexpr bool v_matches(const value_sheet::selector & sel, const element_ref * chain,
                         std::size_t n) noexcept {
	return n > 0 && !sel.steps.empty() && v_match_at(sel, sel.steps.size() - 1, chain, n - 1);
}

} // namespace detail

// parse a stylesheet BY VALUE (linear, lenient); the runtime twin of parse<>()
CTLL_EXPORT constexpr value_sheet parse_value(std::string_view css) {
	value_sheet out;
	const std::string cleaned = detail::strip_css_comments(css);
	detail::css_parser p{cleaned, 0, out, 0};
	p.run();
	return out;
}

// resolve one property for one element chain (root-first, self-last) through
// the cascade: !important, then specificity, then source order. "" if none.
CTLL_EXPORT constexpr std::string_view query(const value_sheet & sheet, const element_ref * chain,
                                             std::size_t n, std::string_view property) noexcept {
	std::string_view best{};
	std::int32_t best_spec = -1;
	std::int32_t best_order = -1;
	bool best_important = false;
	for (const value_sheet::entry & e : sheet.entries) {
		if (!detail::ascii_iequals(e.property, property)) { continue; }
		if (e.selector < 0 || static_cast<std::size_t>(e.selector) >= sheet.selectors.size()) {
			continue;
		}
		const value_sheet::selector & sel = sheet.selectors[static_cast<std::size_t>(e.selector)];
		if (!detail::v_matches(sel, chain, n)) { continue; }
		const bool wins = (e.important != best_important)
		                      ? e.important
		                      : (sel.spec != best_spec ? sel.spec > best_spec
		                                               : e.order >= best_order);
		if (wins) {
			best = e.value;
			best_spec = sel.spec;
			best_order = e.order;
			best_important = e.important;
		}
	}
	return best;
}

CTLL_EXPORT template <std::size_t N>
constexpr std::string_view query(const value_sheet & sheet, const element_ref (&chain)[N],
                                 std::string_view property) noexcept {
	return query(sheet, chain, N, property);
}

} // namespace ctcss

#endif
