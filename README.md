> **Attribution:** this library is built on the CTLL compile-time LL(1)
> parser from [CTRE](https://github.com/hanickadot/compile-time-regular-expressions)
> by Hana Dusíková, via the [notre](https://github.com/alexios-angel/notre)
> fork, and follows the architecture of its siblings
> [compile-time-html](https://github.com/alexios-angel/compile-time-html),
> [compile-time-javascript](https://github.com/alexios-angel/compile-time-javascript) and
> [compile-time-json](https://github.com/alexios-angel/compile-time-json).
> Apache License 2.0 with LLVM Exceptions; see [NOTICE](NOTICE).

# ctcss — compile-time CSS

CSS parsed while your code compiles — and *resolved* there too.
`ctcss::parse_value(std::string_view)` is a constexpr *value* parser:
resolve the cascade (`!important`, specificity, source order) inside a
`static_assert`, or load a runtime stylesheet with the very same call —
which is what a browser needs the moment a script flips a class.
Malformed CSS is handled the way browsers handle it: the broken
declaration drops, everything else still applies.

```c++
#include <ctcss.hpp>

inline constexpr std::string_view theme = R"(
    p          { color: black; margin: 8px; }
    div.note p { color: red !important; }
    #main > ul { width: 640px; }
)";

// the element chain body > div.note > p, root-first
constexpr ctcss::element_ref chain[] = {{"body"}, {"div", "", "note wide"}, {"p"}};

static_assert(ctcss::query(ctcss::parse_value(theme), chain, "color") == "red");  // cascade, resolved
static_assert(ctcss::query(ctcss::parse_value(theme), chain, "margin") == "8px");
static_assert(ctcss::parse_length("8px").value == 8);           // typed values
static_assert(ctcss::parse_color("#f80").g == 136);
```

## What is supported (v0.1)

* **rules**:
  - selector lists (`h1, .title { ... }`) with declaration blocks
  - the final semicolon is optional
  - `/* comments */` anywhere

* **selectors**: type (`div`), universal (`*`), `.class`, `#id`,
  compounds (`div.note#top.wide`), descendant combinator
  (whitespace), child combinator (`>`) — in any combination

* **declarations**: any `property: value` pair — property names fold
  to lowercase (they match case-insensitively), values keep their raw
  text exactly (font stacks, `url(...)`, shorthand lists all pass
  through), `!important` is peeled off and flagged

* **matching**:
  - `ctcss::matches(selector, chain)` against `element_ref` chains (tag/id/space-separated classes — exactly what an HTML element knows)
  - tags compare case-insensitively, classes and ids exactly
  - descendant matching backtracks correctly

* **the cascade**:
  - `ctcss::query(sheet, chain, "property")` resolves `!important` → specificity (ids, classes, types) → source order, like a browser
  - `entries(sheet)` exposes the flattened (selector × declaration) view the cascade runs on

* **typed values**: `parse_length` (`px em rem % vw vh`, unitless
  zero) and `parse_color` (`#rgb #rgba #rrggbb #rrggbbaa`,
  `rgb()/rgba()`, the classic named colors, `transparent`) — all
  `constexpr`, ready for a layout engine

* **serialization**: `ctcss::serialize(sheet)` renders minified CSS
  into static storage

Not in v0.1 (all compile errors, documented): at-rules (`@media`,
`@import`, `@font-face`), pseudo-classes/elements (`:hover`,
`::before`), attribute selectors (`[href]`), sibling combinators
(`+`, `~`), CSS variables/`calc()` resolution, and semicolons/braces
inside quoted values.

## API

```c++
// THE parser - constexpr, lenient, never fails the build:
constexpr ctcss::value_sheet ctcss::parse_value(std::string_view css);
sheet.rule_count();
sheet.animation("name");            // @keyframes lookup
// @media blocks that apply flatten in; @font-face is captured

// resolution - match + specificity + !important + source order:
constexpr std::string_view ctcss::query(const value_sheet &,
    const element_ref * chain, std::size_t n, std::string_view property);
// (an element_ref[N] array overload deduces n)

// the browser seam - chains are ROOT-FIRST, self-last:
struct ctcss::element_ref { std::string_view tag, id, classes; };

// typed values, on demand:
ctcss::parse_length("12px");   // px/em/rem/%/vw/vh, unitless 0 -> {ok, value, u}
ctcss::parse_color("#f80");    // #rgb[a]/#rrggbb[aa], rgb()/rgba(), named -> {ok, r,g,b,a}
```

Constant evaluation note: an owned constexpr `value_sheet` cannot
outlive constant evaluation — parse inside the asserting expression, or
bind to a local in a constexpr lambda and return scalar facts.

## How it works

A hand-written recursive-descent tokenizer/parser builds an owned
`std::vector` sheet in `constexpr` (`value.hpp`), then matching runs
right-to-left with descendant backtracking and the cascade picks
`!important` > specificity (ids, classes, types) > source order — all
ordinary value loops, equally at home in a `static_assert` and in a
restyle after a script mutation. `parse_length`/`parse_color`
(`values.hpp`) cook raw declaration text into numbers on demand.

(The original implementation encoded stylesheets as C++ *types* via a
compile-time Earley grammar; it was retired in 2026-07 for the single
value parser — one code path, browser-lenient, seconds to compile.)

## Building and integrating

Header-only; C++20 or later (constexpr `std::vector`/`std::string`).

```bash
make                 # compiles the static_assert test suite
make CXX=clang++
cmake -B build && cmake --build build && ctest --test-dir build
```

Vendor `include/` (plus `include/ctll/utilities.hpp` from the
compile-time-lark submodule — the `CTLL_EXPORT` macro), or use the
amalgamated `single-header/ctcss.hpp`.

## Roadmap

ctcss is the third brick of a compile-time web stack, joining
[compile-time-html](https://github.com/alexios-angel/compile-time-html)
(the DOM as a type) and
[compile-time-javascript](https://github.com/alexios-angel/compile-time-javascript)
(scripts parsed at compile time, run at runtime). They meet in
**compile-time-browser**: HTML parsed into a DOM type, CSS resolved
against it at compile time, JS driving events at runtime — lowered
into an SDL3 application, as if the page had been hand-written as
native code. ctcss's `element_ref` chains are shaped exactly like
cthtml's elements (tag + id + class attribute), and its runtime-capable
`query()` is what restyles after a script mutation.

## License

Apache License 2.0 with LLVM Exceptions (see [LICENSE](LICENSE)).
The CTLL parser is Hana Dusíková's work, via notre; see
[NOTICE](NOTICE).
