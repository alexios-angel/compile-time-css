// The browser story in miniature: a stylesheet baked in at compile
// time, a DOM-shaped chain of elements, and style RESOLUTION - match,
// specificity, cascade - happening in static_asserts. The same
// query() calls work at runtime, which is what a browser needs the
// moment a script flips a class.
//
// Build: make theme

#include <ctcss.hpp>
#include <iostream>

constexpr auto theme = ctcss::parse<R"(
	*          { box-sizing: border-box; }
	body       { color: #222222; font-size: 16px; }
	h1         { font-size: 32px; margin: 8px 0; }
	.card      { padding: 12px; background: white; }
	.card h1   { font-size: 24px; }
	#hero      { background: #ff8800; }
	#hero > h1 { color: white !important; }
	.dark .card { background: #333333; color: #eeeeee; }
)">();

// the page:  body > div#hero.card > h1
constexpr ctcss::element_ref hero_h1[] = {
    {"body"}, {"div", "hero", "card"}, {"h1"}};

// resolution, proven during compilation
static_assert(ctcss::query(theme, hero_h1, "font-size") == "24px");  // .card h1 wins over h1
static_assert(ctcss::query(theme, hero_h1, "color") == "white");     // !important
static_assert(ctcss::query(theme, hero_h1, "box-sizing") == "border-box");

// typed values for the layout engine
constexpr auto h1_size = ctcss::parse_length(ctcss::query(theme, hero_h1, "font-size"));
static_assert(h1_size.ok && h1_size.value == 24 && h1_size.u == ctcss::unit::px);
constexpr auto hero_bg =
    ctcss::parse_color(ctcss::query(theme, hero_h1 + 0, 2, "background"));
static_assert(hero_bg.ok && hero_bg.r == 255 && hero_bg.g == 136 && hero_bg.b == 0);

int main() {
	// the same calls at runtime - e.g. after a script added class=dark
	// to body, restyle the card:
	const ctcss::element_ref dark_card[] = {
	    {"body", "", "dark"}, {"div", "", "card"}};
	std::cout << "card background (light): ";
	{
		const ctcss::element_ref card[] = {{"body"}, {"div", "", "card"}};
		std::cout << ctcss::query(theme, card, "background") << "\n";
	}
	std::cout << "card background (dark):  "
	          << ctcss::query(theme, dark_card, "background") << "\n";

	const auto rgb = ctcss::parse_color(ctcss::query(theme, dark_card, "background"));
	std::cout << "as rgb: " << int(rgb.r) << "," << int(rgb.g) << "," << int(rgb.b) << "\n";

	std::cout << "minified: " << ctcss::serialize(theme).substr(0, 60) << "...\n";
}
