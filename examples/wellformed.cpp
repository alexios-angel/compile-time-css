// The value parser is LENIENT, the way browsers are: a malformed
// declaration or rule is skipped, everything else still applies. What
// used to be a compile-time validity bool is now a behavioral
// property you can prove: the good rules survive, the broken ones
// contribute nothing.
//
// Build: make wellformed

#include <ctcss.hpp>
#include <iostream>

constexpr ctcss::element_ref p_chain[] = {{"p", "", ""}};

// clean css parses and resolves
static_assert(ctcss::query(ctcss::parse_value("p { color: red }"), p_chain, "color") == "red");
static_assert(ctcss::query(ctcss::parse_value("h1, p, .title { font-weight: bold; }"),
                           p_chain, "font-weight") == "bold");
static_assert(ctcss::query(ctcss::parse_value("/* comments */ p { /* here too */ color: red }"),
                           p_chain, "color") == "red");

// browser-style recovery: the broken declaration drops, the good one applies
static_assert(ctcss::query(ctcss::parse_value("p { color red; margin: 4px }"),
                           p_chain, "margin") == "4px");
static_assert(ctcss::query(ctcss::parse_value("p { color red; margin: 4px }"),
                           p_chain, "color") == "");

// a rule that never closes takes what it can; later text is gone
static_assert(ctcss::query(ctcss::parse_value("p { color: red;"), p_chain, "color") == "red");

// applying @media flattens in; non-applying blocks are skipped
static_assert(ctcss::query(ctcss::parse_value("@media screen { p { color: red } }"),
                           p_chain, "color") == "red");
static_assert(ctcss::query(ctcss::parse_value("@media print { p { color: red } }"),
                           p_chain, "color") == "");

int main() {
	std::cout << "every claim in this file was proven during compilation\n";
}
