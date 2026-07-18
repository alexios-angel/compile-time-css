// Validity as a compile-time property: is_valid answers as a bool
// without failing the build, so shipping broken CSS becomes
// impossible - the checks below run in the compiler, not in
// production.
//
// Build: make wellformed

#include <ctcss.hpp>
#include <iostream>

// what parses
static_assert(ctcss::is_valid<"p { color: red }">);
static_assert(ctcss::is_valid<"div.note#top > ul li { margin: 0 4px; }">);
static_assert(ctcss::is_valid<"h1, h2, .title { font-weight: bold; }">);
static_assert(ctcss::is_valid<"p { font: 12px/1.4 'Fira Sans', sans-serif; }">);
static_assert(ctcss::is_valid<"p { color: red !important; }">);
static_assert(ctcss::is_valid<"/* comments are fine anywhere */ p { /* here too */ }">);

// what does not (each is one typo away from the lines above)
static_assert(!ctcss::is_valid<"p { color red }">);   // lost the colon
static_assert(!ctcss::is_valid<"p { color: red;">);   // lost the brace
static_assert(!ctcss::is_valid<"color: red;">);       // lost the selector
static_assert(!ctcss::is_valid<"@import url(x.css);">); // at-rules: not in v0.1

// and the diagnostic is one query away, at compile time
static_assert(!ctcss::error_message<"p { color: red;">().empty());

int main() {
	std::cout << "every claim in this file was proven during compilation\n";
}
