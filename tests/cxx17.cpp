#include <cthtml.hpp>
#include <string_view>

// C++17: inputs and keys are fixed_string variables with linkage
static constexpr auto doc_text =
    ctll::fixed_string{"<ul id=nav><li>Docs<li>Code</ul>"};
static constexpr auto bad_text = ctll::fixed_string{"<b><i>x</b></i>"};
static constexpr ctll::fixed_string body_tag = "body";
static constexpr ctll::fixed_string ul_tag = "ul";
static constexpr ctll::fixed_string li_tag = "li";
static constexpr ctll::fixed_string id_key = "id";

static_assert(cthtml::is_valid<doc_text>);
static_assert(!cthtml::is_valid<bad_text>);

constexpr auto doc = cthtml::parse<doc_text>();
constexpr auto list = doc.template get<body_tag>().template get<ul_tag>();
static_assert(list.template attribute<id_key>() == std::string_view{"nav"});
static_assert(list.template count<li_tag>() == 2);
static_assert(list.template child<0>().text() == std::string_view{"Docs"});

void empty_symbol() { }

// operator[] needs no C++20: ordinary strings and integer indexes work as-is

static_assert(doc[1].name() == std::string_view{"body"});
static_assert(doc["body"]["ul"]["li"].text() == std::string_view{"Docs"});
static_assert(doc["BODY"]["UL"].attribute("ID") == std::string_view{"nav"});

// iteration: uniform views, range-for, constexpr
static_assert([] {
	size_t n = 0;
	for (const auto & node : list) {
		n += node.name().size() + node.text().size();
	}
	return n;
}() == (2 + 4) + (2 + 4));
static_assert(cthtml::attributes(list).size() == 1);

// diagnostics through the variable-form API
static_assert(cthtml::error_info<doc_text>().ok());
static_assert(cthtml::error_info<bad_text>().ok()); // it PARSES; tree construction rejects it
constexpr auto crossing = cthtml::bind_error<bad_text>();
static_assert(crossing.reason == cthtml::bind_reason::mismatched_tag);
static_assert(crossing.where == std::string_view{"</b>"});
static constexpr auto unclosed_text = ctll::fixed_string{"<p class=x"};
constexpr auto unclosed_info = cthtml::error_info<unclosed_text>();
static_assert(unclosed_info.kind != ctlark::error_kind::none);
static_assert(unclosed_info.position == 10);
static_assert(!cthtml::error_message<unclosed_text>().empty());
constexpr auto traced = cthtml::debug::traced_parse<bad_text>();
static_assert(traced.ok && traced.log.events > 0);
static_assert(!cthtml::debug::dump_tokens<doc_text>().empty());
