# CLAUDE.md — compile-time-css (ctcss)

Header-only, compile-time (constexpr) CSS parser AND resolver. A
stylesheet is a *type*: `ctcss::parse<...>()`, every accessor
constexpr, malformed CSS a compile error (or `false` from `is_valid`).
Selector matching and the cascade (`!important` → specificity → source
order) are VALUE computations over flattened static views, so
`matches()`/`query()` run in static_asserts against a compile-time DOM
chain and equally at runtime (a browser restyling after a script
mutation). Namespace `ctcss`. Work on `main`. Prefer `rg` over `grep`.

## Build & test — "compiling the tests IS the test"
Tests under `tests/*.cpp` are `static_assert` suites; each compiles to a `.o`.
```bash
make                                   # C++20 (default), one .o per test
make CXX=clang++                       # clang
make CXX=clang++ CXX_STANDARD=17       # C++17 path (variable-form API)
make clean
cmake -B build && cmake --build build && ctest --test-dir build
```
Flags are `-O2 -pedantic -Wall -Wextra -Werror -Wconversion` — keep every
change warning-clean. The PCH (`make pch`, automatic) bakes the grammar
tables once — the CSS grammar is SMALL, so this is quick (unlike ctjs).

## Layout
- `include/ctcss.hpp` — umbrella; public API (`is_valid`, `parse`, `error_info/message`, `debug::*`).
- `include/ctcss/grammar.hpp` — the CSS subset as a **lark grammar string**. TWO load-bearing tricks: a COMPOUND selector ("div.note#top") is ONE token (whitespace adjacency of compounds IS the descendant combinator — that's how it survives %ignore; ">" is child), and a declaration VALUE is one raw token up to `;`/`}`.
- `include/ctcss/bind.hpp` — lowers the tree: splits COMPOUND text into typed tag/#id/.classes (tags fold to lowercase; a later #id wins), trims values, peels `!important` (also "! important", any case).
- `include/ctcss/types.hpp` — `text`, `compound`, `sel_step<C, rel>` (rel = how the step attaches to the one on its LEFT), `selector` (packed specificity = ids*10000 + classes*100 + types), `declaration`, `rule` (with case-insensitive `property<"key">()`), `stylesheet`, `for_each_rule`.
- `include/ctcss/match.hpp` — the browser seam: `element_ref{tag,id,classes}` (classes space-separated), per-selector/per-sheet STATIC VIEWS (`selector_data`, `sheet_data`: one `decl_entry` per selector × declaration), `matches()` (right-to-left with descendant backtracking), `query()` (the cascade), `entries()`.
- `include/ctcss/values.hpp` — `parse_length` (px/em/rem/%/vw/vh, unitless 0), `parse_color` (#rgb[a]/#rrggbb[aa], rgb()/rgba() with clamping, named colors, transparent), all constexpr.
- `include/ctcss/serialize.hpp` — minified CSS from the sheet type.
- `external/compile-time-lark/` — git SUBMODULE (ctlark + ctll). Never edit here.
- `tests/` (`document.cpp` — a real sheet end-to-end, `css.cpp` — the feature matrix, `cxx17.cpp`), `examples/` (`theme`, `wellformed`).

## Semantics decisions (keep consistent; README documents all)
- Chains are ROOT-FIRST, self-last. Tags match case-insensitively
  (bind folds selector tags to lowercase; cthtml tags are lowercase);
  classes/ids exact. The same element cannot satisfy two steps.
- v0.1 rejects (compile errors): at-rules, pseudo-classes/elements,
  attribute selectors, `+`/`~` combinators; `;`/`}` cannot appear
  inside quoted values (the VALUE token stops there).
- Values stay RAW text; typed parsing (`parse_length`/`parse_color`)
  happens on demand. Property names fold to lowercase, lookups fold too.
- Cascade in `query`: important > specificity > source order; order =
  document position of the declaration (selector-list twins share it).

## GOTCHAS
- **ctlark and ctll are a git SUBMODULE**: `git submodule update
  --init` once; bump = checkout in submodule + commit gitlink; build
  adds `<sub>/include` + `/ctlark` + `/ctll` to -I (quoted-include
  fallback). CMake install flattens to include/{ctcss,ctlark,ctll}.
- **single-header** — `make single-header` (needs `quom`); prepends LICENSE.
- **Attribution** — CTLL is Hana Dusíková's (via `notre`, from CTRE);
  the Lark grammar language is the lark-parser project's; CSS
  semantics follow the W3C specs. Preserve `NOTICE` and `LICENSE`
  (Apache-2.0 w/ LLVM Exceptions).
