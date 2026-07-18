#ifndef CTCSS__GRAMMAR__HPP
#define CTCSS__GRAMMAR__HPP

#include "../ctlark.hpp"

// The grammar layer: the ctcss CSS subset written in lark's grammar
// language and parsed by ctlark. Two tricks carry it:
//
// * A COMPOUND selector ("div.note#top") is a single TOKEN. CSS's
//   descendant combinator is whitespace ("div .note" is not
//   "div.note"), which would die under %ignore whitespace - but with
//   compounds as tokens, adjacency of two COMPOUND tokens IS the
//   descendant combinator, and ">" between them is the child
//   combinator. The binder splits the token's text into tag/#id/.classes.
//
// * A declaration VALUE is a single raw token up to the ; or } - CSS
//   values are freeform enough (lengths, colors, font stacks, url())
//   that tokenizing them buys nothing at this level. The binder trims
//   whitespace and peels "!important"; typed value queries (lengths,
//   colors) parse the text on demand.
//
// The last declaration's semicolon is optional, comments are ignored,
// and at-rules (@media, @import) are NOT in v0.1 - see the README.

namespace ctcss::detail {

inline constexpr ctll::fixed_string css_grammar = R"x(
start: rule*

rule: selectors "{" body "}"
selectors: selector ("," selector)*
?selector: COMPOUND
         | selector COMPOUND     -> descendant
         | selector ">" COMPOUND -> child
body: (decl ";")* [decl]
decl: IDENT ":" VALUE

COMPOUND: /(\*|-?[A-Za-z][A-Za-z0-9_\-]*)([.#][A-Za-z0-9_\-]+)*|([.#][A-Za-z0-9_\-]+)+/
IDENT: /-?[A-Za-z][A-Za-z0-9\-]*/
VALUE: /[^;}]+/

%ignore /[ \x09\x0a\x0d]+/
%ignore /\/\*([^*]|\*+[^*\/])*\*+\//
)x";

inline constexpr ctll::fixed_string css_start = "start";

static_assert(ctlark::grammar_valid<css_grammar>,
              "ctcss: internal error - the CSS grammar failed to compile");

} // namespace ctcss::detail

#endif
