# CLAUDE.md — compile-time-css (ctcss)

Header-only, constexpr CSS VALUE parser AND resolver. One entry point:
`ctcss::parse_value(std::string_view) -> ctcss::value_sheet` — folds in
a `static_assert` and parses runtime strings with the same call.
Selector matching and the cascade (`!important` > specificity > source
order) are value loops (`query(sheet, chain, n, prop)`), equally at
home in constant evaluation and in a restyle after a script mutation.
LENIENT like a browser: broken declarations drop, the rest applies —
there is no validity bool. Namespace `ctcss`. C++20+. Work on `main`.

(History: the type-level parser — a ctlark Earley grammar producing the
stylesheet as a TYPE, with is_valid<>/parse<>/error_message<> — was
removed 2026-07; the value parser is the only path.)

## Build & test — "compiling the tests IS the test"
`tests/value.cpp` is a `static_assert` suite; compiling it = passing.
```bash
make                # C++20+, seconds (no grammar bake exists anymore)
make CXX=clang++
cmake -B build && cmake --build build && ctest --test-dir build
```
Flags: `-O2 -pedantic -Wall -Wextra -Werror -Wconversion` — stay clean.

## Layout
- `include/ctcss.hpp` — umbrella (types, values, match, value).
- `include/ctcss/value.hpp` — THE parser + resolver: `parse_value`,
  `value_sheet` (incl. lenient `@media` flattening, `@keyframes` /
  `@font-face` capture, `animation()`), `query`, the value matchers.
- `include/ctcss/values.hpp` — `parse_length` (px/em/rem/%/vw/vh,
  unitless 0), `parse_color` (#rgb[a]/#rrggbb[aa], rgb()/rgba(),
  named, transparent), all constexpr.
- `include/ctcss/match.hpp` — the browser seam: `element_ref{tag, id,
  classes}` (classes space-separated) + `has_class`.
- `include/ctcss/types.hpp` — shared helpers (`ascii_lower`,
  `is_css_blank`, `ascii_iequals`) and the `rel` enum.
- `external/compile-time-lark/` — git SUBMODULE; only
  `ctll/utilities.hpp` (`CTLL_EXPORT`) is consumed now.
- `tests/value.cpp`, `examples/` (theme, wellformed — value API),
  `single-header/ctcss.hpp` (quom), `ctcss.cppm`.

## Semantics decisions (keep consistent; README documents all)
- Chains are ROOT-FIRST, self-last. Tags match case-insensitively;
  classes/ids exact. The same element cannot satisfy two steps.
- Values stay RAW text; typed parsing on demand. Property names fold
  to lowercase, lookups fold too.
- Cascade in `query`: important > specificity > source order; order =
  document position of the declaration.
- Leniency contract: a malformed declaration or rule contributes
  nothing; parsing never fails. Non-applying `@media` blocks skip.

## Gotchas
- **Constexpr lifetime idioms**: an owned constexpr `value_sheet`
  cannot escape constant evaluation — parse inside the asserting
  expression or bind to a named local in a constexpr lambda (gcc needs
  the named binding; see examples/theme.cpp).
- **ctll is a git SUBMODULE**, never edit here; only utilities.hpp is
  used. Bump = checkout in submodule + commit gitlink.
- **single-header** — `make single-header` (needs `quom`).
- **Attribution** — CTLL is Hana Dusíková's (via notre, from CTRE); CSS
  semantics follow the W3C specs. Preserve `NOTICE`/`LICENSE`.
