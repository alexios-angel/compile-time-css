// The browser story in miniature: a stylesheet proven at compile
// time, a DOM-shaped chain of elements, and style RESOLUTION - match,
// specificity, cascade - happening in static_asserts. The same
// query() calls work at runtime, which is what a browser needs the
// moment a script flips a class.
//
// Build: make theme

#include <ctcss.hpp>
#include <iostream>

inline constexpr std::string_view theme_css = R"(
	*          { box-sizing: border-box; }
	body       { color: #222222; font-size: 16px; }
	h1         { font-size: 32px; margin: 8px 0; }
	.card      { padding: 12px; background: white; }
	.card h1   { font-size: 24px; }
	#hero      { background: #ff8800; }
	#hero > h1 { color: white !important; }
	.dark .card { background: #333333; color: #eeeeee; }
)";

// the page:  body > div#hero.card > h1
constexpr ctcss::element_ref hero_h1[] = {
    {"body"}, {"div", "hero", "card"}, {"h1"}};

// resolution, proven during compilation (parse_value folds; an owned
// constexpr sheet cannot outlive constant evaluation, so each proof
// parses inside its own expression)
static_assert(ctcss::query(ctcss::parse_value(theme_css), hero_h1, "font-size") == "24px"); // .card h1 wins over h1
static_assert(ctcss::query(ctcss::parse_value(theme_css), hero_h1, "color") == "white");    // !important
static_assert(ctcss::query(ctcss::parse_value(theme_css), hero_h1, "box-sizing") == "border-box");

// typed values for the layout engine
static_assert([] {
	const ctcss::value_sheet theme = ctcss::parse_value(theme_css);
	const ctcss::length h1_size = ctcss::parse_length(ctcss::query(theme, hero_h1, "font-size"));
	const ctcss::color hero_bg = ctcss::parse_color(ctcss::query(theme, hero_h1 + 0, 2, "background"));
	return h1_size.ok && h1_size.value == 24 && h1_size.u == ctcss::unit::px &&
	       hero_bg.ok && hero_bg.r == 255 && hero_bg.g == 136 && hero_bg.b == 0;
}());

int main() {
	// the same calls at runtime - e.g. after a script added class=dark
	// to body, restyle the card:
	const ctcss::value_sheet theme = ctcss::parse_value(theme_css);
	const ctcss::element_ref dark_card[] = {
	    {"body", "", "dark"}, {"div", "", "card"}};
	std::cout << "card background (light): ";
	{
		const ctcss::element_ref card[] = {{"body"}, {"div", "", "card"}};
		std::cout << ctcss::query(theme, card, "background") << "\n";
	}
	std::cout << "card background (dark):  "
	          << ctcss::query(theme, dark_card, "background") << "\n";

	const ctcss::color rgb = ctcss::parse_color(ctcss::query(theme, dark_card, "background"));
	std::cout << "as rgb: " << int(rgb.r) << "," << int(rgb.g) << "," << int(rgb.b) << "\n";
}
